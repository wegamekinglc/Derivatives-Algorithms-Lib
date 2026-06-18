//
// Created by wegam on 2026/5/9.
//

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>
#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/curve/calibration.hpp>
#include <dal/curve/curveblock.hpp>
#include <dal/curve/piecewiseconstant.hpp>
#include <dal/curve/piecewiselinear.hpp>
#include <dal/curve/ycconst.hpp>
#include <dal/curve/ycctx.hpp>
#include <dal/curve/ycimp.hpp>
#include <dal/curve/yclogdf.hpp>
#include <dal/math/aad/aad.hpp>
#include <dal/math/matrix/banded.hpp>
#include <dal/math/matrix/matrixarithmetic.hpp>
#include <dal/math/matrix/squarematrix.hpp>
#include <dal/math/optimization/underdetermined.hpp>
#include <dal/math/optimization/underdeterminedutils.hpp>
#include <dal/time/datetime.hpp>
#include <dal/utilities/dictionary.hpp>
#include <dal/utilities/functionals.hpp>
#include <dal/utilities/numerics.hpp>

namespace Dal {
#include <dal/auto/MG_CurveSolveMode_enum.inc>
#include <dal/auto/MG_CurveParameterization_enum.inc>
#include <dal/auto/MG_CurveKnotPolicy_enum.inc>
#include <dal/auto/MG_CurveJacobianMode_enum.inc>
#include <dal/auto/MG_LogDfScheme_enum.inc>

    // Stores the LOG_DISCOUNT calibration Jacobian dense and assembles sparse. The method
    // bodies mirror XJDense_ (dal-cpp/dal/math/optimization/underdetermined.cpp) -- the storage
    // is dense regardless of how the matrix is filled, so the sparse-vs-banded optimization is
    // a follow-up once the assembly is validated against central differences. Declared in Dal::
    // (not anonymous) so the TestOnly helper can dynamic_cast to extract entries for unit tests.
    struct XCurveJacobian_ : Underdetermined::Jacobian_ {
        Matrix_<> j_;
        explicit XCurveJacobian_(Matrix_<>&& j) : j_(std::move(j)) {}

        [[nodiscard]] int Rows() const override { return j_.Rows(); }
        [[nodiscard]] int Columns() const override { return j_.Cols(); }

        void DivideRows(const Vector_<>& tol) override {
            for (int ii = 0; ii < j_.Rows(); ++ii) {
                auto row = j_.Row(ii);
                Transform(&row, [&tol, &ii](double x) { return 1.0 / tol[ii] * x; });
            }
        }

        [[nodiscard]] Vector_<> MultiplyRight(const Vector_<>& t) const override {
            Vector_<> retval;
            Matrix::Multiply(t, j_, &retval);
            return retval;
        }
        [[nodiscard]] Vector_<> MultiplyLeft(const Vector_<>& dx) const override {
            Vector_<> retval;
            Matrix::Multiply(j_, dx, &retval);
            return retval;
        }

        void QForm(const Sparse::SymmetricDecomposition_& w, SquareMatrix_<>* form) const override {
            w.QForm(j_, form);
        }

        void SecantUpdate(const Vector_<>& dx, const Vector_<>& df) override {
            const auto nf = df.size();
            const double x2 = InnerProduct(dx, dx);
            for (int ii = 0; ii < nf; ++ii) {
                auto row = j_.Row(ii);
                const double excess = df[ii] - InnerProduct(dx, row);
                Transform(&row, dx, LinearIncrement(excess / x2));
            }
        }
    };

    namespace {
        constexpr int MAX_RELEVANT_DATES_PER_INSTRUMENT = 2;
        // Flat-rate seed for LOG_DISCOUNT calibration (R4: scalar 0.05 is wrong-sign on log(DF)).
        // log(DF)(node_i) = -FLAT_SEED_RATE * yf_365F(anchor, node_i).
        constexpr double FLAT_SEED_RATE = 0.02;

        constexpr const char* KEY_MAX_EVALUATIONS = "MAXEVALUATIONS";
        constexpr const char* KEY_MAX_RESTARTS = "MAXRESTARTS";
        void LoadDiscountCurves(const MultiCurveCalibrationResult_& source, CurveCalibrationSpec_* stageSpec) {
            for (const auto& [collateral, curve] : source.discountCurves_)
                if (!stageSpec->discountCurves_.count(collateral))
                    stageSpec->discountCurves_[collateral] = curve;
        }

        void LoadForwardCurves(const MultiCurveCalibrationResult_& source, CurveCalibrationSpec_* stageSpec) {
            for (const auto& [tenor, curve] : source.forwardCurves_)
                if (!stageSpec->forwardCurves_.count(tenor))
                    stageSpec->forwardCurves_[tenor] = curve;
        }

        void ApplyStageDefaults(const MultiCurveCalibrationSpec_& spec,
                                const MultiCurveCalibrationResult_& currentResult,
                                CurveCalibrationSpec_* stageSpec) {
            if (stageSpec->ccy_.empty())
                stageSpec->ccy_ = spec.ccy_;
            if (stageSpec->curveName_.empty())
                stageSpec->curveName_ = spec.name_;
            stageSpec->liborBasis_ = spec.liborBasis_;
            LoadDiscountCurves(currentResult, stageSpec);
            LoadForwardCurves(currentResult, stageSpec);
            if (!stageSpec->calibrateDiscountCurve_ && stageSpec->baseCurve_.IsEmpty()) {
                REQUIRE(stageSpec->discountCurves_.count(stageSpec->targetCollateral_),
                        "Forward-curve calibration requires a preloaded discount curve for the requested collateral");
                stageSpec->baseCurve_ = stageSpec->discountCurves_.at(stageSpec->targetCollateral_);
            }
        }

        void
        StoreStageResult(const CurveCalibrationSpec_& stageSpec, CurveCalibrationResult_* stageResult, MultiCurveCalibrationResult_* multiResult) {
            Handle_<DiscountCurve_> calibrated(stageResult->curve_.release());
            if (stageSpec.calibrateDiscountCurve_)
                multiResult->discountCurves_[stageSpec.targetCollateral_] = calibrated;
            else
                multiResult->forwardCurves_[stageSpec.targetTenor_] = calibrated;
            multiResult->diagnostics_.push_back(stageResult->diagnostics_);
        }

        const char* ParameterizationName(CurveParameterization_ parameterization) {
            switch (parameterization.Switch()) {
            case CurveParameterization_::Value_::PIECEWISE_LINEAR_FWD:
                return "piecewise linear forward";
            case CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD:
                return "piecewise constant forward";
            case CurveParameterization_::Value_::ZERO_RATE:
                return "zero rate";
            case CurveParameterization_::Value_::LOG_DISCOUNT:
                return "log discount";
            default:
                return "unknown";
            }
        }

        Vector_<Handle_<YCInstrument_>> OrderInstruments(const Vector_<Handle_<YCInstrument_>>& instruments) {
            auto ordered = instruments;
            std::sort(ordered.begin(), ordered.end(), [](const Handle_<YCInstrument_>& lhs, const Handle_<YCInstrument_>& rhs) {
                const auto lhsSpan = lhs->TimeSpan();
                const auto rhsSpan = rhs->TimeSpan();
                if (lhsSpan.second != rhsSpan.second)
                    return lhsSpan.second < rhsSpan.second;
                if (lhsSpan.first != rhsSpan.first)
                    return lhsSpan.first < rhsSpan.first;
                return lhs->Name() < rhs->Name();
            });
            return ordered;
        }

        Vector_<Date_> UniqueSortedDates(const Vector_<Date_>& dates) {
            auto sorted = dates;
            std::sort(sorted.begin(), sorted.end());
            sorted.erase(std::unique(sorted.begin(), sorted.end()), sorted.end());
            return sorted;
        }

        Vector_<Date_> InstrumentDates(const Date_& today, const Vector_<Handle_<YCInstrument_>>& instruments) {
            Vector_<Date_> retval;
            retval.reserve(instruments.size() * MAX_RELEVANT_DATES_PER_INSTRUMENT);
            for (const auto& inst : instruments) {
                const auto span = inst->TimeSpan();
                if (span.first > today)
                    retval.push_back(span.first);
                if (span.second > today)
                    retval.push_back(span.second);
            }
            return UniqueSortedDates(retval);
        }

        int ParamsPerKnot(CurveParameterization_ parameterization) {
            switch (parameterization.Switch()) {
            case CurveParameterization_::Value_::PIECEWISE_LINEAR_FWD:
                return 2;
            case CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD:
                return 1;
            case CurveParameterization_::Value_::LOG_DISCOUNT:
                return 1;
            case CurveParameterization_::Value_::ZERO_RATE:
                REQUIRE(false, "Requested curve parameterization is not implemented");
                return 0;
            default:
                REQUIRE(false, "Unknown curve parameterization");
                return 0;
            }
        }

        std::unique_ptr<DiscountCurve_> BuildDiscountCurve(const String_& name,
                                                           const String_& ccy,
                                                           CurveParameterization_ parameterization,
                                                           LogDfScheme_ logDfScheme,
                                                           const Vector_<Date_>& knotDates,
                                                           const Vector_<>& x,
                                                           const DayBasis_& dayCount,
                                                           const Handle_<DiscountCurve_>& baseCurve) {
            switch (parameterization.Switch()) {
            case CurveParameterization_::Value_::PIECEWISE_LINEAR_FWD: {
                Vector_<> fLeft(knotDates.size());
                Vector_<> fRight(knotDates.size());
                for (int i = 0; i < static_cast<int>(knotDates.size()); ++i) {
                    fLeft[i] = x[2 * i];
                    fRight[i] = x[2 * i + 1];
                }
                return std::unique_ptr<DiscountCurve_>(NewDiscountPWLF(name, ccy, PiecewiseLinear_(knotDates, fLeft, fRight), baseCurve));
            }
            case CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD:
                return std::unique_ptr<DiscountCurve_>(NewDiscountPWC(name, ccy, PiecewiseConstant_(knotDates, x), baseCurve));
            case CurveParameterization_::Value_::LOG_DISCOUNT: {
                // x is the free-node parameter vector (anchor excluded); prepend log(DF)=0 for the anchor.
                REQUIRE(static_cast<int>(x.size()) + 1 == static_cast<int>(knotDates.size()),
                        "LOG_DISCOUNT parameter vector must have length (knotDates - 1)");
                Vector_<> logDF(knotDates.size());
                logDF[0] = 0.0;
                std::copy(x.begin(), x.end(), logDF.begin() + 1);
                return std::unique_ptr<DiscountCurve_>(NewDiscountLogDF(name, ccy, knotDates, logDF, dayCount, logDfScheme, baseCurve));
            }
            case CurveParameterization_::Value_::ZERO_RATE:
                REQUIRE(false,
                        String_("Requested curve parameterization is reserved for future implementation: ") + ParameterizationName(parameterization));
                return nullptr;
            default:
                REQUIRE(false, "Unknown curve parameterization");
                return nullptr;
            }
        }

        // Phase A templated curve builder. Handles ONLY LOG_DISCOUNT -- the only parameterization
        // the AAD path supports. The other parameterizations REQUIRE(false) in the Number_
        // instantiation; this is unreachable because EligibleForAnalyticJacobian rejects non-LOG_DISCOUNT
        // before constructing any templated object.
        template <class T_>
        std::unique_ptr<Tape::DiscountCurve_<T_>> BuildDiscountCurveT(const String_& name,
                                                                 const String_& ccy,
                                                                 CurveParameterization_ parameterization,
                                                                 LogDfScheme_ logDfScheme,
                                                                 const Vector_<Date_>& knotDates,
                                                                 const Vector_<T_>& logDF,
                                                                 const DayBasis_& dayCount,
                                                                 const Handle_<DiscountCurve_>& baseCurve) {
            // baseCurve is a Handle_<DiscountCurve_> (double) -- the templated curve treats it as
            // a constant multiplier. The Number_-typed Tape::DiscountLogDF_ ctor accepts this via the
            // CurveWithBase_<Tape::DiscountCurve_<T_>> base, but the base's operator() reads double DFs
            // (constant from the tape's perspective).
            REQUIRE(parameterization == CurveParameterization_::Value_::LOG_DISCOUNT,
                    "Phase A BuildDiscountCurveT: only LOG_DISCOUNT parameterization is supported");
            return std::unique_ptr<Tape::DiscountCurve_<T_>>(
                new Tape::DiscountLogDF_<T_>(name, ccy, knotDates, logDF, dayCount, logDfScheme, baseCurve));
        }

        // RAII tape scope: Clear on entry, Clear on exit (even on throw). AnalyticJacobian owns
        // exactly one AAD cycle per solver restart -- the recording, sweep, and harvest all live
        // inside this scope. NewRecording is opened by AnalyticJacobian AFTER RegisterIndependent,
        // not here: XAD requires inputs registered before the recording window opens (its canonical
        // idiom), and opening the recording before registration silently drops the inputs and yields
        // an all-zero Jacobian. The tape must be clean before the next Gradient call (or the next
        // F call which, while it does not touch the tape, would inherit any state if a future change
        // ever reads the tape from F). Single-threaded today; thread-local Tape_ isolates
        // per-worker state if calibration ever becomes parallel, but the residual loop itself must
        // stay serial until a parallel-residual design exists.
        struct TapeGuard_ {
            Dal::AAD::Tape_* t_;
            explicit TapeGuard_(Dal::AAD::Tape_* t) : t_(t) { Dal::AAD::Clear(*t_); }
            ~TapeGuard_() {
                try {
                    Dal::AAD::Clear(*t_);
                } catch (...) {
                    // swallow; we are unwinding
                }
            }
            TapeGuard_(const TapeGuard_&) = delete;
            TapeGuard_& operator=(const TapeGuard_&) = delete;
        };

        class YieldCurveCalibrationFunc_ : public Underdetermined::Function_ {
            // Cached eligibility verdict for the AAD analytic Jacobian. EligibleForAnalyticJacobian()
            // is expensive (walks every instrument) and emits NOTICEs on fall-through, and Gradient is
            // called per solver iteration (up to maxEvaluations_ * maxRestarts_). Evaluating the
            // predicate once and caching the verdict bounds every NOTICE to at most one per
            // CalibrateYieldCurve call (H1 NOTICE-frequency contract).
            enum class Eligibility_ { Unknown, Eligible, Ineligible };

            String_ ccy_;
            String_ curveName_;
            CurveParameterization_ parameterization_;
            Vector_<Handle_<YCInstrument_>> instruments_;
            Vector_<Handle_<YCInstrument_::Rate_>> rates_;
            Vector_<> marketRates_;
            Vector_<Date_> knotDates_;
            std::map<CollateralType_, Handle_<DiscountCurve_>> discountCurves_;
            std::map<PeriodLength_, Handle_<DiscountCurve_>> forwardCurves_;
            Handle_<DiscountCurve_> baseCurve_;
            CollateralType_ targetCollateral_;
            PeriodLength_ targetTenor_;
            bool calibrateDiscountCurve_;
            DayBasis_ liborBasis_;
            LogDfScheme_ logDfScheme_;
            CurveJacobianMode_ jacobianMode_;
            mutable Eligibility_ cachedEligibility_ = Eligibility_::Unknown;

        public:
            YieldCurveCalibrationFunc_(const String_& ccy,
                                       const String_& curveName,
                                       CurveParameterization_ parameterization,
                                       const Vector_<Handle_<YCInstrument_>>& instruments,
                                       const Vector_<Date_>& knotDates,
                                       const std::map<CollateralType_, Handle_<DiscountCurve_>>& discountCurves,
                                       const std::map<PeriodLength_, Handle_<DiscountCurve_>>& forwardCurves,
                                       const Handle_<DiscountCurve_>& baseCurve,
                                       const CollateralType_& targetCollateral,
                                       const PeriodLength_& targetTenor,
                                       bool calibrateDiscountCurve,
                                       const DayBasis_& liborBasis,
                                       LogDfScheme_ logDfScheme,
                                       CurveJacobianMode_ jacobianMode)
                : ccy_(ccy), curveName_(curveName), parameterization_(parameterization), instruments_(instruments), knotDates_(knotDates),
                  discountCurves_(discountCurves), forwardCurves_(forwardCurves), baseCurve_(baseCurve), targetCollateral_(targetCollateral),
                  targetTenor_(targetTenor), calibrateDiscountCurve_(calibrateDiscountCurve), liborBasis_(liborBasis), logDfScheme_(logDfScheme),
                  jacobianMode_(jacobianMode) {
                Handle_<YieldCurve_> fundingYC;
                if (!discountCurves_.empty())
                    fundingYC.reset(new CurveBlock_(curveName_, ccy_, discountCurves_, forwardCurves_, liborBasis_));
                rates_.reserve(instruments_.size());
                marketRates_.reserve(instruments_.size());
                for (const auto& inst : instruments_) {
                    rates_.push_back(inst->Precompute(fundingYC));
                    marketRates_.push_back(inst->MarketRate());
                }
            }

            [[nodiscard]] Vector_<> F(const Vector_<>& x) const override {
                Handle_<DiscountCurve_> dc(
                    BuildDiscountCurve(curveName_, ccy_, parameterization_, logDfScheme_, knotDates_, x, liborBasis_, baseCurve_).release());
                CurveBlock_ yc = YieldCurveWith(dc);

                Vector_<> result(instruments_.size());
                for (int i = 0; i < static_cast<int>(instruments_.size()); ++i)
                    result[i] = (*rates_[i])(yc) - marketRates_[i];
                return result;
            }

            // Slot a calibrated discount curve into the discount or forward position (per
            // calibrateDiscountCurve_) and return the yield-curve context the rates read from.
            CurveBlock_ YieldCurveWith(const Handle_<DiscountCurve_>& dc) const {
                auto discountCurves = discountCurves_;
                auto forwardCurves = forwardCurves_;
                if (calibrateDiscountCurve_)
                    discountCurves[targetCollateral_] = dc;
                else
                    forwardCurves[targetTenor_] = dc;
                return CurveBlock_(curveName_, ccy_, discountCurves, forwardCurves, liborBasis_);
            }

            // Sparse Jacobian (LOG_DISCOUNT free-node log-DF unknowns). Returns the reverse-mode AAD
            // Jacobian (single-result sweep on the active Number_ tape) when the mode is ANALYTIC and
            // EligibleForAnalyticJacobian() holds; otherwise returns nullptr so the solver dense-bumps.
            // The residual is modelRate - marketRate, so dResidual_i/dx_j = dModelRate_i/dx_j
            // (marketRate is constant, contributes nothing). Backend-neutral: the same path runs under
            // native, XAD, CoDiPack, and Adept via the Dal::AAD facade (RegisterIndependent,
            // ZeroAdjoints, Adjoint, PropagateToStart).
            [[nodiscard]] Underdetermined::Jacobian_* Gradient(const Vector_<>& x, const Vector_<>& f) const override {
                // Engage the analytic Jacobian IFF the mode is explicitly ANALYTIC. Both BUMPED and
                // _NOT_SET (default-constructed / uninitialized) route to nullptr so the solver
                // dense-bumps -- this matches the contract "analytic iff mode == ANALYTIC && eligible"
                // and keeps a stray _NOT_SET off the analytic path.
                if (jacobianMode_ != CurveJacobianMode_::Value_::ANALYTIC) {
                    static_cast<void>(x);
                    static_cast<void>(f);
                    return nullptr;
                }
                // ANALYTIC: engage the AAD Jacobian iff the cached eligibility verdict is Eligible.
                // The verdict is evaluated once (EvaluateEligibilityOnce) and cached, so the NOTICEs
                // inside EligibleForAnalyticJacobian fire at most once per CalibrateYieldCurve call.
                EvaluateEligibilityOnce();
                if (cachedEligibility_ == Eligibility_::Eligible)
                    return AnalyticJacobian(x, f);
                // Ineligible: NOTICE was emitted once inside EvaluateEligibilityOnce; return nullptr so
                // the solver dense-bumps. ANALYTIC never throws -- it is a best-effort hint.
                static_cast<void>(x);
                static_cast<void>(f);
                return nullptr;
            }

            // Evaluate EligibleForAnalyticJacobian() exactly once per func lifetime and cache the
            // verdict. Gradient is called per solver iteration; without this guard the predicate (and
            // its NOTICEs) would re-fire thousands of times per CalibrateYieldCurve call. The NOTICEs
            // naming the offending condition live inside EligibleForAnalyticJacobian and fire on the
            // single uncached evaluation.
            void EvaluateEligibilityOnce() const {
                if (cachedEligibility_ != Eligibility_::Unknown)
                    return;
                cachedEligibility_ = EligibleForAnalyticJacobian() ? Eligibility_::Eligible : Eligibility_::Ineligible;
            }

            // Phase A eligibility predicate (per .claude/designs/aad-analytic-jacobian-selector-api.md
            // §4.2). Returns true iff every Phase A constraint is satisfied. The predicate is a
            // pure query over ctor-stored state and NEVER throws -- ineligibility routes through
            // a NOTICE + return nullptr (solver dense-bumps) instead of failing. Each fall-through
            // path emits a NOTICE that names the offending condition so the user can see why the
            // AAD Jacobian did not engage.
            [[nodiscard]] bool EligibleForAnalyticJacobian() const {
                if (parameterization_ != CurveParameterization_::Value_::LOG_DISCOUNT) {
                    const String_ msg = String_("AAD Jacobian requires CurveParameterization_::LOG_DISCOUNT, got ")
                                        + parameterization_.String() + "; falling back to bumped";
                    NOTICE(msg);
                    return false;
                }
                if (!calibrateDiscountCurve_) {
                    NOTICE("AAD Jacobian requires DISCOUNT-target calibration "
                           "(calibrateDiscountCurve_ == true); falling back to bumped");
                    return false;
                }
                // Every instrument must (a) be a type Phase A has a templated rate for, and
                // (b) not use a projection curve (forecast == discount). The tradeDate == anchor
                // check is folded in: a Swap_ with tradeDate != knotDates_.front() is structurally
                // fine for Phase A (the templated Tape::SwapRate_ reads DF(tradeDate_, p) directly), but
                // requires anchor alignment so every instrument starts at knotDates_.front().
                for (int i = 0; i < static_cast<int>(instruments_.size()); ++i)
                    if (!InstrumentEligibleForAnalyticJacobian(instruments_[i].get()))
                        return false;
                return true;
            }

            // Float-index convention of a Phase-A-supported instrument (Deposit/FRA/Future/vanilla
            // Swap, in that priority order), or nullptr if the instrument type has no Phase A
            // templated rate. Mirrors the dispatch in PhaseARateAt so eligibility and pricing agree.
            [[nodiscard]] static const RateIndexConvention_* PhaseAFloatConvention(const YCInstrument_* inst) {
                if (const auto* deposit = dynamic_cast<const Deposit_*>(inst))
                    return &deposit->FloatConvention();
                if (const auto* fra = dynamic_cast<const FRA_*>(inst))
                    return &fra->FloatConvention();
                if (const auto* future = dynamic_cast<const Future_*>(inst))
                    return &future->FloatConvention();
                if (const auto* swap = dynamic_cast<const Swap_*>(inst))
                    return &swap->FloatConvention();
                return nullptr;
            }

            // Per-instrument Phase A eligibility. Returns true iff the instrument has a templated
            // rate, uses no projection curve (forecast == discount), and starts at the curve anchor.
            // Emits a NOTICE naming the offending condition on each fall-through.
            [[nodiscard]] bool InstrumentEligibleForAnalyticJacobian(const YCInstrument_* inst) const {
                const String_ name = inst->Name();
                const RateIndexConvention_* floatConv = PhaseAFloatConvention(inst);
                if (!floatConv) {
                    const String_ msg = String_("AAD Jacobian has no templated rate for instrument '")
                                        + name + "'; falling back to bumped";
                    NOTICE(msg);
                    return false;
                }
                if (floatConv->useProjectionCurve_) {
                    const String_ msg = String_("AAD Jacobian requires forecast==discount for every "
                                                "instrument; instrument '")
                                        + name + "' uses a projection curve, falling back to bumped";
                    NOTICE(msg);
                    return false;
                }
                // tradeDate == anchor (knotDates_.front()). Phase A's templated rates read
                // DF(tradeDate_, p) (see ycinstrument.cpp), so the gate must check the real trade
                // date via TradeDate(), not the effective/spot start that TimeSpan().first returns
                // -- a spot-started instrument has tradeDate strictly before start, and admitting
                // it would silently misprice its residual row on the tape.
                if (inst->TradeDate() != knotDates_.front()) {
                    const String_ msg = String_("AAD Jacobian requires every instrument to trade at the "
                                                "curve anchor; instrument '")
                                        + name + "' does not, falling back to bumped";
                    NOTICE(msg);
                    return false;
                }
                return true;
            }

            // Phase A AAD-tape Jacobian. Single-result reverse sweep: one forward recording per
            // Gradient call, nRows reverse sweeps (one per residual), harvest adjoints column by
            // column. Returns XCurveJacobian_ (a dense Jacobian subclass: storage is dense,
            // assembly is sparse-by-row because AAD produces exact structural zeros).
            [[nodiscard]] Underdetermined::Jacobian_* AnalyticJacobian(const Vector_<>& x, const Vector_<>& f) const;

            // Downcast instrument i to its concrete type and dispatch to PrecomputeT<T_>. Returns
            // an empty handle if the instrument type is not supported (Phase A scope is Deposit,
            // FRA, Future, vanilla Swap); EligibleForAnalyticJacobian rejects such calibrations before this
            // is ever called, so the empty-handle branch is unreachable in practice.
            template <class T_> [[nodiscard]] Handle_<Tape::Rate_<T_>> PhaseARateAt(int i) const {
                const auto* inst = instruments_[i].get();
                if (const auto* d = dynamic_cast<const Deposit_*>(inst))
                    return d->PrecomputeT<T_>();
                if (const auto* f = dynamic_cast<const FRA_*>(inst))
                    return f->PrecomputeT<T_>();
                if (const auto* fu = dynamic_cast<const Future_*>(inst))
                    return fu->PrecomputeT<T_>();
                if (const auto* s = dynamic_cast<const Swap_*>(inst))
                    return s->PrecomputeT<T_>();
                return Handle_<Tape::Rate_<T_>>();
            }
        };

        // AnalyticJacobian body. TapeGuard on entry/exit (Clear only), build Number_-typed
        // curve, build Number_-typed rates via PrecomputeT<Number_>, compute residuals, single-result
        // reverse loop. NewRecording is opened explicitly after RegisterIndependent (XAD requires
        // inputs registered before newRecording). The column map is solver col j = storage node j+1,
        // so Adjoint(logDF[j+1]) reads the sensitivity to x[j]. Backend-neutral: RegisterIndependent,
        // ZeroAdjoints, Adjoint, and PropagateToStart are all Dal::AAD facade primitives, so the same
        // loop runs unchanged under native, XAD, CoDiPack, and Adept.
        Underdetermined::Jacobian_* YieldCurveCalibrationFunc_::AnalyticJacobian(const Vector_<>& x, const Vector_<>& f) const {
            auto* tape = Dal::AAD::Tape();
            TapeGuard_ guard(tape);
            static_cast<void>(f); // the residual values themselves are unused; we recompute on the tape

            // Independents: free-node log(DF) values. Anchor (node 0) is pinned at 0 and is NOT an
            // independent -- the solver's x vector has NX() = nNodes-1 entries, matching. RegisterIndependent
            // registers each node with the active tape on every backend; the anchor stays a plain
            // assignment because routing it through RegisterIndependent would add a phantom input column.
            const int nNodes = static_cast<int>(knotDates_.size());
            REQUIRE(static_cast<int>(x.size()) == nNodes - 1,
                    "AnalyticJacobian: x vector length must equal nNodes - 1 (anchor excluded)");
            Vector_<Dal::AAD::Number_> logDF(nNodes);
            logDF[0] = 0.0; // pinned anchor; deliberately NOT registered (adjoint never read)
            for (int i = 0; i < static_cast<int>(x.size()); ++i)
                Dal::AAD::RegisterIndependent(logDF[i + 1], x[i]);

            // Open the recording AFTER registering the independents and BEFORE the forward pass.
            // XAD's canonical contract requires inputs registered before newRecording; opening the
            // recording earlier silently drops them and yields an all-zero Jacobian (the B2 class).
            // On native NewRecording is a no-op, so this is invisible there; on CoDiPack/Adept it
            // anchors the sweep bounds to this point, which is exactly what PropagateToStart sweeps.
            Dal::AAD::NewRecording(*tape);

            // Build the Number_-typed calibrated curve directly with the tape-registered logDF.
            std::unique_ptr<Tape::DiscountCurve_<Dal::AAD::Number_>> dc(
                BuildDiscountCurveT<Dal::AAD::Number_>(curveName_, ccy_, parameterization_, logDfScheme_,
                                                       knotDates_, logDF, liborBasis_, baseCurve_));
            Tape::YCCtx_<Dal::AAD::Number_> ctx(*dc);

            // Compute Number_-typed residuals: modelRate - marketRate, for every instrument.
            const int nRows = static_cast<int>(instruments_.size());
            Vector_<Dal::AAD::Number_> residuals(nRows);
            for (int i = 0; i < nRows; ++i) {
                Handle_<Tape::Rate_<Dal::AAD::Number_>> rateT = PhaseARateAt<Dal::AAD::Number_>(i);
                residuals[i] = (*rateT)(ctx) - static_cast<double>(marketRates_[i]);
            }

            // Dense Jacobian (m x NX()). Assembly is sparse by row; structural zeros stay exactly
            // zero because AAD produces exact structural zeros (one of the wins over bump).
            const int nCols = nNodes - 1;
            Matrix_<> j(nRows, nCols, 0.0);

            // Single-result reverse sweep. One PropagateToStart per residual row; ZeroAdjoints clears
            // the propagated adjoints between rows so each seed lands on a clean slate. The multi-result
            // path (SetNumResultsForAAD(true, nRows), one sweep for all rows) is a ~nRows-x speedup and
            // remains a profiling-driven follow-up; the single-result path is simpler and runs identically
            // on every backend.
            for (int i = 0; i < nRows; ++i) {
                Dal::AAD::ZeroAdjoints(*tape);
                Dal::AAD::Adjoint(residuals[i]) = 1.0;
                Dal::AAD::PropagateToStart(*tape);
                for (int col = 0; col < nCols; ++col)
                    j(i, col) = Dal::AAD::Adjoint(logDF[col + 1]);
            }
            return new XCurveJacobian_(std::move(j));
        }

        Vector_<> ModelRates(const Vector_<Handle_<YCInstrument_>>& instruments, const YieldCurve_& curve, const Handle_<YieldCurve_>& fundingCurve) {
            Vector_<> modelRates(instruments.size());
            for (int i = 0; i < static_cast<int>(instruments.size()); ++i) {
                auto rate = instruments[i]->Precompute(fundingCurve);
                modelRates[i] = (*rate)(curve);
            }
            return modelRates;
        }

        CurveCalibrationDiagnostics_ BuildDiagnostics(const String_& curveName,
                                                      const Vector_<Handle_<YCInstrument_>>& instruments,
                                                      const YieldCurve_& curve,
                                                      const Handle_<YieldCurve_>& fundingCurve,
                                                      bool usedApproximateFit,
                                                      const Matrix_<>* effJacobianInverse) {
            CurveCalibrationDiagnostics_ retval;
            retval.curveName_ = curveName;
            retval.usedApproximateFit_ = usedApproximateFit;
            retval.modelRates_ = ModelRates(instruments, curve, fundingCurve);
            retval.instrumentNames_.reserve(instruments.size());
            retval.marketRates_.reserve(instruments.size());
            retval.residuals_.reserve(instruments.size());
            double sqResidual = 0.0;
            for (int i = 0; i < static_cast<int>(instruments.size()); ++i) {
                retval.instrumentNames_.push_back(instruments[i]->Name());
                retval.marketRates_.push_back(instruments[i]->MarketRate());
                const double residual = retval.modelRates_[i] - retval.marketRates_[i];
                retval.residuals_.push_back(residual);
                retval.maxAbsResidual_ = std::max(retval.maxAbsResidual_, std::fabs(residual));
                sqResidual += residual * residual;
            }
            retval.rmsResidual_ = retval.residuals_.empty() ? 0.0 : std::sqrt(sqResidual / retval.residuals_.size());
            if (effJacobianInverse)
                retval.effJacobianInverse_ = *effJacobianInverse;
            return retval;
        }
    } // namespace

    Sparse::TriDiagonal_* BuildCurveCalibrationWeights(const Vector_<Date_>& knotDates, int paramsPerKnot, double smoothingWeight) {
        REQUIRE(!knotDates.empty(), "Curve calibration weights require knot dates");
        REQUIRE(paramsPerKnot > 0, "Curve calibration weights require positive params-per-knot");
        REQUIRE(smoothingWeight > 0.0, "Curve calibration smoothing weight must be positive");
        Vector_<DateTime_> expandedKnots;
        expandedKnots.reserve(knotDates.size() * paramsPerKnot);
        for (const auto& knot : knotDates)
            for (int i = 0; i < paramsPerKnot; ++i)
                expandedKnots.push_back(DateTime_(knot));
        return Underdetermined::WeightsPWC(expandedKnots, smoothingWeight);
    }

    Vector_<Date_> BuildCurveCalibrationKnots(const Date_& today,
                                              const Vector_<Handle_<YCInstrument_>>& instruments,
                                              const Vector_<Date_>& inputKnots,
                                              CurveKnotPolicy_ policy) {
        const Vector_<Date_> instrumentKnots = InstrumentDates(today, instruments);
        switch (policy.Switch()) {
        case CurveKnotPolicy_::Value_::INPUT:
            return UniqueSortedDates(inputKnots);
        case CurveKnotPolicy_::Value_::INSTRUMENTS:
            return instrumentKnots;
        case CurveKnotPolicy_::Value_::AUGMENTED:
            return UniqueSortedDates(Vector::Join(UniqueSortedDates(inputKnots), instrumentKnots));
        default:
            REQUIRE(false, "Unknown curve knot policy");
            return {};
        }
    }

    void ValidateCurveCalibrationSpec(const CurveCalibrationSpec_& spec) {
        REQUIRE(!spec.instruments_.empty(), "Curve calibration requires at least one instrument");
        REQUIRE(spec.smoothingWeight_ > 0.0, "Curve calibration smoothing weight must be positive");
        REQUIRE(spec.tolerance_ > 0.0, "Curve calibration tolerance must be positive");
        REQUIRE(spec.fitTolerance_ > 0.0, "Curve calibration fit tolerance must be positive");
        REQUIRE(spec.maxEvaluations_ > 0, "Curve calibration max evaluations must be positive");
        REQUIRE(spec.maxRestarts_ > 0, "Curve calibration max restarts must be positive");
        REQUIRE(std::isfinite(spec.initialGuess_), "Curve calibration initial guess must be finite");
        if (!spec.calibrateDiscountCurve_)
            REQUIRE(spec.targetTenor_ != PeriodLength_(), "Forward-curve calibration requires a target tenor");

        const Vector_<Date_> knotDates = BuildCurveCalibrationKnots(spec.today_, spec.instruments_, spec.knotDates_, spec.knotPolicy_);
        REQUIRE(!knotDates.empty(), "Curve calibration requires at least one knot date");
        const bool anchorIsToday = spec.parameterization_ == CurveParameterization_::Value_::LOG_DISCOUNT;
        if (anchorIsToday) {
            REQUIRE(knotDates.front() == spec.today_, "LOG_DISCOUNT calibration requires knot 0 to be exactly the anchor (== today)");
        } else {
            REQUIRE(knotDates.front() > spec.today_, "Curve calibration knot dates must be after today");
        }
        for (int i = 0; i < static_cast<int>(spec.initialGuessPerNode_.size()); ++i) {
            REQUIRE(std::isfinite(spec.initialGuessPerNode_[i]),
                    String_("Curve calibration per-node initial guess must be finite at index ") + String::FromInt(i));
        }
        if (anchorIsToday && !spec.initialGuessPerNode_.empty()) {
            REQUIRE(static_cast<int>(spec.initialGuessPerNode_.size()) == static_cast<int>(knotDates.size()) - 1,
                    "Curve calibration per-node initial guess length must match the number of free knots");
        }

        Date_ latestEnd = spec.today_;
        for (const auto& inst : spec.instruments_) {
            const auto span = inst->TimeSpan();
            REQUIRE(span.second > span.first, "Curve instrument maturity must follow its start date");
            REQUIRE(span.second > spec.today_, "Curve instrument maturity must be after today");
            REQUIRE(std::isfinite(inst->MarketRate()), "Curve instrument market rate must be finite");
            latestEnd = std::max(latestEnd, span.second);
        }
        REQUIRE(knotDates.back() >= latestEnd, "Curve calibration knots must span all instrument maturities");
        static_cast<void>(ParamsPerKnot(spec.parameterization_));
    }

    void ValidatePositiveDiscountFactors(const DiscountCurve_& curve, const Date_& today, const Vector_<Date_>& checkDates) {
        for (const auto& checkDate : checkDates) {
            REQUIRE(checkDate > today, "Discount factor checks require future dates");
            const double df = curve(today, checkDate);
            REQUIRE(std::isfinite(df), "Discount factor must be finite");
            REQUIRE(df > 0.0, "Discount factor must stay positive");
        }
    }

    namespace {
        // Build the initial-guess vector for the solver. Split out of CalibrateYieldCurve to keep
        // its cyclomatic complexity under the Codacy limit. Behavior is byte-for-byte identical to
        // the inlined version that previously lived in CalibrateYieldCurve.
        Vector_<> BuildCalibrationGuess(const CurveCalibrationSpec_& spec, const Vector_<Date_>& knotDates, bool anchorIsToday, int nParams) {
            const int nKnots = static_cast<int>(knotDates.size());
            Vector_<> guess(nParams);
            if (anchorIsToday) {
                if (spec.initialGuessPerNode_.empty()) {
                    // R4 default seed: flat-2% rate mapped through yf_365F from the anchor.
                    const Date_& anchor = knotDates.front();
                    for (int i = 1; i < nKnots; ++i)
                        guess[i - 1] = -FLAT_SEED_RATE * spec.liborBasis_(anchor, knotDates[i], nullptr);
                } else {
                    std::copy(spec.initialGuessPerNode_.begin(), spec.initialGuessPerNode_.end(), guess.begin());
                }
            } else {
                std::fill(guess.begin(), guess.end(), spec.initialGuess_);
            }
            return guess;
        }

        // Run the EXACT or APPROXIMATE solver and return {result, effJacobianInverse}. Split out so
        // the solveMode branch does not count against CalibrateYieldCurve's cyclomatic complexity.
        struct SolverOutput_ {
            Vector_<> result_;
            Matrix_<> effJacobianInverse_;
        };
        SolverOutput_ RunCalibrationSolver(const CurveCalibrationSpec_& spec,
                                           const Underdetermined::Function_& func,
                                           const Vector_<>& guess,
                                           const Vector_<>& tol,
                                           const Sparse::TriDiagonal_& weights) {
            Dictionary_ ctrlDict;
            ctrlDict.Insert(KEY_MAX_EVALUATIONS, Cell_(static_cast<double>(spec.maxEvaluations_)));
            ctrlDict.Insert(KEY_MAX_RESTARTS, Cell_(static_cast<double>(spec.maxRestarts_)));
            UnderdeterminedControls_ controls(ctrlDict);

            SolverOutput_ out;
            if (spec.solveMode_ == CurveSolveMode_::Value_::EXACT) {
                std::unique_ptr<Sparse::SymmetricDecomposition_> wDecomp(weights.DecomposeSymmetric());
                out.result_ = Underdetermined::Find(func, guess, tol, *wDecomp, controls, &out.effJacobianInverse_);
            } else {
                out.result_ = Underdetermined::Approximate(func, guess, tol, spec.fitTolerance_, weights, controls);
            }
            return out;
        }

        // Assemble the CurveCalibrationResult_ from the solver output. Split out so the
        // parameterization / diagnostics branches do not count against CalibrateYieldCurve's
        // cyclomatic complexity. Behavior is byte-for-byte identical to the inlined version.
        CurveCalibrationResult_ AssembleCalibrationResult(const CurveCalibrationSpec_& spec,
                                                          const Vector_<Handle_<YCInstrument_>>& instruments,
                                                          const Vector_<Date_>& knotDates,
                                                          const Vector_<>& result,
                                                          const Matrix_<>& effJacobianInverse) {
            CurveCalibrationResult_ retval;
            retval.curve_ = BuildDiscountCurve(spec.curveName_, spec.ccy_, spec.parameterization_, spec.logDfScheme_, knotDates, result, spec.liborBasis_,
                                               spec.baseCurve_);
            // For LOG_DISCOUNT the anchor knot equals today_ and would fail the strict > today check;
            // validate the free knots only.
            if (spec.parameterization_ == CurveParameterization_::Value_::LOG_DISCOUNT) {
                Vector_<Date_> freeKnots;
                for (int i = 1; i < static_cast<int>(knotDates.size()); ++i)
                    freeKnots.push_back(knotDates[i]);
                ValidatePositiveDiscountFactors(*retval.curve_, spec.today_, freeKnots);
            } else {
                ValidatePositiveDiscountFactors(*retval.curve_, spec.today_, knotDates);
            }
            Handle_<DiscountCurve_> diagnosticsCurve(std::shared_ptr<const DiscountCurve_>(std::shared_ptr<void>(), retval.curve_.get()));
            auto discountCurves = spec.discountCurves_;
            auto forwardCurves = spec.forwardCurves_;
            if (spec.calibrateDiscountCurve_)
                discountCurves[spec.targetCollateral_] = diagnosticsCurve;
            else
                forwardCurves[spec.targetTenor_] = diagnosticsCurve;
            CurveBlock_ curveView(spec.curveName_, spec.ccy_, discountCurves, forwardCurves, spec.liborBasis_);
            Handle_<YieldCurve_> fundingCurve;
            if (!spec.discountCurves_.empty())
                fundingCurve.reset(new CurveBlock_(spec.curveName_, spec.ccy_, spec.discountCurves_, spec.forwardCurves_, spec.liborBasis_));
            retval.diagnostics_ =
                BuildDiagnostics(spec.curveName_, instruments, curveView, fundingCurve, spec.solveMode_ == CurveSolveMode_::Value_::APPROXIMATE,
                                 spec.solveMode_ == CurveSolveMode_::Value_::EXACT ? &effJacobianInverse : nullptr);
            return retval;
        }
    } // namespace

    CurveCalibrationResult_ CalibrateYieldCurve(const CurveCalibrationSpec_& spec) {
        // Default-constructed options -> jacobianMode_ == BUMPED -> byte-for-byte pre-analytic path.
        return CalibrateYieldCurve(spec, CurveCalibrationOptions_());
    }

    CurveCalibrationResult_ CalibrateYieldCurve(const CurveCalibrationSpec_& spec, const CurveCalibrationOptions_& options) {
        ValidateCurveCalibrationSpec(spec);

        const Vector_<Handle_<YCInstrument_>> instruments = OrderInstruments(spec.instruments_);
        const Vector_<Date_> knotDates = BuildCurveCalibrationKnots(spec.today_, instruments, spec.knotDates_, spec.knotPolicy_);
        const int paramsPerKnot = ParamsPerKnot(spec.parameterization_);
        const int nKnots = static_cast<int>(knotDates.size());
        const bool anchorIsToday = spec.parameterization_ == CurveParameterization_::Value_::LOG_DISCOUNT;
        const int nFreeKnots = anchorIsToday ? nKnots - 1 : nKnots;
        const int nParams = paramsPerKnot * nFreeKnots;

        const Vector_<> guess = BuildCalibrationGuess(spec, knotDates, anchorIsToday, nParams);
        const Vector_<> tol(instruments.size(), spec.tolerance_);
        Vector_<Date_> weightKnots;
        if (anchorIsToday) {
            // LOG_DISCOUNT: parameter vector excludes the anchor; weights metric must match its dimension.
            for (int i = 1; i < static_cast<int>(knotDates.size()); ++i)
                weightKnots.push_back(knotDates[i]);
        } else {
            weightKnots = knotDates;
        }
        std::unique_ptr<Sparse::TriDiagonal_> weights(BuildCurveCalibrationWeights(weightKnots, paramsPerKnot, spec.smoothingWeight_));

        YieldCurveCalibrationFunc_ func(spec.ccy_, spec.curveName_, spec.parameterization_, instruments, knotDates, spec.discountCurves_,
                                        spec.forwardCurves_, spec.baseCurve_, spec.targetCollateral_, spec.targetTenor_, spec.calibrateDiscountCurve_,
                                        spec.liborBasis_, spec.logDfScheme_, options.jacobianMode_);

        const SolverOutput_ solved = RunCalibrationSolver(spec, func, guess, tol, *weights);
        return AssembleCalibrationResult(spec, instruments, knotDates, solved.result_, solved.effJacobianInverse_);
    }

    MultiCurveCalibrationResult_ CalibrateMultiCurve(const MultiCurveCalibrationSpec_& spec) {
        REQUIRE(!spec.stages_.empty(), "Multi-curve calibration requires at least one stage");
        MultiCurveCalibrationResult_ retval;
        for (const auto& inputStageSpec : spec.stages_) {
            auto stageSpec = inputStageSpec;
            ApplyStageDefaults(spec, retval, &stageSpec);

            CurveCalibrationResult_ stageResult = CalibrateYieldCurve(stageSpec);
            StoreStageResult(stageSpec, &stageResult, &retval);
        }
        return retval;
    }

    namespace TestOnly {
        Matrix_<> AnalyticJacobianAt(const CurveCalibrationSpec_& spec, const Vector_<>& x) {
            const Vector_<Handle_<YCInstrument_>> instruments = OrderInstruments(spec.instruments_);
            const Vector_<Date_> knotDates = BuildCurveCalibrationKnots(spec.today_, instruments, spec.knotDates_, spec.knotPolicy_);
            YieldCurveCalibrationFunc_ func(spec.ccy_, spec.curveName_, spec.parameterization_, instruments, knotDates,
                                            spec.discountCurves_, spec.forwardCurves_, spec.baseCurve_, spec.targetCollateral_,
                                            spec.targetTenor_, spec.calibrateDiscountCurve_, spec.liborBasis_, spec.logDfScheme_,
                                            CurveJacobianMode_::Value_::ANALYTIC);
            const Vector_<> f = func.F(x);
            std::unique_ptr<Underdetermined::Jacobian_> j(func.Gradient(x, f));
            Matrix_<> retval;
            if (j == nullptr)
                return retval; // empty signals analytic path not engaged
            const auto* dense = dynamic_cast<const XCurveJacobian_*>(j.get());
            REQUIRE(dense != nullptr, "TestOnly::AnalyticJacobianAt: expected an XCurveJacobian_");
            // Note: the solver applies DivideRows(tol_) to the returned Jacobian before use, which
            // mutates the matrix in place. The raw Gradient output is UNSCALED here -- the caller
            // applies the same row-scaling the solver would, or compares against unscaled FD.
            const Matrix_<>& src = dense->j_;
            retval = Matrix_<>(src.Rows(), src.Cols(), 0.0);
            for (int r = 0; r < src.Rows(); ++r)
                for (int c = 0; c < src.Cols(); ++c)
                    retval(r, c) = src(r, c);
            return retval;
        }
    } // namespace TestOnly

} // namespace Dal

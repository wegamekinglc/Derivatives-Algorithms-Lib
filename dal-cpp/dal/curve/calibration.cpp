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
#include <dal/curve/ycimp.hpp>
#include <dal/curve/yclogdf.hpp>
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

    // CP1 stores the LOG_DISCOUNT calibration Jacobian dense and assembles sparse. The method
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
            Vector_<> ret_val;
            Matrix::Multiply(t, j_, &ret_val);
            return ret_val;
        }
        [[nodiscard]] Vector_<> MultiplyLeft(const Vector_<>& dx) const override {
            Vector_<> ret_val;
            Matrix::Multiply(j_, dx, &ret_val);
            return ret_val;
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

        class YieldCurveCalibrationFunc_ : public Underdetermined::Function_ {
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
                auto discountCurves = discountCurves_;
                auto forwardCurves = forwardCurves_;
                if (calibrateDiscountCurve_)
                    discountCurves[targetCollateral_] = dc;
                else
                    forwardCurves[targetTenor_] = dc;
                CurveBlock_ yc(curveName_, ccy_, discountCurves, forwardCurves, liborBasis_);

                Vector_<> result(instruments_.size());
                for (int i = 0; i < static_cast<int>(instruments_.size()); ++i)
                    result[i] = (*rates_[i])(yc) - marketRates_[i];
                return result;
            }

            // Sparse Jacobian (LOG_DISCOUNT free-node log-DF unknowns). Returns nullptr unless
            // jacobianMode_ == ANALYTIC_LOG_DISCOUNT AND parameterization_ == LOG_DISCOUNT, in which
            // case the column count is NX() = nNodes - 1 (anchor pinned). The CP1 path uses no AAD
            // tape: the entire chain is double, the override runs once per solver restart, never
            // per iteration (no RAII tape guard needed -- a future CP2-style AAD port must not
            // assume a tape region is scoped here). The residual is modelRate - marketRate, so
            // dResidual_i/dx_j = dModelRate_i/dx_j (marketRate is constant, contributes nothing).
            [[nodiscard]] Underdetermined::Jacobian_* Gradient(const Vector_<>& x, const Vector_<>& f) const override {
                if (jacobianMode_ != CurveJacobianMode_::Value_::ANALYTIC_LOG_DISCOUNT)
                    return nullptr;
                if (parameterization_ != CurveParameterization_::Value_::LOG_DISCOUNT) {
                    const String_ msg = String_("Analytic LOG_DISCOUNT Jacobian requested but parameterization is ")
                                        + parameterization_.String() + "; falling back to bumped";
                    NOTICE(msg);
                    return nullptr;
                }
                // Build the calibrated curve at x -- the SAME path F() uses.
                Handle_<DiscountCurve_> dc(
                    BuildDiscountCurve(curveName_, ccy_, parameterization_, logDfScheme_, knotDates_, x, liborBasis_, baseCurve_).release());
                const auto* logDfCurve = dynamic_cast<const DiscountLogDF_*>(dc.get());
                REQUIRE(logDfCurve != nullptr,
                        "LOG_DISCOUNT analytic Jacobian requires a DiscountLogDF_ curve, got something else");

                // Build the yield-curve context the rates read from (target slotted into the
                // discount or forward position depending on calibrateDiscountCurve_).
                auto discountCurves = discountCurves_;
                auto forwardCurves = forwardCurves_;
                if (calibrateDiscountCurve_)
                    discountCurves[targetCollateral_] = dc;
                else
                    forwardCurves[targetTenor_] = dc;
                CurveBlock_ yc(curveName_, ccy_, discountCurves, forwardCurves, liborBasis_);

                const YCInstrument_::Rate_::Target_ rateTarget =
                    calibrateDiscountCurve_ ? YCInstrument_::Rate_::Target_::DISCOUNT : YCInstrument_::Rate_::Target_::FORECAST;

                const int nRows = static_cast<int>(instruments_.size());
                const int nCols = logDfCurve->NX();
                Matrix_<> j(nRows, nCols, 0.0); // structural zeros stay exactly zero

                for (int i = 0; i < nRows; ++i) {
                    const auto dRateDdf = rates_[i]->DRateDDiscount(yc, rateTarget);
                    if (dRateDdf.empty()) {
                        const String_ fallbackMsg = String_("instrument '") + instruments_[i]->Name() + "' row "
                                                    + String::FromInt(i)
                                                    + " filled by DF-bump fallback (no analytic DRateDDiscount)";
                        NOTICE(fallbackMsg);
                        FillRowByDFBump(i, *logDfCurve, yc, &j);
                        continue;
                    }
                    for (const auto& [payDate, dRate_dD] : dRateDdf) {
                        const double yfPay = liborBasis_(logDfCurve->NodeDates().front(), payDate, nullptr);
                        const double dfPay = logDfCurve->operator()(logDfCurve->NodeDates().front(), payDate);
                        for (const auto& [col, basisW] : logDfCurve->InterpBasisWeights(yfPay))
                            j(i, col) += dRate_dD * dfPay * basisW;
                    }
                }
                return new XCurveJacobian_(std::move(j));
            }

            // Per-instrument DF-bump fallback (silent, in double). For each distinct knot column,
            // bump that column's logDF by BumpSize(), re-evaluate (*rates_[i])(yc_bumped), and
            // finite-difference the rate w.r.t. the log-DF. Fills j.Row(i) with the column
            // sensitivities. Used only when DRateDDiscount returns empty.
            void FillRowByDFBump(int row,
                                 const DiscountLogDF_& baseCurve,
                                 const YieldCurve_& yc,
                                 Matrix_<>* j) const {
                constexpr double BUMP = 1.0e-7; // one-sided step for the per-instrument fallback
                const int nCols = baseCurve.NX();
                const Vector_<> baseLogDF = baseCurve.NodeLogDF();
                const Vector_<Date_> knotDates = baseCurve.NodeDates();
                const double baseRate = (*rates_[row])(yc);
                for (int col = 0; col < nCols; ++col) {
                    Vector_<> bumpedLog = baseLogDF;
                    bumpedLog[col + 1] += BUMP; // col is a solver column; storage node = col + 1
                    std::unique_ptr<DiscountCurve_> bumped(NewDiscountLogDF(curveName_ + "_b", ccy_, knotDates, bumpedLog,
                                                                            baseCurve.DayCount(), baseCurve.Scheme(), Handle_<DiscountCurve_>()));
                    Handle_<DiscountCurve_> bumpedHandle(bumped.release());
                    auto discountCurves = discountCurves_;
                    auto forwardCurves = forwardCurves_;
                    if (calibrateDiscountCurve_)
                        discountCurves[targetCollateral_] = bumpedHandle;
                    else
                        forwardCurves[targetTenor_] = bumpedHandle;
                    CurveBlock_ bumpedYC(curveName_, ccy_, discountCurves, forwardCurves, liborBasis_);
                    const double bumpedRate = (*rates_[row])(bumpedYC);
                    (*j)(row, col) = (bumpedRate - baseRate) / BUMP;
                }
            }
        };

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

    CurveCalibrationResult_ CalibrateYieldCurve(const CurveCalibrationSpec_& spec) {
        ValidateCurveCalibrationSpec(spec);

        const Vector_<Handle_<YCInstrument_>> instruments = OrderInstruments(spec.instruments_);
        const Vector_<Date_> knotDates = BuildCurveCalibrationKnots(spec.today_, instruments, spec.knotDates_, spec.knotPolicy_);
        const int paramsPerKnot = ParamsPerKnot(spec.parameterization_);
        const int nKnots = static_cast<int>(knotDates.size());
        const bool anchorIsToday = spec.parameterization_ == CurveParameterization_::Value_::LOG_DISCOUNT;
        const int nFreeKnots = anchorIsToday ? nKnots - 1 : nKnots;
        const int nParams = paramsPerKnot * nFreeKnots;

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
        Vector_<> tol(instruments.size(), spec.tolerance_);
        Vector_<Date_> weightKnots;
        if (anchorIsToday) {
            // LOG_DISCOUNT: parameter vector excludes the anchor; weights metric must match its dimension.
            for (int i = 1; i < static_cast<int>(knotDates.size()); ++i)
                weightKnots.push_back(knotDates[i]);
        } else {
            weightKnots = knotDates;
        }
        std::unique_ptr<Sparse::TriDiagonal_> weights(BuildCurveCalibrationWeights(weightKnots, paramsPerKnot, spec.smoothingWeight_));

        Dictionary_ ctrlDict;
        ctrlDict.Insert(KEY_MAX_EVALUATIONS, Cell_(static_cast<double>(spec.maxEvaluations_)));
        ctrlDict.Insert(KEY_MAX_RESTARTS, Cell_(static_cast<double>(spec.maxRestarts_)));
        UnderdeterminedControls_ controls(ctrlDict);

        YieldCurveCalibrationFunc_ func(spec.ccy_, spec.curveName_, spec.parameterization_, instruments, knotDates, spec.discountCurves_,
                                        spec.forwardCurves_, spec.baseCurve_, spec.targetCollateral_, spec.targetTenor_, spec.calibrateDiscountCurve_,
                                        spec.liborBasis_, spec.logDfScheme_, spec.jacobianMode_);
        Vector_<> result;
        Matrix_<> effJacobianInverse;
        if (spec.solveMode_ == CurveSolveMode_::Value_::EXACT) {
            std::unique_ptr<Sparse::SymmetricDecomposition_> wDecomp(weights->DecomposeSymmetric());
            result = Underdetermined::Find(func, guess, tol, *wDecomp, controls, &effJacobianInverse);
        } else {
            result = Underdetermined::Approximate(func, guess, tol, spec.fitTolerance_, *weights, controls);
        }

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
                                            spec.jacobianMode_);
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

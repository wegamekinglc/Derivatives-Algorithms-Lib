//
// Created by wegam on 2026/5/9.
//

#include <algorithm>
#include <cmath>
#include <dal/curve/aadjacobian.hpp>
#include <dal/curve/calibration.hpp>
#include <dal/curve/calibration_internal.hpp>
#include <dal/curve/curveblock.hpp>
#include <dal/curve/curvejacobian.hpp>
#include <dal/curve/curveparameterization.hpp>
#include <dal/curve/piecewiseconstant.hpp>
#include <dal/curve/piecewiselinear.hpp>
#include <dal/curve/tapeguard.hpp>
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
#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/time/datetime.hpp>
#include <dal/utilities/dictionary.hpp>
#include <dal/utilities/functionals.hpp>
#include <dal/utilities/numerics.hpp>
#include <memory>
#include <utility>

namespace Dal {
#include <dal/auto/MG_AnalyticEligibility_enum.inc>
#include <dal/auto/MG_CurveJacobianMode_enum.inc>
#include <dal/auto/MG_CurveKnotPolicy_enum.inc>
#include <dal/auto/MG_CurveParameterization_enum.inc>
#include <dal/auto/MG_CurveSolveMode_enum.inc>
#include <dal/auto/MG_LogDfScheme_enum.inc>

    namespace {
        constexpr int MAX_RELEVANT_DATES_PER_INSTRUMENT = 2;
        // Flat-rate seed for LOG_DISCOUNT calibration; log(DF) < 0, so the seed is signed (see next line).
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

        class YieldCurveCalibrationFunc_ : public Underdetermined::Function_ {
            // Cached eligibility avoids re-evaluating the expensive per-instrument predicate every solver iteration.

            String_ ccy_;
            String_ curveName_;
            Date_ anchor_;
            CurveDefinition_ definition_;
            Vector_<Handle_<YCInstrument_>> instruments_;
            Vector_<Handle_<YCInstrument_::Rate_>> rates_;
            Vector_<> marketRates_;
            std::map<CollateralType_, Handle_<DiscountCurve_>> discountCurves_;
            std::map<PeriodLength_, Handle_<DiscountCurve_>> forwardCurves_;
            Handle_<DiscountCurve_> baseCurve_;
            CollateralType_ targetCollateral_;
            PeriodLength_ targetTenor_;
            bool calibrateDiscountCurve_;
            DayBasis_ liborBasis_;
            CurveJacobianMode_ jacobianMode_;
            mutable AnalyticEligibility_ cachedEligibility_ = AnalyticEligibility_::Value_::UNKNOWN;

        public:
            YieldCurveCalibrationFunc_(const String_& ccy,
                                       const String_& curveName,
                                       const Date_& anchor,
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
                : ccy_(ccy), curveName_(curveName), anchor_(anchor),
                  definition_(MakeCurveDefinition(curveName, ccy, parameterization, logDfScheme, knotDates, anchor, liborBasis)),
                  instruments_(instruments), discountCurves_(discountCurves), forwardCurves_(forwardCurves), baseCurve_(baseCurve),
                  targetCollateral_(targetCollateral), targetTenor_(targetTenor), calibrateDiscountCurve_(calibrateDiscountCurve),
                  liborBasis_(liborBasis), jacobianMode_(jacobianMode) {
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
                Handle_<DiscountCurve_> dc(BuildDiscountCurveUniqueT<double>(definition_, x, baseCurve_).release());
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

            // Returns AAD-tape Jacobian when ANALYTIC + eligible; nullptr otherwise (solver falls back to dense bumping).
            [[nodiscard]] Underdetermined::Jacobian_* Gradient(const Vector_<>& x, const Vector_<>& f) const override {
                if (jacobianMode_ != CurveJacobianMode_::Value_::ANALYTIC) {
                    static_cast<void>(x);
                    static_cast<void>(f);
                    return nullptr;
                }
                EvaluateEligibilityOnce();
                if (cachedEligibility_ == AnalyticEligibility_::Value_::ELIGIBLE)
                    return AnalyticJacobian(x, f);
                static_cast<void>(x);
                static_cast<void>(f);
                return nullptr;
            }

            // Cache the eligibility verdict once; Gradient is called per solver iteration.
            void EvaluateEligibilityOnce() const {
                if (cachedEligibility_ != AnalyticEligibility_::Value_::UNKNOWN)
                    return;
                cachedEligibility_ =
                    EligibleForAnalyticJacobian() ? AnalyticEligibility_::Value_::ELIGIBLE : AnalyticEligibility_::Value_::INELIGIBLE;
            }

            [[nodiscard]] bool Eligible() const {
                EvaluateEligibilityOnce();
                return cachedEligibility_ == AnalyticEligibility_::Value_::ELIGIBLE;
            }

            [[nodiscard]] bool EligibleForAnalyticJacobian() const {
                if (definition_.parameterization_ == CurveParameterization_::Value_::ZERO_RATE) {
                    NOTICE("AAD Jacobian does not support the unimplemented ZERO_RATE parameterization; falling back to bumped");
                    return false;
                }
                if (!calibrateDiscountCurve_) {
                    NOTICE("AAD Jacobian requires DISCOUNT-target calibration "
                           "(calibrateDiscountCurve_ == true); falling back to bumped");
                    return false;
                }
                for (int i = 0; i < static_cast<int>(instruments_.size()); ++i)
                    if (!InstrumentEligibleForAnalyticJacobian(instruments_[i].get()))
                        return false;
                return true;
            }

            [[nodiscard]] bool InstrumentEligibleForAnalyticJacobian(const YCInstrument_* inst) const {
                const String_ name = inst->Name();
                const RateIndexConvention_* floatConv = FloatConventionOf(*inst);
                if (!floatConv) {
                    const String_ msg = String_("AAD Jacobian has no templated rate for instrument '") + name + "'; falling back to bumped";
                    NOTICE(msg);
                    return false;
                }
                if (floatConv->useProjectionCurve_) {
                    const String_ msg = String_("AAD Jacobian requires forecast==discount for every "
                                                "instrument; instrument '") +
                                        name + "' uses a projection curve, falling back to bumped";
                    NOTICE(msg);
                    return false;
                }
                // Must use TradeDate(), not TimeSpan().first -- spot-started instruments trade before start.
                if (inst->TradeDate() != anchor_) {
                    const String_ msg = String_("AAD Jacobian requires every instrument to trade at the "
                                                "curve anchor; instrument '") +
                                        name + "' does not, falling back to bumped";
                    NOTICE(msg);
                    return false;
                }
                return true;
            }

            [[nodiscard]] Underdetermined::Jacobian_* AnalyticJacobian(const Vector_<>& x, const Vector_<>& f) const;

            template <class T_> [[nodiscard]] Handle_<Tape::Rate_<T_>> PhaseARateAt(int i) const {
                return VisitRate(
                    *instruments_[i], [](const Deposit_& d) { return d.PrecomputeT<T_>(); }, [](const FRA_& f) { return f.PrecomputeT<T_>(); },
                    [](const Future_& fu) { return fu.PrecomputeT<T_>(); }, [](const Swap_& s) { return s.PrecomputeT<T_>(); });
            }
        };

        Underdetermined::Jacobian_* YieldCurveCalibrationFunc_::AnalyticJacobian(const Vector_<>& x, const Vector_<>& f) const {
            auto* tape = Dal::AAD::Tape();
            TapeGuard_ guard(tape);
            static_cast<void>(f); // the residual values themselves are unused; we recompute on the tape

            const CurveParameterLayout_ layout = BuildCurveParameterLayout(definition_);
            REQUIRE(static_cast<int>(x.size()) == layout.parameterCount_, "AnalyticJacobian: x vector length must equal the curve parameter count");
            Vector_<Dal::AAD::Number_> parameters = RegisterCurveParameters(x);
            Dal::AAD::NewRecording(*tape);

            auto dc = BuildDiscountCurveUniqueT<Dal::AAD::Number_>(definition_, parameters, baseCurve_);
            Tape::YCCtx_<Dal::AAD::Number_> ctx(*dc);

            const int nRows = static_cast<int>(instruments_.size());
            Vector_<Dal::AAD::Number_> residuals(nRows);
            for (int i = 0; i < nRows; ++i) {
                Handle_<Tape::Rate_<Dal::AAD::Number_>> rateT = PhaseARateAt<Dal::AAD::Number_>(i);
                residuals[i] = (*rateT)(ctx) - static_cast<double>(marketRates_[i]);
            }

            return new XCurveJacobian_(HarvestCurveJacobian(*tape, parameters, residuals));
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
            for (int i = 0; i < static_cast<int>(instruments.size()); ++i) {
                retval.instrumentNames_.push_back(instruments[i]->Name());
                retval.marketRates_.push_back(instruments[i]->MarketRate());
                retval.residuals_.push_back(retval.modelRates_[i] - retval.marketRates_[i]);
            }
            const ResidualStats_ stats = ResidualStats(retval.residuals_);
            retval.maxAbsResidual_ = stats.maxAbsResidual_;
            retval.rmsResidual_ = stats.rmsResidual_;
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
        static_cast<void>(BuildCurveParameterLayout(
            MakeCurveDefinition(spec.curveName_, spec.ccy_, spec.parameterization_, spec.logDfScheme_, knotDates, spec.today_, spec.liborBasis_)));
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
        Vector_<> BuildCalibrationGuess(const CurveCalibrationSpec_& spec, const Vector_<Date_>& knotDates, bool anchorIsToday, int nParams) {
            const int nKnots = static_cast<int>(knotDates.size());
            Vector_<> guess(nParams);
            if (anchorIsToday) {
                if (spec.initialGuessPerNode_.empty()) {
                    // Default seed: flat-2% rate mapped through yf_365F from the anchor.
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

        struct SolverOutput_ {
            Vector_<> result_;
            Matrix_<> effJacobianInverse_;
        };
        SolverOutput_ RunCalibrationSolver(const CurveCalibrationSpec_& spec,
                                           const Underdetermined::Function_& func,
                                           const Vector_<>& guess,
                                           const Vector_<>& tol,
                                           const Sparse::TriDiagonal_& weights,
                                           bool computeEffJacobianInverse,
                                           Matrix_<>* fwdJacobian) {
            Dictionary_ ctrlDict;
            ctrlDict.Insert(KEY_MAX_EVALUATIONS, Cell_(static_cast<double>(spec.maxEvaluations_)));
            ctrlDict.Insert(KEY_MAX_RESTARTS, Cell_(static_cast<double>(spec.maxRestarts_)));
            UnderdeterminedControls_ controls(ctrlDict);

            SolverOutput_ out;
            if (spec.solveMode_ == CurveSolveMode_::Value_::EXACT) {
                std::unique_ptr<Sparse::SymmetricDecomposition_> wDecomp(weights.DecomposeSymmetric());
                Matrix_<>* effJacobianInverse = computeEffJacobianInverse ? &out.effJacobianInverse_ : nullptr;
                out.result_ = Underdetermined::Find(func, guess, tol, *wDecomp, controls, effJacobianInverse, fwdJacobian);
            } else {
                out.result_ = Underdetermined::Approximate(func, guess, tol, spec.fitTolerance_, weights, controls);
            }
            return out;
        }

        CurveCalibrationResult_ AssembleCalibrationResult(const CurveCalibrationSpec_& spec,
                                                          const Vector_<Handle_<YCInstrument_>>& instruments,
                                                          const CurveDefinition_& definition,
                                                          const Vector_<Date_>& knotDates,
                                                          const Vector_<>& result,
                                                          const Matrix_<>* effJacobianInverse) {
            CurveCalibrationResult_ retval;
            retval.curve_ = BuildDiscountCurveUniqueT<double>(definition, result, spec.baseCurve_);
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
                                 spec.solveMode_ == CurveSolveMode_::Value_::EXACT ? effJacobianInverse : nullptr);
            return retval;
        }
    } // namespace

    CurveCalibrationResult_ CalibrateYieldCurve(const CurveCalibrationSpec_& spec) { return CalibrateYieldCurve(spec, CurveCalibrationOptions_()); }

    CurveCalibrationResult_ CalibrateYieldCurve(const CurveCalibrationSpec_& spec, const CurveCalibrationOptions_& options) {
        ValidateCurveCalibrationSpec(spec);

        const Vector_<Handle_<YCInstrument_>> instruments = OrderInstruments(spec.instruments_);
        const Vector_<Date_> knotDates = BuildCurveCalibrationKnots(spec.today_, instruments, spec.knotDates_, spec.knotPolicy_);
        const CurveDefinition_ definition =
            MakeCurveDefinition(spec.curveName_, spec.ccy_, spec.parameterization_, spec.logDfScheme_, knotDates, spec.today_, spec.liborBasis_);
        const CurveParameterLayout_ layout = BuildCurveParameterLayout(definition);
        const int paramsPerKnot = layout.paramsPerDeclaredKnot_;
        const bool anchorIsToday = layout.pinnedAnchor_;
        const int nParams = layout.parameterCount_;

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

        YieldCurveCalibrationFunc_ func(spec.ccy_, spec.curveName_, spec.today_, spec.parameterization_, instruments, knotDates, spec.discountCurves_,
                                        spec.forwardCurves_, spec.baseCurve_, spec.targetCollateral_, spec.targetTenor_, spec.calibrateDiscountCurve_,
                                        spec.liborBasis_, spec.logDfScheme_, options.jacobianMode_);

        // Forward Jacobian requested only for ANALYTIC + EXACT + eligible; nullptr otherwise so the solver leaves the output empty.
        const bool wantFwdJacobian = options.computeForwardJacobian_ && options.jacobianMode_ == CurveJacobianMode_::Value_::ANALYTIC &&
                                     spec.solveMode_ == CurveSolveMode_::Value_::EXACT && func.Eligible();
        Matrix_<> fwdJacobian;
        const SolverOutput_ solved =
            RunCalibrationSolver(spec, func, guess, tol, *weights, options.computeEffJacobianInverse_, wantFwdJacobian ? &fwdJacobian : nullptr);
        CurveCalibrationResult_ retval = AssembleCalibrationResult(spec, instruments, definition, knotDates, solved.result_,
                                                                   options.computeEffJacobianInverse_ ? &solved.effJacobianInverse_ : nullptr);
        retval.diagnostics_.jacobian_ = std::move(fwdJacobian);
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

} // namespace Dal

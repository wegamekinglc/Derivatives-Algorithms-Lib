//
// Created by wegam on 2026/5/9.
//

#include <algorithm>
#include <atomic>
#include <cmath>
#include <dal/curve/calibration.hpp>
#include <dal/curve/calibration_internal.hpp>
#include <dal/curve/curveblock.hpp>
#include <dal/curve/curveparameterization.hpp>
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
#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/time/datetime.hpp>
#include <dal/utilities/dictionary.hpp>
#include <dal/utilities/functionals.hpp>
#include <dal/utilities/numerics.hpp>
#include <exception>
#include <memory>
#include <new>
#include <typeinfo>
#include <utility>

namespace Dal {
#include <dal/auto/MG_AnalyticEligibility_enum.inc>
#include <dal/auto/MG_AnalyticIneligibilityReason_enum.inc>
#include <dal/auto/MG_CurveFreeParameterComponent_enum.inc>
#include <dal/auto/MG_CurveJacobianMode_enum.inc>
#include <dal/auto/MG_CurveKnotCandidateDisposition_enum.inc>
#include <dal/auto/MG_CurveKnotOriginKind_enum.inc>
#include <dal/auto/MG_CurveKnotPolicy_enum.inc>
#include <dal/auto/MG_CurveParameterization_enum.inc>
#include <dal/auto/MG_CurveSolveMode_enum.inc>
#include <dal/auto/MG_LogDfScheme_enum.inc>

    namespace {
        constexpr int MAX_RELEVANT_DATES_PER_INSTRUMENT = 2;
        // Benchmark/test observation seam: counts calibrations so the quote-risk performance gate
        // can assert the timed aggregate never recalibrates. Exported across the library boundary on
        // purpose -- a header-inline counter would split into one instance per module on Windows.
        // The count is process-global, so observing it requires a quiescent process.
        std::atomic<int> calibrationInvocationCount{0};

        void ValidateInstrumentHandles(const CurveCalibrationSpec_& spec) {
            REQUIRE(!spec.instruments_.empty(), "Curve calibration requires at least one instrument");
            for (int i = 0; i < static_cast<int>(spec.instruments_.size()); ++i)
                REQUIRE(spec.instruments_[i], spec.curveName_ + " has an empty instrument at input index " + String::FromInt(i));
        }

        bool HasTemplatedSingleRate(const YCInstrument_& instrument) {
            return dynamic_cast<const Deposit_*>(&instrument) || dynamic_cast<const FRA_*>(&instrument) ||
                   dynamic_cast<const Future_*>(&instrument) || dynamic_cast<const Swap_*>(&instrument);
        }

        AnalyticEligibilityReport_
        SingleAnalyticEligibility(const Date_& anchor, bool calibrateDiscountCurve, const Vector_<Handle_<YCInstrument_>>& instruments) {
            AnalyticEligibilityReport_ report;
            if (!calibrateDiscountCurve) {
                AddEligibilityIssue(&report, AnalyticIneligibilityReason_::Value_::DISCOUNT_TARGET_REQUIRED, "single", -1, -1, -1,
                                    "AAD Jacobian requires a discount-target declaration");
            }
            for (int i = 0; i < static_cast<int>(instruments.size()); ++i) {
                const YCInstrument_* instrument = instruments[i].get();
                if (!instrument || !HasTemplatedSingleRate(*instrument)) {
                    AddEligibilityIssue(&report, AnalyticIneligibilityReason_::Value_::TEMPLATED_RATE_UNAVAILABLE, "single", -1, i, -1,
                                        "AAD Jacobian has no templated rate for the instrument");
                    continue;
                }
                const RateIndexConvention_* convention = FloatConventionOf(*instrument);
                if (!convention) {
                    AddEligibilityIssue(&report, AnalyticIneligibilityReason_::Value_::TEMPLATED_RATE_UNAVAILABLE, "single", -1, i, -1,
                                        "AAD Jacobian has no floating-rate convention for the instrument");
                    continue;
                }
                if (convention->useProjectionCurve_) {
                    AddEligibilityIssue(&report, AnalyticIneligibilityReason_::Value_::PROJECTION_NOT_ALLOWED, "single", -1, i, -1,
                                        "AAD Jacobian requires forecast and discount routing to coincide");
                }
                if (instrument->TradeDate() != anchor) {
                    AddEligibilityIssue(&report, AnalyticIneligibilityReason_::Value_::TRADE_DATE_MISMATCH, "single", -1, i, -1,
                                        "instrument trade date does not equal the curve anchor");
                }
            }
            return report;
        }
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

        const Handle_<DiscountCurve_>&
        StoreStageResult(const CurveCalibrationSpec_& stageSpec, CurveCalibrationResult_* stageResult, MultiCurveCalibrationResult_* multiResult) {
            auto& calibrated = stageSpec.calibrateDiscountCurve_ ? multiResult->discountCurves_[stageSpec.targetCollateral_]
                                                                 : multiResult->forwardCurves_[stageSpec.targetTenor_];
            calibrated.reset(stageResult->curve_.release());
            multiResult->diagnostics_.push_back(std::move(stageResult->diagnostics_));
            return calibrated;
        }

        CurveKnotOrigin_ InputOrigin(int index) { return {CurveKnotOriginKind_::Value_::INPUT, index, -1}; }

        CurveKnotOrigin_ InstrumentOrigin(CurveKnotOriginKind_ kind, int index) {
            REQUIRE(kind == CurveKnotOriginKind_::Value_::INSTRUMENT_START || kind == CurveKnotOriginKind_::Value_::INSTRUMENT_END,
                    "Instrument knot origin must be a start or end");
            return {kind, -1, index};
        }

        CurveKnotOrigin_ SyntheticAnchorOrigin() { return {CurveKnotOriginKind_::Value_::SYNTHETIC_ANCHOR, -1, -1}; }

        void VisitKnotCandidate(ResolvedSingleKnotPlan_* result,
                                std::map<Date_, int>* traversalIndex,
                                const Date_& today,
                                const Date_& date,
                                const CurveKnotOrigin_& origin,
                                bool filterNotAfterToday) {
            CurveKnotCandidate_ candidate;
            candidate.ordinal_ = static_cast<int>(result->candidateTrace_.size());
            candidate.date_ = date;
            candidate.origin_ = origin;
            if (filterNotAfterToday && date <= today) {
                candidate.disposition_ = CurveKnotCandidateDisposition_::Value_::FILTERED_NOT_AFTER_TODAY;
            } else {
                const auto [found, inserted] = traversalIndex->emplace(date, static_cast<int>(result->resolvedDeclaredNodes_.size()));
                candidate.resolvedIndex_ = found->second;
                if (inserted) {
                    candidate.disposition_ = CurveKnotCandidateDisposition_::Value_::ADDED;
                    result->resolvedDeclaredNodes_.push_back({date, {origin}});
                } else {
                    candidate.disposition_ = CurveKnotCandidateDisposition_::Value_::DUPLICATE;
                    result->resolvedDeclaredNodes_[found->second].origins_.push_back(origin);
                }
            }
            result->candidateTrace_.push_back(candidate);
        }

        void VisitSubmittedKnots(CurveKnotPolicy_ requestedPolicy,
                                 const Vector_<Date_>& submittedKnots,
                                 const Date_& today,
                                 ResolvedSingleKnotPlan_* result,
                                 std::map<Date_, int>* traversalIndex) {
            if (requestedPolicy != CurveKnotPolicy_::Value_::INPUT && requestedPolicy != CurveKnotPolicy_::Value_::AUGMENTED)
                return;
            for (int i = 0; i < static_cast<int>(submittedKnots.size()); ++i)
                VisitKnotCandidate(result, traversalIndex, today, submittedKnots[i], InputOrigin(i), false);
        }

        void VisitInstrumentKnots(CurveKnotPolicy_ requestedPolicy,
                                  const Vector_<Handle_<YCInstrument_>>& instruments,
                                  const Date_& today,
                                  ResolvedSingleKnotPlan_* result,
                                  std::map<Date_, int>* traversalIndex) {
            if (requestedPolicy == CurveKnotPolicy_::Value_::INPUT)
                return;
            REQUIRE(requestedPolicy == CurveKnotPolicy_::Value_::INSTRUMENTS || requestedPolicy == CurveKnotPolicy_::Value_::AUGMENTED,
                    "Unknown curve knot policy");
            result->counts_.instrumentCandidates_ = static_cast<int>(instruments.size()) * MAX_RELEVANT_DATES_PER_INSTRUMENT;
            for (int i = 0; i < static_cast<int>(instruments.size()); ++i) {
                const auto span = instruments[i]->TimeSpan();
                VisitKnotCandidate(result, traversalIndex, today, span.first, InstrumentOrigin(CurveKnotOriginKind_::Value_::INSTRUMENT_START, i),
                                   true);
                VisitKnotCandidate(result, traversalIndex, today, span.second, InstrumentOrigin(CurveKnotOriginKind_::Value_::INSTRUMENT_END, i),
                                   true);
            }
        }

        std::map<Date_, int> SortResolvedKnots(ResolvedSingleKnotPlan_* result) {
            std::sort(result->resolvedDeclaredNodes_.begin(), result->resolvedDeclaredNodes_.end(),
                      [](const ResolvedCurveKnotNode_& lhs, const ResolvedCurveKnotNode_& rhs) { return lhs.date_ < rhs.date_; });
            std::map<Date_, int> sortedIndex;
            for (int i = 0; i < static_cast<int>(result->resolvedDeclaredNodes_.size()); ++i)
                sortedIndex[result->resolvedDeclaredNodes_[i].date_] = i;
            for (auto& candidate : result->candidateTrace_)
                if (candidate.disposition_ != CurveKnotCandidateDisposition_::Value_::FILTERED_NOT_AFTER_TODAY)
                    candidate.resolvedIndex_ = sortedIndex.at(candidate.date_);
            return sortedIndex;
        }

        void RequireLogDiscountAnchor(const ResolvedSingleKnotPlan_& result,
                                      const Vector_<Date_>& resolvedDates,
                                      CurveParameterization_ parameterization,
                                      const Date_& today) {
            if (parameterization != CurveParameterization_::Value_::LOG_DISCOUNT)
                return;
            REQUIRE(resolvedDates.front() == today &&
                        std::any_of(result.resolvedDeclaredNodes_.front().origins_.begin(), result.resolvedDeclaredNodes_.front().origins_.end(),
                                    [](const CurveKnotOrigin_& origin) { return origin.kind_ == CurveKnotOriginKind_::Value_::INPUT; }),
                    "LOG_DISCOUNT knot planning requires an input anchor at today");
        }

        void ProjectKnotRepresentation(ResolvedSingleKnotPlan_* result,
                                       bool projectRepresentation,
                                       CurveParameterization_ parameterization,
                                       const Date_& today,
                                       const std::map<Date_, int>& sortedIndex) {
            if (!projectRepresentation || result->resolvedDeclaredNodes_.empty())
                return;
            Vector_<Date_> resolvedDates;
            resolvedDates.reserve(result->resolvedDeclaredNodes_.size());
            for (const auto& node : result->resolvedDeclaredNodes_)
                resolvedDates.push_back(node.date_);
            RequireLogDiscountAnchor(*result, resolvedDates, parameterization, today);
            const CurveDefinition_ definition =
                MakeCurveDefinition("planned", "", parameterization, LogDfScheme_::Value_::LOG_LINEAR, resolvedDates, today, DayBasis::Act365F());
            result->freeParameters_ = DescribeCurveFreeParameters(definition);
            result->storageNodes_.reserve(definition.nodeDates_.size());
            for (const auto& date : definition.nodeDates_) {
                if (parameterization == CurveParameterization_::Value_::ZERO_RATE && date == today) {
                    result->storageNodes_.push_back({today, {SyntheticAnchorOrigin()}});
                    result->anchorAdded_ = true;
                } else {
                    result->storageNodes_.push_back(result->resolvedDeclaredNodes_[sortedIndex.at(date)]);
                }
            }
        }

        ResolvedSingleKnotPlan_ BuildKnotPlan(const Date_& today,
                                              const Vector_<Handle_<YCInstrument_>>& instruments,
                                              const Vector_<Date_>& submittedKnots,
                                              CurveKnotPolicy_ requestedPolicy,
                                              CurveParameterization_ parameterization,
                                              bool projectRepresentation = true) {
            ResolvedSingleKnotPlan_ result;
            result.requestedPolicy_ = requestedPolicy;
            result.submittedKnotDates_ = submittedKnots;
            result.counts_.submittedKnots_ = static_cast<int>(submittedKnots.size());

            std::map<Date_, int> traversalIndex;
            VisitSubmittedKnots(requestedPolicy, submittedKnots, today, &result, &traversalIndex);
            VisitInstrumentKnots(requestedPolicy, instruments, today, &result, &traversalIndex);
            const std::map<Date_, int> sortedIndex = SortResolvedKnots(&result);
            ProjectKnotRepresentation(&result, projectRepresentation, parameterization, today, sortedIndex);

            result.counts_.resolvedDeclaredNodes_ = static_cast<int>(result.resolvedDeclaredNodes_.size());
            result.counts_.storageNodes_ = static_cast<int>(result.storageNodes_.size());
            result.counts_.freeParameters_ = static_cast<int>(result.freeParameters_.size());
            return result;
        }

        using AnalyticRates_ = Vector_<Handle_<Tape::Rate_<AAD::Number_>>>;

        AnalyticRates_ PrepareAnalyticRates(const Vector_<Handle_<YCInstrument_>>& instruments) {
            AnalyticRates_ rates;
            rates.reserve(instruments.size());
            for (const auto& instrument : instruments)
                rates.push_back(VisitRate(
                    *instrument, [](const Deposit_& d) { return d.PrecomputeT<AAD::Number_>(); }, [](const FRA_& f) { return f.PrecomputeT<AAD::Number_>(); },
                    [](const Future_& f) { return f.PrecomputeT<AAD::Number_>(); }, [](const Swap_& s) { return s.PrecomputeT<AAD::Number_>(); }));
            return rates;
        }

        class YieldCurveCalibrationFunc_ : public Underdetermined::Function_ {
            // Cached eligibility avoids re-evaluating the expensive per-instrument predicate every solver iteration.

            String_ ccy_;
            String_ curveName_;
            Date_ anchor_;
            CurveDefinition_ definition_;
            Vector_<Handle_<YCInstrument_>> instruments_;
            Vector_<Handle_<YCInstrument_::Rate_>> rates_;
            AnalyticRates_* analyticRates_;
            Vector_<> marketRates_;
            std::map<CollateralType_, Handle_<DiscountCurve_>> discountCurves_;
            std::map<PeriodLength_, Handle_<DiscountCurve_>> forwardCurves_;
            Handle_<DiscountCurve_> baseCurve_;
            CollateralType_ targetCollateral_;
            PeriodLength_ targetTenor_;
            bool calibrateDiscountCurve_;
            DayBasis_ liborBasis_;
            CurveJacobianMode_ jacobianMode_;
            double bumpSize_;
            AnalyticEligibility_ analyticEligibility_ = AnalyticEligibility_::Value_::UNKNOWN;

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
                                       CurveJacobianMode_ jacobianMode,
                                       CurveSolveMode_ solveMode,
                                       AnalyticRates_* analyticRates)
                : ccy_(ccy), curveName_(curveName), anchor_(anchor),
                  definition_(MakeCurveDefinition(curveName, ccy, parameterization, logDfScheme, knotDates, anchor, liborBasis)),
                  instruments_(instruments), analyticRates_(analyticRates), discountCurves_(discountCurves), forwardCurves_(forwardCurves), baseCurve_(baseCurve),
                  targetCollateral_(targetCollateral), targetTenor_(targetTenor), calibrateDiscountCurve_(calibrateDiscountCurve),
                  liborBasis_(liborBasis), jacobianMode_(jacobianMode), bumpSize_(solveMode == CurveSolveMode_::Value_::EXACT ? 1.0e-6 : 1.0e-4) {
                Handle_<YieldCurve_> fundingYC;
                if (!discountCurves_.empty())
                    fundingYC.reset(new CurveBlock_(curveName_, ccy_, discountCurves_, forwardCurves_, liborBasis_));
                rates_.reserve(instruments_.size());
                marketRates_.reserve(instruments_.size());
                for (const auto& inst : instruments_) {
                    rates_.push_back(inst->Precompute(fundingYC));
                    marketRates_.push_back(inst->MarketRate());
                }
                if (jacobianMode_ == CurveJacobianMode_::Value_::ANALYTIC)
                    analyticEligibility_ =
                        EligibleForAnalyticJacobian() ? AnalyticEligibility_::Value_::ELIGIBLE : AnalyticEligibility_::Value_::INELIGIBLE;
            }

            // Exact quote-risk diagnostics need the oracle bump; approximate solves retain their historical linearization.
            [[nodiscard]] double BumpSize() const override { return bumpSize_; }

            [[nodiscard]] Vector_<> F(const Vector_<>& x) const override {
                Handle_<DiscountCurve_> dc(BuildDiscountCurveUniqueT<double>(definition_, x, baseCurve_).release());
                CurveBlock_ yc = YieldCurveWith(dc);

                Vector_<> result(instruments_.size());
                for (int i = 0; i < static_cast<int>(instruments_.size()); ++i)
                    result[i] = (*rates_[i])(yc)-marketRates_[i];
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
            [[nodiscard]] std::unique_ptr<Underdetermined::Jacobian_> Gradient(const Vector_<>& x, const Vector_<>& f) const override {
                if (jacobianMode_ != CurveJacobianMode_::Value_::ANALYTIC) {
                    static_cast<void>(x);
                    static_cast<void>(f);
                    return nullptr;
                }
                if (analyticEligibility_ == AnalyticEligibility_::Value_::ELIGIBLE)
                    return AnalyticJacobian(x, f);
                static_cast<void>(x);
                static_cast<void>(f);
                return nullptr;
            }

            [[nodiscard]] bool Eligible() const { return analyticEligibility_ == AnalyticEligibility_::Value_::ELIGIBLE; }

            [[nodiscard]] bool EligibleForAnalyticJacobian() const {
                const AnalyticEligibilityReport_ report = SingleAnalyticEligibility(anchor_, calibrateDiscountCurve_, instruments_);
                if (!report.eligible_) {
                    const String_ message = report.issues_.front().nativeMessage_ + "; falling back to bumped";
                    NOTICE(message);
                }
                return report.eligible_;
            }

            [[nodiscard]] std::unique_ptr<Underdetermined::Jacobian_> AnalyticJacobian(const Vector_<>& x, const Vector_<>& f) const;
        };

        std::unique_ptr<Underdetermined::Jacobian_> YieldCurveCalibrationFunc_::AnalyticJacobian(const Vector_<>& x, const Vector_<>& f) const {
            static_cast<void>(f); // the residual values themselves are unused; we recompute on the tape
            const CurveParameterLayout_ layout = BuildCurveParameterLayout(definition_);
            REQUIRE(static_cast<int>(x.size()) == layout.parameterCount_, "AnalyticJacobian: x vector length must equal the curve parameter count");
            // The solve owns the schedules. An initial fit without diagnostics never needs them.
            if (analyticRates_->empty())
                *analyticRates_ = PrepareAnalyticRates(instruments_);
            return TapeResidualJacobian(x, [&](const Vector_<Dal::AAD::Number_>& parameters) {
                auto dc = BuildDiscountCurveUniqueT<Dal::AAD::Number_>(definition_, parameters, baseCurve_);
                Tape::YCCtx_<Dal::AAD::Number_> ctx(*dc);

                const int nRows = static_cast<int>(instruments_.size());
                Vector_<Dal::AAD::Number_> residuals(nRows);
                for (int i = 0; i < nRows; ++i)
                    residuals[i] = (*(*analyticRates_)[i])(ctx) - static_cast<double>(marketRates_[i]);
                return residuals;
            });
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

    void RecordCurveCalibrationInvocation() { calibrationInvocationCount.fetch_add(1, std::memory_order_relaxed); }

    void ResetCurveCalibrationInvocationCount() { calibrationInvocationCount.store(0, std::memory_order_relaxed); }

    int CurveCalibrationInvocationCount() { return calibrationInvocationCount.load(std::memory_order_relaxed); }

    std::unique_ptr<Sparse::TriDiagonal_> BuildCurveCalibrationWeights(const Vector_<Date_>& knotDates, int paramsPerKnot, double smoothingWeight) {
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
        const auto plan = BuildKnotPlan(today, instruments, inputKnots, policy, CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD, false);
        Vector_<Date_> result;
        result.reserve(plan.resolvedDeclaredNodes_.size());
        for (const auto& node : plan.resolvedDeclaredNodes_)
            result.push_back(node.date_);
        return result;
    }

    ResolvedSingleKnotPlan_ PlanCurveCalibrationKnots(const Date_& today,
                                                      const Vector_<Handle_<YCInstrument_>>& instruments,
                                                      const Vector_<Date_>& submittedKnots,
                                                      CurveKnotPolicy_ requestedPolicy,
                                                      CurveParameterization_ parameterization) {
        return BuildKnotPlan(today, instruments, submittedKnots, requestedPolicy, parameterization);
    }

    ExecutionSingleKnotIdentity_ InspectCurveCalibrationExecutionIdentity(const CurveCalibrationSpec_& finalInputSpec) {
        REQUIRE(finalInputSpec.KnotPolicy() == CurveKnotPolicy_::Value_::INPUT, "Execution identity inspection requires an INPUT knot policy");
        const CurveDefinition_ definition =
            MakeCurveDefinition(finalInputSpec.curveName_, finalInputSpec.ccy_, finalInputSpec.Parameterization(), finalInputSpec.LogDfScheme(),
                                finalInputSpec.KnotDates(), finalInputSpec.Today(), finalInputSpec.liborBasis_);

        ExecutionSingleKnotIdentity_ result;
        result.today_ = finalInputSpec.Today();
        result.parameterization_ = finalInputSpec.Parameterization();
        if (result.parameterization_ == CurveParameterization_::Value_::ZERO_RATE ||
            result.parameterization_ == CurveParameterization_::Value_::LOG_DISCOUNT)
            result.logDfScheme_ = finalInputSpec.LogDfScheme();
        result.resolvedDeclaredDates_ = finalInputSpec.KnotDates();
        result.storageDates_ = definition.nodeDates_;
        result.freeParameters_ = DescribeCurveFreeParameters(definition);
        result.counts_.resolvedDeclaredNodes_ = static_cast<int>(result.resolvedDeclaredDates_.size());
        result.counts_.storageNodes_ = static_cast<int>(result.storageDates_.size());
        result.counts_.freeParameters_ = static_cast<int>(result.freeParameters_.size());
        return result;
    }

    void ValidateCurveCalibrationSpec(const CurveCalibrationSpec_& spec) {
        ValidateInstrumentHandles(spec);
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
        if (spec.parameterization_ == CurveParameterization_::Value_::LOG_DISCOUNT) {
            REQUIRE(knotDates.front() == spec.today_, "LOG_DISCOUNT calibration requires knot 0 to be exactly the anchor (== today)");
        } else if (spec.parameterization_ == CurveParameterization_::Value_::ZERO_RATE) {
            REQUIRE(knotDates.front() > spec.today_, "ZERO_RATE calibration requires every knot to be strictly after today");
        } else if (spec.parameterization_ == CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD) {
            REQUIRE(knotDates.front() >= spec.today_, "PIECEWISE_CONSTANT_FWD calibration requires knot 0 to be on or after today");
        } else {
            REQUIRE(knotDates.front() > spec.today_, "Curve calibration knot dates must be after today");
        }
        const CurveDefinition_ definition =
            MakeCurveDefinition(spec.curveName_, spec.ccy_, spec.parameterization_, spec.logDfScheme_, knotDates, spec.today_, spec.liborBasis_);
        const CurveParameterLayout_ layout = BuildCurveParameterLayout(definition);
        for (int i = 0; i < static_cast<int>(spec.initialGuessPerNode_.size()); ++i) {
            REQUIRE(std::isfinite(spec.initialGuessPerNode_[i]),
                    String_("Curve calibration per-node initial guess must be finite at index ") + String::FromInt(i));
        }
        if (!spec.initialGuessPerNode_.empty())
            REQUIRE(static_cast<int>(spec.initialGuessPerNode_.size()) == layout.parameterCount_,
                    "Curve calibration per-node initial guess length must match the curve parameter count");

        Date_ latestEnd = spec.today_;
        for (const auto& inst : spec.instruments_) {
            const auto span = inst->TimeSpan();
            REQUIRE(span.second > span.first, "Curve instrument maturity must follow its start date");
            REQUIRE(span.second > spec.today_, "Curve instrument maturity must be after today");
            REQUIRE(std::isfinite(inst->MarketRate()), "Curve instrument market rate must be finite");
            latestEnd = std::max(latestEnd, span.second);
        }
        REQUIRE(knotDates.back() >= latestEnd, "Curve calibration knots must span all instrument maturities");
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
        Vector_<> BuildCalibrationGuess(const CurveCalibrationSpec_& spec, const CurveDefinition_& definition, const CurveParameterLayout_& layout) {
            static_cast<void>(layout); // the shared slice rebuilds the layout from the definition
            return BuildGuessSlice(spec, definition, spec.initialGuess_, String_("Curve calibration"));
        }

        CurveCalibrationResult_ AssembleCalibrationResult(const CurveCalibrationSpec_& spec,
                                                          const Vector_<Handle_<YCInstrument_>>& instruments,
                                                          const CurveDefinition_& definition,
                                                          const Vector_<Date_>& knotDates,
                                                          const Vector_<>& result,
                                                          const Matrix_<>* effJacobianInverse) {
            CurveCalibrationResult_ retval;
            retval.curve_ = BuildDiscountCurveUniqueT<double>(definition, result, spec.baseCurve_);
            Vector_<Date_> futureKnots;
            for (const auto& knot : knotDates)
                if (knot > spec.today_)
                    futureKnots.push_back(knot);
            ValidatePositiveDiscountFactors(*retval.curve_, spec.today_, futureKnots);
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

    AnalyticEligibilityReport_ ValidateSingleCurveAnalyticEligibility(const CurveCalibrationSpec_& spec) {
        return SingleAnalyticEligibility(spec.today_, spec.calibrateDiscountCurve_, spec.instruments_);
    }

    Vector_<> ResolveCurveCalibrationInitialGuess(const CurveCalibrationSpec_& finalSpec) {
        REQUIRE(finalSpec.knotPolicy_ == CurveKnotPolicy_::Value_::INPUT, "Initial-guess resolution requires the final INPUT execution spec");
        REQUIRE(std::isfinite(finalSpec.initialGuess_), "Curve calibration initial guess must be finite");
        const CurveDefinition_ definition =
            MakeCurveDefinition(finalSpec.curveName_, finalSpec.ccy_, finalSpec.parameterization_, finalSpec.logDfScheme_, finalSpec.knotDates_,
                                finalSpec.today_, finalSpec.liborBasis_);
        const CurveParameterLayout_ layout = BuildCurveParameterLayout(definition);
        for (int i = 0; i < static_cast<int>(finalSpec.initialGuessPerNode_.size()); ++i) {
            REQUIRE(std::isfinite(finalSpec.initialGuessPerNode_[i]),
                    String_("Curve calibration per-node initial guess must be finite at index ") + String::FromInt(i));
        }
        if (!finalSpec.initialGuessPerNode_.empty()) {
            REQUIRE(static_cast<int>(finalSpec.initialGuessPerNode_.size()) == layout.parameterCount_,
                    "Curve calibration per-node initial guess length must match the curve parameter count");
        }
        return BuildCalibrationGuess(finalSpec, definition, layout);
    }

    CurveCalibrationResult_ CalibrateYieldCurve(const CurveCalibrationSpec_& spec) { return CalibrateYieldCurve(spec, CurveCalibrationOptions_()); }

    namespace {
        Vector_<int> ResidualRowsByInput(const Vector_<Handle_<YCInstrument_>>& input, const Vector_<Handle_<YCInstrument_>>& ordered) {
            // Use the exact solver ordering, including equivalent sort keys and repeated handles.
            std::map<const YCInstrument_*, Vector_<int>> rows;
            for (int row = static_cast<int>(ordered.size()); row-- > 0;)
                rows[ordered[row].get()].push_back(row);
            Vector_<int> result;
            result.reserve(input.size());
            for (const auto& instrument : input) {
                auto& matches = rows.at(instrument.get());
                result.push_back(matches.back());
                matches.pop_back();
            }
            return result;
        }

        void SetUnavailable(StagedMultiCurveCalibrationRecord_* record, const String_& stage, const String_& reason) {
            if (record->unavailableReason_.empty() && !reason.empty()) {
                record->unavailableStage_ = stage;
                record->unavailableReason_ = reason;
            }
        }

        String_ StageComponentRejection(const CurveCalibrationSpec_& stage, const StagedMultiCurveCalibrationRecord_& record) {
            if (stage.calibrateDiscountCurve_) {
                if (stage.baseCurve_)
                    return "The discount stage must not depend on an external base curve";
            } else {
                const auto& discount = record.stages_.front();
                const auto route = stage.discountCurves_.find(stage.targetCollateral_);
                if (route == stage.discountCurves_.end() || route->second.get() != discount.curve_.get())
                    return "The forward stage discount route must reference the fitted discount curve";
                if (stage.baseCurve_.get() != discount.curve_.get())
                    return "The forward stage base must reference the fitted discount curve";
            }
            return {};
        }

        bool HasStagedRiskPricingImplementation(const YCInstrument_& instrument) {
            // Subclasses can override passive Precompute while inheriting a different, nonvirtual templated rate.
            const auto& type = typeid(instrument);
            return type == typeid(Deposit_) || type == typeid(FRA_) || type == typeid(STIR_) || type == typeid(Future_) || type == typeid(Swap_) ||
                   type == typeid(OISSwap_);
        }

        String_ InstrumentRiskRejection(const CurveCalibrationSpec_& stage, int parameterCount) {
            for (int i = 0; i < static_cast<int>(stage.instruments_.size()); ++i) {
                const auto& instrument = *stage.instruments_[i];
                if (!HasStagedRiskPricingImplementation(instrument))
                    return String_("Staged risk cannot verify the pricing implementation at input index ") + String::FromInt(i);
                const auto* index = FloatConventionOf(instrument);
                if (index->collateral_ != stage.targetCollateral_)
                    return "Instrument collateral does not match the discount route";
                if (index->useProjectionCurve_ == stage.calibrateDiscountCurve_ ||
                    (index->useProjectionCurve_ && index->forecastTenor_ != stage.targetTenor_))
                    return "Instrument forecast route does not match the stage target";
            }
            if (parameterCount != static_cast<int>(stage.instruments_.size()))
                return "Staged quote risk requires a square quote-by-parameter system";
            return {};
        }

        String_ StageRiskRejection(const MultiCurveCalibrationSpec_& bundle,
                                   const CurveCalibrationSpec_& stage,
                                   const StagedMultiCurveCalibrationRecord_& record) {
            if (stage.ccy_.empty() || (!bundle.ccy_.empty() && bundle.ccy_ != stage.ccy_) ||
                (!record.stages_.empty() && stage.ccy_ != record.stages_.front().spec_.ccy_))
                return "Stages must share the same currency";
            if (stage.solveMode_ != CurveSolveMode_::Value_::EXACT)
                return "Staged quote risk requires EXACT calibration";
            return StageComponentRejection(stage, record);
        }

        CurveCalibrationResult_
        CalibrateStage(const CurveCalibrationSpec_& spec, const CurveCalibrationOptions_& options, StagedMultiCurveCalibrationRecord_* riskRecord) {
            RecordCurveCalibrationInvocation();
            ValidateCurveCalibrationSpec(spec);

            const Vector_<Handle_<YCInstrument_>> instruments = OrderInstruments(spec.instruments_);
            const Vector_<Date_> knotDates = BuildCurveCalibrationKnots(spec.today_, instruments, spec.knotDates_, spec.knotPolicy_);
            const CurveDefinition_ definition =
                MakeCurveDefinition(spec.curveName_, spec.ccy_, spec.parameterization_, spec.logDfScheme_, knotDates, spec.today_, spec.liborBasis_);
            const CurveParameterLayout_ layout = BuildCurveParameterLayout(definition);
            const int paramsPerKnot = layout.paramsPerDeclaredKnot_;
            if (riskRecord && riskRecord->unavailableReason_.empty())
                SetUnavailable(riskRecord, spec.curveName_, InstrumentRiskRejection(spec, layout.parameterCount_));

            const Vector_<> guess = BuildCalibrationGuess(spec, definition, layout);
            const Vector_<> tol(instruments.size(), spec.tolerance_);
            const auto firstFreeKnot = definition.nodeDates_.begin() + (layout.pinnedAnchor_ ? 1 : 0);
            const Vector_<Date_> weightKnots(firstFreeKnot, definition.nodeDates_.end());
            std::unique_ptr<Sparse::TriDiagonal_> weights(BuildCurveCalibrationWeights(weightKnots, paramsPerKnot, spec.smoothingWeight_));

            AnalyticRates_ analyticRates;
            YieldCurveCalibrationFunc_ func(spec.ccy_, spec.curveName_, spec.today_, spec.parameterization_, instruments, knotDates,
                                            spec.discountCurves_, spec.forwardCurves_, spec.baseCurve_, spec.targetCollateral_, spec.targetTenor_,
                                            spec.calibrateDiscountCurve_, spec.liborBasis_, spec.logDfScheme_, options.jacobianMode_, spec.solveMode_,
                                            &analyticRates);

            // Eligibility is cached for ANALYTIC mode. RunCurveCalibration gates the diagnostic on EXACT.
            const bool wantFwdJacobian = options.computeForwardJacobian_ && func.Eligible();
            CurveSolveOutput_ solved =
                RunCurveCalibration(func, guess, tol, spec.solveMode_ == CurveSolveMode_::Value_::EXACT, options.computeEffJacobianInverse_,
                                    wantFwdJacobian, spec.fitTolerance_, *weights, spec.maxEvaluations_, spec.maxRestarts_);
            CurveCalibrationResult_ retval = AssembleCalibrationResult(spec, instruments, definition, knotDates, solved.parameters_,
                                                                       solved.hasEffJacobianInverse_ ? &solved.effJacobianInverse_ : nullptr);
            retval.diagnostics_.jacobian_ = std::move(solved.forwardJacobian_);
            if (riskRecord) {
                if (retval.diagnostics_.effJacobianInverse_.Empty())
                    SetUnavailable(riskRecord, spec.curveName_, "Full rank could not be verified from the solution Jacobian");
                riskRecord->stages_.emplace_back();
                auto& record = riskRecord->stages_.back();
                record.spec_ = spec;
                record.spec_.knotPolicy_ = CurveKnotPolicy_::Value_::INPUT;
                record.spec_.knotDates_ = knotDates;
                record.freeParameters_ = DescribeCurveFreeParameters(definition);
                record.solution_ = std::move(solved.parameters_);
                record.residualRowByInput_ = ResidualRowsByInput(spec.instruments_, instruments);
            }
            return retval;
        }

        void PrepareRecordedStage(const MultiCurveCalibrationSpec_& bundle,
                                  const MultiCurveCalibrationResult_& fitted,
                                  CurveCalibrationSpec_* stage,
                                  StagedMultiCurveCalibrationRecord_* record) {
            if (stage->calibrateDiscountCurve_ != fitted.diagnostics_.empty())
                SetUnavailable(record, stage->curveName_.empty() ? bundle.name_ : stage->curveName_,
                               "Staged quote risk requires a discount stage followed by a forward stage");
            ApplyStageDefaults(bundle, fitted, stage);
            if (record->unavailableReason_.empty())
                SetUnavailable(record, stage->curveName_, StageRiskRejection(bundle, *stage, *record));
        }

        void
        StoreStageFailure(StagedMultiCurveCalibrationRecord_* record, const String_& name, const std::exception& error, bool previouslyUnavailable) {
            const char* message = error.what();
            if (*message == '\0')
                message = "Calibration stage failed without an exception message";
            if (previouslyUnavailable || record->unavailableReason_.empty()) {
                record->unavailableStage_ = name;
                record->unavailableReason_ = message;
            } else {
                record->unavailableReason_ += String_("; ") + message;
            }
        }
    } // namespace

    CurveCalibrationResult_ CalibrateYieldCurve(const CurveCalibrationSpec_& spec, const CurveCalibrationOptions_& options) {
        return CalibrateStage(spec, options, nullptr);
    }

    MultiCurveCalibrationResult_ CalibrateMultiCurve(const MultiCurveCalibrationSpec_& spec) {
        REQUIRE(!spec.stages_.empty(), "Multi-curve calibration requires at least one stage");
        MultiCurveCalibrationResult_ result;
        for (auto stage : spec.stages_) {
            ApplyStageDefaults(spec, result, &stage);
            auto fitted = CalibrateYieldCurve(stage);
            StoreStageResult(stage, &fitted, &result);
        }
        return result;
    }

    MultiCurveCalibrationResult_ CalibrateMultiCurve(const MultiCurveCalibrationSpec_& spec, StagedMultiCurveCalibrationRecord_* riskRecord) {
        if (!riskRecord)
            return CalibrateMultiCurve(spec);
        MultiCurveCalibrationResult_ retval;
        *riskRecord = {};
        if (spec.stages_.size() != 2)
            SetUnavailable(riskRecord, spec.name_, "Staged quote risk requires exactly two stages");
        for (auto stageSpec : spec.stages_) {
            const bool previouslyUnavailable = !riskRecord->unavailableReason_.empty();
            try {
                PrepareRecordedStage(spec, retval, &stageSpec, riskRecord);
                auto stageResult = CalibrateStage(stageSpec, CurveCalibrationOptions_(), riskRecord);
                riskRecord->stages_.back().curve_ = StoreStageResult(stageSpec, &stageResult, &retval);
            } catch (const std::bad_alloc&) {
                throw;
            } catch (const std::exception& e) {
                StoreStageFailure(riskRecord, stageSpec.curveName_, e, previouslyUnavailable);
                break;
            }
        }
        return retval;
    }

} // namespace Dal

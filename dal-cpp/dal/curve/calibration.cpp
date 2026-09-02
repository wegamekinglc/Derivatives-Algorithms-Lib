//
// Created by wegam on 2026/5/9.
//

#include <algorithm>
#include <atomic>
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
        std::atomic<int> calibrationInvocationCount{0};

        void AddAnalyticIssue(AnalyticEligibilityReport_* report,
                              AnalyticIneligibilityReason_ reason,
                              const String_& group,
                              int declarationIndex,
                              int instrumentIndex,
                              int resetIndex,
                              const String_& message) {
            AnalyticEligibilityIssue_ issue;
            issue.reason_ = reason;
            issue.group_ = group;
            issue.declarationIndex_ = declarationIndex;
            issue.instrumentIndex_ = instrumentIndex;
            issue.resetIndex_ = resetIndex;
            issue.nativeMessage_ = message;
            report->issues_.push_back(issue);
            report->eligible_ = false;
        }

        bool HasTemplatedSingleRate(const YCInstrument_& instrument) {
            return dynamic_cast<const Deposit_*>(&instrument) || dynamic_cast<const FRA_*>(&instrument) ||
                   dynamic_cast<const Future_*>(&instrument) || dynamic_cast<const Swap_*>(&instrument);
        }

        AnalyticEligibilityReport_
        SingleAnalyticEligibility(const Date_& anchor, bool calibrateDiscountCurve, const Vector_<Handle_<YCInstrument_>>& instruments) {
            AnalyticEligibilityReport_ report;
            if (!calibrateDiscountCurve) {
                AddAnalyticIssue(&report, AnalyticIneligibilityReason_::Value_::DISCOUNT_TARGET_REQUIRED, "single", -1, -1, -1,
                                 "AAD Jacobian requires a discount-target declaration");
            }
            for (int i = 0; i < static_cast<int>(instruments.size()); ++i) {
                const YCInstrument_* instrument = instruments[i].get();
                if (!instrument || !HasTemplatedSingleRate(*instrument)) {
                    AddAnalyticIssue(&report, AnalyticIneligibilityReason_::Value_::TEMPLATED_RATE_UNAVAILABLE, "single", -1, i, -1,
                                     "AAD Jacobian has no templated rate for the instrument");
                    continue;
                }
                const RateIndexConvention_* convention = FloatConventionOf(*instrument);
                if (!convention) {
                    AddAnalyticIssue(&report, AnalyticIneligibilityReason_::Value_::TEMPLATED_RATE_UNAVAILABLE, "single", -1, i, -1,
                                     "AAD Jacobian has no floating-rate convention for the instrument");
                    continue;
                }
                if (convention->useProjectionCurve_) {
                    AddAnalyticIssue(&report, AnalyticIneligibilityReason_::Value_::PROJECTION_NOT_ALLOWED, "single", -1, i, -1,
                                     "AAD Jacobian requires forecast and discount routing to coincide");
                }
                if (instrument->TradeDate() != anchor) {
                    AddAnalyticIssue(&report, AnalyticIneligibilityReason_::Value_::TRADE_DATE_MISMATCH, "single", -1, i, -1,
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

        void
        StoreStageResult(const CurveCalibrationSpec_& stageSpec, CurveCalibrationResult_* stageResult, MultiCurveCalibrationResult_* multiResult) {
            Handle_<DiscountCurve_> calibrated(stageResult->curve_.release());
            if (stageSpec.calibrateDiscountCurve_)
                multiResult->discountCurves_[stageSpec.targetCollateral_] = calibrated;
            else
                multiResult->forwardCurves_[stageSpec.targetTenor_] = calibrated;
            multiResult->diagnostics_.push_back(stageResult->diagnostics_);
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
                                       CurveSolveMode_ solveMode)
                : ccy_(ccy), curveName_(curveName), anchor_(anchor),
                  definition_(MakeCurveDefinition(curveName, ccy, parameterization, logDfScheme, knotDates, anchor, liborBasis)),
                  instruments_(instruments), discountCurves_(discountCurves), forwardCurves_(forwardCurves), baseCurve_(baseCurve),
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

            template <class T_> [[nodiscard]] Handle_<Tape::Rate_<T_>> PhaseARateAt(int i) const {
                return VisitRate(
                    *instruments_[i], [](const Deposit_& d) { return d.PrecomputeT<T_>(); }, [](const FRA_& f) { return f.PrecomputeT<T_>(); },
                    [](const Future_& fu) { return fu.PrecomputeT<T_>(); }, [](const Swap_& s) { return s.PrecomputeT<T_>(); });
            }
        };

        std::unique_ptr<Underdetermined::Jacobian_> YieldCurveCalibrationFunc_::AnalyticJacobian(const Vector_<>& x, const Vector_<>& f) const {
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

            return std::make_unique<XCurveJacobian_>(HarvestCurveJacobian(*tape, parameters, residuals));
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
            Vector_<> guess(layout.parameterCount_);
            if (!spec.initialGuessPerNode_.empty()) {
                std::copy(spec.initialGuessPerNode_.begin(), spec.initialGuessPerNode_.end(), guess.begin());
            } else if (definition.parameterization_ == CurveParameterization_::Value_::LOG_DISCOUNT) {
                // initialGuess_ is an annualized continuously-compounded rate.
                for (int i = 1; i < static_cast<int>(definition.nodeDates_.size()); ++i)
                    guess[i - 1] = -spec.initialGuess_ * spec.liborBasis_(definition.anchorDate_, definition.nodeDates_[i], nullptr);
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
            SolverOutput_ out;
            out.result_ =
                RunCurveSolver(func, guess, tol, spec.solveMode_ == CurveSolveMode_::Value_::EXACT, spec.fitTolerance_, weights, spec.maxEvaluations_,
                               spec.maxRestarts_, computeEffJacobianInverse ? &out.effJacobianInverse_ : nullptr, fwdJacobian);
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

    CurveCalibrationResult_ CalibrateYieldCurve(const CurveCalibrationSpec_& spec, const CurveCalibrationOptions_& options) {
        RecordCurveCalibrationInvocation();
        ValidateCurveCalibrationSpec(spec);

        const Vector_<Handle_<YCInstrument_>> instruments = OrderInstruments(spec.instruments_);
        const Vector_<Date_> knotDates = BuildCurveCalibrationKnots(spec.today_, instruments, spec.knotDates_, spec.knotPolicy_);
        const CurveDefinition_ definition =
            MakeCurveDefinition(spec.curveName_, spec.ccy_, spec.parameterization_, spec.logDfScheme_, knotDates, spec.today_, spec.liborBasis_);
        const CurveParameterLayout_ layout = BuildCurveParameterLayout(definition);
        const int paramsPerKnot = layout.paramsPerDeclaredKnot_;

        const Vector_<> guess = BuildCalibrationGuess(spec, definition, layout);
        const Vector_<> tol(instruments.size(), spec.tolerance_);
        Vector_<Date_> weightKnots;
        if (layout.pinnedAnchor_) {
            // Pinned representations store an internal anchor but expose only the free future nodes.
            for (int i = 1; i < static_cast<int>(definition.nodeDates_.size()); ++i)
                weightKnots.push_back(definition.nodeDates_[i]);
        } else {
            weightKnots = definition.nodeDates_;
        }
        std::unique_ptr<Sparse::TriDiagonal_> weights(BuildCurveCalibrationWeights(weightKnots, paramsPerKnot, spec.smoothingWeight_));

        YieldCurveCalibrationFunc_ func(spec.ccy_, spec.curveName_, spec.today_, spec.parameterization_, instruments, knotDates, spec.discountCurves_,
                                        spec.forwardCurves_, spec.baseCurve_, spec.targetCollateral_, spec.targetTenor_, spec.calibrateDiscountCurve_,
                                        spec.liborBasis_, spec.logDfScheme_, options.jacobianMode_, spec.solveMode_);

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

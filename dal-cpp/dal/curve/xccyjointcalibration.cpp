//
// Created by Codex on 2026/7/14.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>

#include <algorithm>
#include <cmath>
#include <memory>
#include <set>
#include <type_traits>
#include <utility>

#include <dal/curve/aadjacobian.hpp>
#include <dal/curve/curvejacobian.hpp>
#include <dal/curve/jointcalibration_internal.hpp>
#include <dal/curve/tapeguard.hpp>
#include <dal/curve/xccyjointcalibration.hpp>
#include <dal/curve/xccypricing.hpp>
#include <dal/math/aad/aad.hpp>
#include <dal/math/matrix/banded.hpp>
#include <dal/math/optimization/underdetermined.hpp>
#include <dal/utilities/dictionary.hpp>

namespace Dal {
    namespace {
        using JointCalibrationInternal::CurveCollectionSpec_;
        using JointCalibrationInternal::CurveSlot_;

        constexpr const char* KEY_MAX_EVALUATIONS = "MAXEVALUATIONS";
        constexpr const char* KEY_MAX_RESTARTS = "MAXRESTARTS";

        String_ PairName(const CurrencyPair_& pair) { return String_(pair.domestic_.String()) + "/" + pair.foreign_.String(); }

        String_ TenorName(const PeriodLength_& tenor) { return String::FromInt(tenor.Months()) + "M (" + tenor.String() + ")"; }

        CurveCollectionSpec_ CurrencyCollection(const JointXccyCalibrationSpec_& spec, bool domestic) {
            const JointCurrencyCurveSpec_& currency = domestic ? spec.domestic_ : spec.foreign_;
            CurveCollectionSpec_ result;
            result.today_ = spec.valuationTime_.Date();
            result.ccy_ = currency.ccy_.String();
            result.liborBasis_ = currency.liborBasis_;
            result.curves_ = &currency.curves_;
            result.context_ = String_(domestic ? "Domestic " : "Foreign ") + result.ccy_ + " joint calibration";
            result.declarationLabel_ = String_(domestic ? "Domestic slot" : "Foreign slot");
            result.requireCurveNames_ = true;
            return result;
        }

        CurveDefinition_ BasisDefinition(const JointXccyCalibrationSpec_& spec) {
            return MakeCurveDefinition(spec.basis_.curveName_, spec.pair_.domestic_.String(), spec.basis_.parameterization_, spec.basis_.logDfScheme_,
                                       spec.basis_.knotDates_, spec.valuationTime_.Date(), DayBasis_("ACT_365F"));
        }

        String_ DiscountSlotName(const JointCurrencyCurveSpec_& currency, const CollateralType_& collateral) {
            for (const auto& declaration : currency.curves_)
                if (declaration.calibrateDiscountCurve_ && declaration.targetCollateral_ == collateral)
                    return declaration.curveName_;
            return String_();
        }

        String_ ForwardSlotName(const JointCurrencyCurveSpec_& currency, const PeriodLength_& tenor) {
            for (const auto& declaration : currency.curves_)
                if (!declaration.calibrateDiscountCurve_ && declaration.targetTenor_ == tenor)
                    return declaration.curveName_;
            return String_();
        }

        void ValidatePlanSlots(const JointXccyCalibrationSpec_& spec, const XccyCashflowPlan_& plan, int instrumentIndex) {
            const String_ instrument = String_("XCCY instrument ") + String::FromInt(instrumentIndex) + " for pair " + PairName(spec.pair_);
            REQUIRE(plan.config_.pair_ == spec.pair_, instrument + " has mismatched pair " + PairName(plan.config_.pair_));
            const auto& domesticIndex = plan.config_.convention_.domesticIndex_;
            const auto& foreignIndex = plan.config_.convention_.foreignIndex_;
            REQUIRE(!DiscountSlotName(spec.domestic_, domesticIndex.collateral_).empty(),
                    instrument + " requires missing domestic discount slot " + domesticIndex.collateral_.String());
            REQUIRE(!DiscountSlotName(spec.foreign_, foreignIndex.collateral_).empty(),
                    instrument + " requires missing foreign discount slot " + foreignIndex.collateral_.String());
            if (domesticIndex.useProjectionCurve_)
                REQUIRE(!ForwardSlotName(spec.domestic_, domesticIndex.forecastTenor_).empty(),
                        instrument + " requires missing domestic forward slot " + TenorName(domesticIndex.forecastTenor_));
            if (foreignIndex.useProjectionCurve_)
                REQUIRE(!ForwardSlotName(spec.foreign_, foreignIndex.forecastTenor_).empty(),
                        instrument + " requires missing foreign forward slot " + TenorName(foreignIndex.forecastTenor_));

            const Vector_<XccyCouponPeriod_>& spreadPeriods =
                plan.config_.convention_.spreadOnForeignLeg_ ? plan.foreignPeriods_ : plan.domesticPeriods_;
            bool hasRemainingAnnuity = false;
            for (const auto& period : spreadPeriods) {
                if (period.schedule_.paymentDate_ >= spec.valuationTime_.Date() && period.accrual_.dcf_ > 0.0) {
                    hasRemainingAnnuity = true;
                    break;
                }
            }
            REQUIRE(hasRemainingAnnuity,
                    instrument + String_(plan.config_.convention_.spreadOnForeignLeg_ ? " has no remaining foreign spread annuity"
                                                                                      : " has no remaining domestic spread annuity"));
        }

        Vector_<XccyCashflowPlan_> ValidateAndBuildPlans(const JointXccyCalibrationSpec_& spec) {
            REQUIRE(!spec.basis_.instruments_.empty(),
                    "Joint XCCY calibration requires a non-empty XCCY instrument group for " + PairName(spec.pair_));
            Vector_<XccyCashflowPlan_> result;
            result.reserve(spec.basis_.instruments_.size());
            for (int i = 0; i < static_cast<int>(spec.basis_.instruments_.size()); ++i) {
                REQUIRE(spec.basis_.instruments_[i], String_("Joint XCCY calibration has an empty XCCY instrument at index ") + String::FromInt(i));
                const auto span = spec.basis_.instruments_[i]->TimeSpan();
                result.push_back(BuildXccyCashflowPlan(span.first, span.second, spec.basis_.instruments_[i]->Config()));
                ValidatePlanSlots(spec, result.back(), i);
            }
            return result;
        }

        void ValidateTopLevelSpec(const JointXccyCalibrationSpec_& spec) {
            REQUIRE(spec.valuationTime_.IsValid(), "Joint XCCY calibration requires a valid valuation time");
            REQUIRE(spec.pair_.domestic_ == spec.domestic_.ccy_, String_("Joint XCCY pair domestic currency ") + spec.pair_.domestic_.String() +
                                                                     " does not match domestic curve currency " + spec.domestic_.ccy_.String());
            REQUIRE(spec.pair_.foreign_ == spec.foreign_.ccy_, String_("Joint XCCY pair foreign currency ") + spec.pair_.foreign_.String() +
                                                                   " does not match foreign curve currency " + spec.foreign_.ccy_.String());
            REQUIRE(spec.collateralCurrency_ == spec.pair_.domestic_,
                    "Joint XCCY pair " + PairName(spec.pair_) + " supports domestic collateral only, not " + spec.collateralCurrency_.String());
            REQUIRE(std::isfinite(spec.fxSpot_) && spec.fxSpot_ > 0.0,
                    "Joint XCCY pair " + PairName(spec.pair_) + " requires a positive finite FX spot");
            REQUIRE(spec.tolerance_ > 0.0 && spec.fitTolerance_ > 0.0, "Joint XCCY calibration tolerances must be positive");
            REQUIRE(std::isfinite(spec.initialGuess_), "Joint XCCY calibration initial guess must be finite");
            REQUIRE(spec.maxEvaluations_ > 0 && spec.maxRestarts_ > 0, "Joint XCCY calibration iteration caps must be positive");
            REQUIRE(!spec.basis_.curveName_.empty(), "Joint XCCY calibration requires a named basis declaration");
            REQUIRE(!spec.basis_.knotDates_.empty(), "XCCY basis slot '" + spec.basis_.curveName_ + "' requires at least one knot date");
            REQUIRE(spec.basis_.knotDates_.front() > spec.valuationTime_.Date(),
                    "XCCY basis slot '" + spec.basis_.curveName_ + "' knot dates must be after the valuation date");
            for (int i = 1; i < static_cast<int>(spec.basis_.knotDates_.size()); ++i)
                REQUIRE(spec.basis_.knotDates_[i] > spec.basis_.knotDates_[i - 1],
                        "XCCY basis slot '" + spec.basis_.curveName_ + "' knot dates must be strictly increasing");
            REQUIRE(spec.basis_.smoothingWeight_ > 0.0, "XCCY basis slot '" + spec.basis_.curveName_ + "' smoothing weight must be positive");
        }

        Handle_<MarketFixingSnapshot_> ResolveFixings(const JointXccyCalibrationSpec_& spec, const Vector_<XccyCashflowPlan_>& plans) {
            if (spec.fixings_)
                return spec.fixings_;
            Vector_<FixingRequest_> requests;
            for (const auto& plan : plans) {
                const Vector_<FixingRequest_> required = RequiredHistoricalFixings(plan, spec.valuationTime_);
                requests.Append(required);
            }
            return SnapshotGlobalFixings(requests);
        }

        struct JointLayout_ {
            CurveCollectionSpec_ domesticCollection_;
            CurveCollectionSpec_ foreignCollection_;
            std::vector<CurveSlot_> domesticSlots_;
            std::vector<CurveSlot_> foreignSlots_;
            CurveDefinition_ basisDefinition_;
            CurveParameterLayout_ basisLayout_;
            CurveSlot_ basisSlot_;
            int totalParameters_ = 0;
            int totalResiduals_ = 0;
        };

        JointLayout_ BuildLayout(const JointXccyCalibrationSpec_& spec) {
            JointLayout_ result;
            result.domesticCollection_ = CurrencyCollection(spec, true);
            result.foreignCollection_ = CurrencyCollection(spec, false);
            result.domesticSlots_ = JointCalibrationInternal::ValidateAndBuildSlots(result.domesticCollection_);
            int parameterOffset = 0;
            int residualOffset = 0;
            for (const auto& slot : result.domesticSlots_) {
                parameterOffset = std::max(parameterOffset, slot.paramOffset_ + slot.nParams_);
                residualOffset = std::max(residualOffset, slot.residualOffset_ + slot.nInstruments_);
            }
            result.foreignSlots_ = JointCalibrationInternal::ValidateAndBuildSlots(result.foreignCollection_, parameterOffset, residualOffset);
            for (const auto& slot : result.foreignSlots_) {
                parameterOffset = std::max(parameterOffset, slot.paramOffset_ + slot.nParams_);
                residualOffset = std::max(residualOffset, slot.residualOffset_ + slot.nInstruments_);
            }

            result.basisDefinition_ = BasisDefinition(spec);
            result.basisLayout_ = BuildCurveParameterLayout(result.basisDefinition_);
            result.basisSlot_.paramOffset_ = parameterOffset;
            result.basisSlot_.nParams_ = result.basisLayout_.parameterCount_;
            result.basisSlot_.residualOffset_ = residualOffset;
            result.basisSlot_.nInstruments_ = static_cast<int>(spec.basis_.instruments_.size());
            result.basisSlot_.smoothingWeight_ = spec.basis_.smoothingWeight_;
            result.basisSlot_.definition_ = result.basisDefinition_;
            result.basisSlot_.layout_ = result.basisLayout_;
            REQUIRE(result.basisSlot_.nParams_ > 0, "XCCY basis slot '" + spec.basis_.curveName_ + "' requires at least one parameter");
            if (!spec.basis_.initialGuessPerNode_.empty())
                REQUIRE(static_cast<int>(spec.basis_.initialGuessPerNode_.size()) == result.basisSlot_.nParams_,
                        "XCCY basis slot '" + spec.basis_.curveName_ + "' initialGuessPerNode_ length must equal its parameter count");
            result.totalParameters_ = parameterOffset + result.basisSlot_.nParams_;
            result.totalResiduals_ = residualOffset + result.basisSlot_.nInstruments_;
            return result;
        }

        void AddEligibilityIssue(AnalyticEligibilityReport_* report,
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

        void ValidateCurrencyEligibility(const JointCurrencyCurveSpec_& currency,
                                         const String_& group,
                                         const Date_& anchor,
                                         AnalyticEligibilityReport_* report) {
            if (currency.liborBasis_.String() != String_("ACT_365F")) {
                const String_ label = group == String_("domestic") ? String_("Domestic") : String_("Foreign");
                AddEligibilityIssue(report, AnalyticIneligibilityReason_::Value_::LIBOR_BASIS_UNSUPPORTED, group, -1, -1, -1,
                                    label + " requires ACT_365F libor basis for an analytic Jacobian");
            }
            for (int declarationIndex = 0; declarationIndex < static_cast<int>(currency.curves_.size()); ++declarationIndex) {
                const JointCurveDeclaration_& declaration = currency.curves_[declarationIndex];
                for (int instrumentIndex = 0; instrumentIndex < static_cast<int>(declaration.instruments_.size()); ++instrumentIndex) {
                    const auto& handle = declaration.instruments_[instrumentIndex];
                    const YCInstrument_* instrument = handle.get();
                    if (!instrument || !JointCalibrationInternal::SupportedInstrumentType(*instrument)) {
                        AddEligibilityIssue(report, AnalyticIneligibilityReason_::Value_::TEMPLATED_RATE_UNAVAILABLE, group, declarationIndex,
                                            instrumentIndex, -1, "instrument has no templated rate implementation");
                        continue;
                    }
                    const RateIndexConvention_* convention = FloatConventionOf(*instrument);
                    if (!convention) {
                        AddEligibilityIssue(report, AnalyticIneligibilityReason_::Value_::TEMPLATED_RATE_UNAVAILABLE, group, declarationIndex,
                                            instrumentIndex, -1, "instrument has no floating-rate convention");
                        continue;
                    }
                    if (declaration.calibrateDiscountCurve_ && convention->useProjectionCurve_) {
                        AddEligibilityIssue(report, AnalyticIneligibilityReason_::Value_::PROJECTION_NOT_ALLOWED, group, declarationIndex,
                                            instrumentIndex, -1, "discount declaration cannot project from a forward slot");
                    }
                    if (!declaration.calibrateDiscountCurve_ && !convention->useProjectionCurve_) {
                        AddEligibilityIssue(report, AnalyticIneligibilityReason_::Value_::PROJECTION_REQUIRED, group, declarationIndex,
                                            instrumentIndex, -1, "forward declaration requires projection routing");
                    }
                    if (instrument->TradeDate() != anchor) {
                        AddEligibilityIssue(report, AnalyticIneligibilityReason_::Value_::TRADE_DATE_MISMATCH, group, declarationIndex,
                                            instrumentIndex, -1, "instrument trade date does not equal the calibration anchor");
                    }
                }
            }
        }

        void ValidateJointXccyPlan(const JointXccyCalibrationSpec_& spec,
                                   const JointLayout_& layout,
                                   const XccyCashflowPlan_& plan,
                                   int instrumentIndex,
                                   AnalyticEligibilityReport_* report) {
            if (!(plan.config_.pair_ == spec.pair_) || layout.domesticCollection_.ccy_ != plan.config_.pair_.domestic_.String() ||
                layout.foreignCollection_.ccy_ != plan.config_.pair_.foreign_.String()) {
                AddEligibilityIssue(report, AnalyticIneligibilityReason_::Value_::PAIR_CURRENCY_MISMATCH, "basis", -1, instrumentIndex, -1,
                                    "XCCY instrument pair does not match the typed curve collections");
            }
            if (plan.domesticPeriods_.empty() || plan.foreignPeriods_.empty()) {
                AddEligibilityIssue(report, AnalyticIneligibilityReason_::Value_::COUPON_PLAN_EMPTY, "basis", -1, instrumentIndex, -1,
                                    "typed pricing requires coupon periods on both legs");
            }
            switch (plan.config_.notionalMode_.Switch()) {
            case XccyNotionalMode_::Value_::FIXED:
                break;
            case XccyNotionalMode_::Value_::RESETTABLE:
            case XccyNotionalMode_::Value_::MARK_TO_MARKET:
                for (int reset = 0; reset < static_cast<int>(plan.resets_.size()); ++reset) {
                    if (plan.resets_[reset].domesticPeriodIndex_ != reset + 1 ||
                        plan.resets_[reset].domesticPeriodIndex_ >= static_cast<int>(plan.domesticPeriods_.size())) {
                        AddEligibilityIssue(report, AnalyticIneligibilityReason_::Value_::RESET_MAPPING_INVALID, "basis", -1, instrumentIndex, reset,
                                            "reset event has an invalid domestic-period mapping");
                    }
                }
                break;
            default:
                AddEligibilityIssue(report, AnalyticIneligibilityReason_::Value_::NOTIONAL_MODE_UNSUPPORTED, "basis", -1, instrumentIndex, -1,
                                    "typed pricing does not support the notional mode");
                break;
            }
            const auto& domesticIndex = plan.config_.convention_.domesticIndex_;
            if (!JointCalibrationInternal::HasTypedDiscountRoute(layout.domesticCollection_, layout.domesticSlots_, domesticIndex.collateral_)) {
                AddEligibilityIssue(report, AnalyticIneligibilityReason_::Value_::DISCOUNT_ROUTE_MISSING, "basis", -1, instrumentIndex, -1,
                                    "domestic discount route is absent");
            }
            if (domesticIndex.useProjectionCurve_ &&
                !JointCalibrationInternal::HasTypedForwardRoute(layout.domesticCollection_, layout.domesticSlots_, domesticIndex)) {
                AddEligibilityIssue(report, AnalyticIneligibilityReason_::Value_::PROJECTION_ROUTE_MISSING, "basis", -1, instrumentIndex, -1,
                                    "domestic projection route is absent");
            }
            const auto& foreignIndex = plan.config_.convention_.foreignIndex_;
            if (!JointCalibrationInternal::HasTypedDiscountRoute(layout.foreignCollection_, layout.foreignSlots_, foreignIndex.collateral_)) {
                AddEligibilityIssue(report, AnalyticIneligibilityReason_::Value_::DISCOUNT_ROUTE_MISSING, "basis", -1, instrumentIndex, -1,
                                    "foreign discount route is absent");
            }
            if (foreignIndex.useProjectionCurve_ &&
                !JointCalibrationInternal::HasTypedForwardRoute(layout.foreignCollection_, layout.foreignSlots_, foreignIndex)) {
                AddEligibilityIssue(report, AnalyticIneligibilityReason_::Value_::PROJECTION_ROUTE_MISSING, "basis", -1, instrumentIndex, -1,
                                    "foreign projection route is absent");
            }
        }

        AnalyticEligibilityReport_
        JointAnalyticEligibility(const JointXccyCalibrationSpec_& spec, const JointLayout_& layout, const Vector_<XccyCashflowPlan_>& plans) {
            AnalyticEligibilityReport_ report;
            const Date_ anchor = spec.valuationTime_.Date();
            ValidateCurrencyEligibility(spec.domestic_, "domestic", anchor, &report);
            ValidateCurrencyEligibility(spec.foreign_, "foreign", anchor, &report);
            for (int i = 0; i < static_cast<int>(plans.size()); ++i)
                ValidateJointXccyPlan(spec, layout, plans[i], i, &report);
            return report;
        }

        class JointXccyResidualFunction_ : public Underdetermined::Function_ {
            const JointXccyCalibrationSpec_* spec_;
            const JointLayout_* layout_;
            const Vector_<XccyCashflowPlan_>* plans_;
            Handle_<MarketFixingSnapshot_> fixings_;
            CurveJacobianMode_ jacobianMode_;
            int* evaluationCount_;

            template <class T_> Vector_<T_> Residuals(const Vector_<T_>& parameters) const {
                auto domestic = JointCalibrationInternal::BuildTypedCurveBlock<T_>(layout_->domesticCollection_, layout_->domesticSlots_, parameters);
                auto foreign = JointCalibrationInternal::BuildTypedCurveBlock<T_>(layout_->foreignCollection_, layout_->foreignSlots_, parameters);
                const auto basis =
                    BuildDiscountCurveT<T_>(layout_->basisDefinition_, JointCalibrationInternal::SliceParameters(parameters, layout_->basisSlot_));

                Vector_<T_> result(layout_->totalResiduals_);
                if constexpr (std::is_same_v<T_, double>) {
                    const Handle_<CurveBlock_> domesticBlock = JointCalibrationInternal::ReferenceCurveBlock(layout_->domesticCollection_, domestic);
                    const Handle_<CurveBlock_> foreignBlock = JointCalibrationInternal::ReferenceCurveBlock(layout_->foreignCollection_, foreign);
                    JointCalibrationInternal::AppendDoubleResiduals(layout_->domesticSlots_, *domesticBlock, &result);
                    JointCalibrationInternal::AppendDoubleResiduals(layout_->foreignSlots_, *foreignBlock, &result);
                } else {
                    JointCalibrationInternal::AppendTemplatedResiduals(layout_->domesticSlots_, domestic.block_, &result);
                    JointCalibrationInternal::AppendTemplatedResiduals(layout_->foreignSlots_, foreign.block_, &result);
                }
                for (int i = 0; i < static_cast<int>(plans_->size()); ++i) {
                    XccyMarketView_<T_> market;
                    market.valuationTime_ = spec_->valuationTime_;
                    market.pair_ = spec_->pair_;
                    market.collateralCurrency_ = spec_->collateralCurrency_;
                    market.fxSpot_ = T_(spec_->fxSpot_);
                    market.domestic_ = &domestic.block_;
                    market.foreign_ = &foreign.block_;
                    market.basis_ = basis.get();
                    result[layout_->basisSlot_.residualOffset_ + i] =
                        PriceXccyParSpread<T_>((*plans_)[i], market, *fixings_) - T_(spec_->basis_.instruments_[i]->MarketRate());
                }
                return result;
            }

        public:
            JointXccyResidualFunction_(const JointXccyCalibrationSpec_& spec,
                                       const JointLayout_& layout,
                                       const Vector_<XccyCashflowPlan_>& plans,
                                       const Handle_<MarketFixingSnapshot_>& fixings,
                                       CurveJacobianMode_ jacobianMode,
                                       int* evaluationCount)
                : spec_(&spec), layout_(&layout), plans_(&plans), fixings_(fixings), jacobianMode_(jacobianMode), evaluationCount_(evaluationCount) {}

            [[nodiscard]] Vector_<> F(const Vector_<>& parameters) const override {
                ++*evaluationCount_;
                return Residuals<double>(parameters);
            }

            [[nodiscard]] std::unique_ptr<Underdetermined::Jacobian_> Gradient(const Vector_<>& parameters, const Vector_<>&) const override {
                if (jacobianMode_ != CurveJacobianMode_::Value_::ANALYTIC)
                    return nullptr;
                auto* tape = Dal::AAD::Tape();
                TapeGuard_ guard(tape);
                Vector_<Dal::AAD::Number_> activeParameters = RegisterCurveParameters(parameters);
                Dal::AAD::NewRecording(*tape);
                Vector_<Dal::AAD::Number_> residuals = Residuals<Dal::AAD::Number_>(activeParameters);
                return std::make_unique<XCurveJacobian_>(HarvestCurveJacobian(*tape, activeParameters, residuals));
            }
        };

        Vector_<> BuildInitialGuess(const JointXccyCalibrationSpec_& spec, const JointLayout_& layout) {
            Vector_<> result(layout.totalParameters_, spec.initialGuess_);
            auto appendCurrency = [&](const CurveCollectionSpec_& collection, const std::vector<CurveSlot_>& slots) {
                for (const auto& slot : slots) {
                    const JointCurveDeclaration_& declaration = (*collection.curves_)[slot.curveIndex_];
                    const Vector_<> slice = JointCalibrationInternal::BuildGuessSlice(
                        declaration, slot.definition_, spec.initialGuess_, JointCalibrationInternal::SlotName(collection, slot.curveIndex_));
                    for (int i = 0; i < slot.nParams_; ++i)
                        result[slot.paramOffset_ + i] = slice[i];
                }
            };
            appendCurrency(layout.domesticCollection_, layout.domesticSlots_);
            appendCurrency(layout.foreignCollection_, layout.foreignSlots_);
            const Vector_<> basisGuess = JointCalibrationInternal::BuildGuessSlice(spec.basis_, layout.basisDefinition_, spec.initialGuess_,
                                                                                   String_("XCCY basis slot '") + spec.basis_.curveName_ + "'");
            for (int i = 0; i < layout.basisSlot_.nParams_; ++i)
                result[layout.basisSlot_.paramOffset_ + i] = basisGuess[i];
            return result;
        }

        std::unique_ptr<Sparse::TriDiagonal_> BuildSmoothing(const JointLayout_& layout) {
            auto result = std::make_unique<Sparse::TriDiagonal_>(layout.totalParameters_);
            JointCalibrationInternal::AddCurveSmoothing(layout.domesticSlots_, result.get());
            JointCalibrationInternal::AddCurveSmoothing(layout.foreignSlots_, result.get());
            JointCalibrationInternal::AddCurveSmoothing({layout.basisSlot_}, result.get());
            return result;
        }

        struct SolveResult_ {
            Vector_<> parameters_;
            Matrix_<> effectiveInverse_;
            Matrix_<> forwardJacobian_;
            bool hasEffectiveInverse_ = false;
            bool approximate_ = false;
        };

        SolveResult_ Solve(const JointXccyCalibrationSpec_& spec,
                           const JointXccyCalibrationOptions_& options,
                           const JointXccyResidualFunction_& function,
                           const Vector_<>& guess,
                           const Sparse::TriDiagonal_& smoothing,
                           int residualCount) {
            Dictionary_ controlsDictionary;
            controlsDictionary.Insert(KEY_MAX_EVALUATIONS, Cell_(static_cast<double>(spec.maxEvaluations_)));
            controlsDictionary.Insert(KEY_MAX_RESTARTS, Cell_(static_cast<double>(spec.maxRestarts_)));
            UnderdeterminedControls_ controls(controlsDictionary);
            const Vector_<> tolerance(residualCount, spec.tolerance_);

            SolveResult_ result;
            result.approximate_ = spec.solveMode_ == CurveSolveMode_::Value_::APPROXIMATE;
            if (result.approximate_) {
                result.parameters_ = Underdetermined::Approximate(function, guess, tolerance, spec.fitTolerance_, smoothing, controls);
                return result;
            }

            result.hasEffectiveInverse_ = options.computeEffJacobianInverse_;
            const bool wantForward = options.computeForwardJacobian_ && options.jacobianMode_ == CurveJacobianMode_::Value_::ANALYTIC;
            std::unique_ptr<Sparse::SymmetricDecomposition_> decomposition(smoothing.DecomposeSymmetric());
            result.parameters_ = Underdetermined::Find(function, guess, tolerance, *decomposition, controls,
                                                       result.hasEffectiveInverse_ ? &result.effectiveInverse_ : nullptr,
                                                       wantForward ? &result.forwardJacobian_ : nullptr);
            return result;
        }

        void AppendRanges(const String_& group,
                          const CurveCollectionSpec_& collection,
                          const std::vector<CurveSlot_>& slots,
                          Vector_<CalibrationBlockRange_>* parameterRanges,
                          Vector_<CalibrationBlockRange_>* residualRanges) {
            for (const auto& slot : slots) {
                const String_ name = group + ":" + (*collection.curves_)[slot.curveIndex_].curveName_;
                parameterRanges->push_back({name, slot.paramOffset_, slot.nParams_});
                residualRanges->push_back({name, slot.residualOffset_, slot.nInstruments_});
            }
        }

        CrossCurrencyFxForwardCurve_ BuildFxForwards(const JointXccyCalibrationSpec_& spec,
                                                     const Handle_<CurveBlock_>& domestic,
                                                     const Handle_<CurveBlock_>& foreign,
                                                     const Handle_<DiscountCurve_>& basis,
                                                     const Handle_<MarketFixingSnapshot_>& fixings) {
            CrossCurrencyMarket_ market(domestic, foreign, spec.fxSpot_, spec.valuationTime_, spec.collateralCurrency_, fixings);
            market.SetBasisCurve(basis);
            CrossCurrencyFxForwardCurve_ result;
            result.pair_ = spec.pair_;
            result.dates_ = spec.basis_.knotDates_;
            const CollateralType_ collateral = spec.basis_.instruments_.front()->Config().convention_.domesticIndex_.collateral_;
            for (const auto& date : result.dates_)
                result.forwards_.push_back(market.FxForward(spec.valuationTime_.Date(), date, collateral));
            return result;
        }

        void AppendCurrencyDiagnostics(const JointCurrencyCurveSpec_& currency,
                                       const std::vector<CurveSlot_>& slots,
                                       const CurveBlock_& block,
                                       bool approximate,
                                       Vector_<JointCurveCalibrationDiagnostics_>* diagnostics) {
            for (const auto& slot : slots)
                diagnostics->push_back(JointCalibrationInternal::BuildCurveDiagnostics(currency.curves_[slot.curveIndex_], slot, block, approximate));
        }

        void PopulateRates(const JointXccyCalibrationSpec_& spec,
                           const JointLayout_& layout,
                           const Vector_<>& residuals,
                           Vector_<>* marketRates,
                           Vector_<>* modelRates) {
            *marketRates = Vector_<>(layout.totalResiduals_);
            *modelRates = Vector_<>(layout.totalResiduals_);
            for (const auto& slot : layout.domesticSlots_)
                for (int i = 0; i < slot.nInstruments_; ++i)
                    (*marketRates)[slot.residualOffset_ + i] = slot.marketRates_[i];
            for (const auto& slot : layout.foreignSlots_)
                for (int i = 0; i < slot.nInstruments_; ++i)
                    (*marketRates)[slot.residualOffset_ + i] = slot.marketRates_[i];
            for (int i = 0; i < static_cast<int>(spec.basis_.instruments_.size()); ++i)
                (*marketRates)[layout.basisSlot_.residualOffset_ + i] = spec.basis_.instruments_[i]->MarketRate();
            for (int i = 0; i < layout.totalResiduals_; ++i)
                (*modelRates)[i] = (*marketRates)[i] + residuals[i];
        }

        void PopulateXccyDiagnostics(const JointXccyCalibrationSpec_& spec,
                                     const JointLayout_& layout,
                                     const JointXccyCalibrationResult_& result,
                                     bool approximate,
                                     CrossCurrencyCalibrationDiagnostics_* diagnostics) {
            diagnostics->usedApproximateFit_ = approximate;
            for (int i = 0; i < static_cast<int>(spec.basis_.instruments_.size()); ++i) {
                const int row = layout.basisSlot_.residualOffset_ + i;
                diagnostics->instrumentNames_.push_back(spec.basis_.instruments_[i]->Name());
                diagnostics->marketRates_.push_back(result.marketRates_[row]);
                diagnostics->modelRates_.push_back(result.modelRates_[row]);
                diagnostics->residuals_.push_back(result.residuals_[row]);
            }
            const ResidualStats_ stats = ResidualStats(diagnostics->residuals_);
            diagnostics->maxAbsResidual_ = stats.maxAbsResidual_;
            diagnostics->rmsResidual_ = stats.rmsResidual_;
        }

        void PopulateRanges(const JointXccyCalibrationSpec_& spec, const JointLayout_& layout, JointXccyCalibrationResult_* result) {
            AppendRanges("domestic", layout.domesticCollection_, layout.domesticSlots_, &result->parameterRanges_, &result->residualRanges_);
            AppendRanges("foreign", layout.foreignCollection_, layout.foreignSlots_, &result->parameterRanges_, &result->residualRanges_);
            result->parameterRanges_.push_back(
                {String_("basis:") + spec.basis_.curveName_, layout.basisSlot_.paramOffset_, layout.basisSlot_.nParams_});
            result->residualRanges_.push_back(
                {String_("xccy:") + spec.basis_.curveName_, layout.basisSlot_.residualOffset_, layout.basisSlot_.nInstruments_});
        }

        void PopulateSummaryAndMatrices(int evaluationCount, SolveResult_* solve, JointXccyCalibrationResult_* result) {
            const ResidualStats_ stats = ResidualStats(result->residuals_);
            result->jointMaxAbsResidual_ = stats.maxAbsResidual_;
            result->jointRmsResidual_ = stats.rmsResidual_;
            result->solverEvaluations_ = evaluationCount;
            if (solve->hasEffectiveInverse_)
                result->effJacobianInverse_ = std::move(solve->effectiveInverse_);
            result->jacobianAtSolution_ = std::move(solve->forwardJacobian_);
        }

        JointXccyCalibrationResult_ AssembleResult(const JointXccyCalibrationSpec_& spec,
                                                   const JointLayout_& layout,
                                                   const JointXccyResidualFunction_& function,
                                                   const int* evaluationCount,
                                                   const Handle_<MarketFixingSnapshot_>& fixings,
                                                   SolveResult_* solve) {
            JointXccyCalibrationResult_ result;
            result.usedApproximateFit_ = solve->approximate_;
            result.fixings_ = fixings;
            result.domesticCurveBlock_ =
                JointCalibrationInternal::BuildCurveBlock(layout.domesticCollection_, layout.domesticSlots_, solve->parameters_);
            result.foreignCurveBlock_ =
                JointCalibrationInternal::BuildCurveBlock(layout.foreignCollection_, layout.foreignSlots_, solve->parameters_);
            result.basisCurve_ =
                Handle_<DiscountCurve_>(BuildDiscountCurveUniqueT<double>(
                                            layout.basisDefinition_, JointCalibrationInternal::SliceParameters(solve->parameters_, layout.basisSlot_))
                                            .release());
            result.fxForwardCurve_ = BuildFxForwards(spec, result.domesticCurveBlock_, result.foreignCurveBlock_, result.basisCurve_, fixings);
            result.residuals_ = function.F(solve->parameters_);
            AppendCurrencyDiagnostics(spec.domestic_, layout.domesticSlots_, *result.domesticCurveBlock_, solve->approximate_,
                                      &result.domesticDiagnostics_);
            AppendCurrencyDiagnostics(spec.foreign_, layout.foreignSlots_, *result.foreignCurveBlock_, solve->approximate_,
                                      &result.foreignDiagnostics_);
            PopulateRates(spec, layout, result.residuals_, &result.marketRates_, &result.modelRates_);
            PopulateXccyDiagnostics(spec, layout, result, solve->approximate_, &result.xccyDiagnostics_);
            PopulateSummaryAndMatrices(*evaluationCount, solve, &result);
            PopulateRanges(spec, layout, &result);
            result.converged_ = true;
            return result;
        }
    } // namespace

    AnalyticEligibilityReport_ ValidateJointXccyAnalyticEligibility(const JointXccyCalibrationSpec_& spec) {
        const JointLayout_ layout = BuildLayout(spec);
        const Vector_<XccyCashflowPlan_> plans = ValidateAndBuildPlans(spec);
        return JointAnalyticEligibility(spec, layout, plans);
    }

    JointXccyCalibrationResult_ CalibrateJointXccyMarket(const JointXccyCalibrationSpec_& spec) {
        return CalibrateJointXccyMarket(spec, JointXccyCalibrationOptions_());
    }

    JointXccyCalibrationResult_ CalibrateJointXccyMarket(const JointXccyCalibrationSpec_& spec, const JointXccyCalibrationOptions_& options) {
        ValidateTopLevelSpec(spec);
        REQUIRE(options.jacobianMode_ == CurveJacobianMode_::Value_::ANALYTIC || options.jacobianMode_ == CurveJacobianMode_::Value_::BUMPED,
                "Joint XCCY calibration requires ANALYTIC or BUMPED Jacobian mode");
        const JointLayout_ layout = BuildLayout(spec);
        const Vector_<XccyCashflowPlan_> plans = ValidateAndBuildPlans(spec);
        const Handle_<MarketFixingSnapshot_> fixings = ResolveFixings(spec, plans);
        if (options.jacobianMode_ == CurveJacobianMode_::Value_::ANALYTIC) {
            const AnalyticEligibilityReport_ eligibility = JointAnalyticEligibility(spec, layout, plans);
            REQUIRE(eligibility.eligible_, "Joint XCCY analytic Jacobian is ineligible: " + eligibility.issues_.front().nativeMessage_);
        }

        const Vector_<> guess = BuildInitialGuess(spec, layout);
        const std::unique_ptr<Sparse::TriDiagonal_> smoothing = BuildSmoothing(layout);
        int evaluationCount = 0;
        JointXccyResidualFunction_ function(spec, layout, plans, fixings, options.jacobianMode_, &evaluationCount);
        SolveResult_ solve = Solve(spec, options, function, guess, *smoothing, layout.totalResiduals_);
        JointXccyCalibrationResult_ result = AssembleResult(spec, layout, function, &evaluationCount, fixings, &solve);
        const double convergenceBound = spec.solveMode_ == CurveSolveMode_::Value_::EXACT ? 10.0 * spec.tolerance_ : 10.0 * spec.fitTolerance_;
        REQUIRE(result.jointMaxAbsResidual_ <= convergenceBound, "Joint XCCY calibration failed to converge for pair " + PairName(spec.pair_) +
                                                                     ": maxAbsResidual = " + String::FromDouble(result.jointMaxAbsResidual_) +
                                                                     " after " + String::FromInt(result.solverEvaluations_) + " evaluations");
        return result;
    }
} // namespace Dal

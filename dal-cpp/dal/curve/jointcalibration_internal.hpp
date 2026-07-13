//
// Created by Codex on 2026/7/14.
//

#pragma once

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>

#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include <dal/curve/calibration_internal.hpp>
#include <dal/curve/curveblock.hpp>
#include <dal/curve/curveparameterization.hpp>
#include <dal/curve/jointcalibration.hpp>
#include <dal/curve/jointrate.hpp>
#include <dal/curve/xccypricing.hpp>
#include <dal/curve/ycctx.hpp>
#include <dal/curve/ycinstrument.hpp>
#include <dal/math/matrix/banded.hpp>
#include <dal/math/optimization/underdeterminedutils.hpp>
#include <dal/time/datetime.hpp>

namespace Dal::JointCalibrationInternal {
    struct CurveCollectionSpec_ {
        Date_ today_;
        String_ ccy_;
        DayBasis_ liborBasis_ = DayBasis_("ACT_365F");
        const Vector_<JointCurveDeclaration_>* curves_ = nullptr;
        String_ context_ = "Joint multi-curve calibration";
        String_ declarationLabel_ = "Declaration";
        bool requireCurveNames_ = false;
    };

    struct CurveSlot_ {
        int curveIndex_ = 0;
        int paramOffset_ = 0;
        int nParams_ = 0;
        int residualOffset_ = 0;
        int nInstruments_ = 0;
        Vector_<Handle_<YCInstrument_>> instruments_;
        Vector_<Handle_<YCInstrument_::Rate_>> rates_;
        Vector_<> marketRates_;
        double smoothingWeight_ = 1.0;
        CurveDefinition_ definition_;
        CurveParameterLayout_ layout_;
    };

    inline String_ SlotName(const CurveCollectionSpec_& spec, int index) {
        REQUIRE(spec.curves_ && index >= 0 && index < static_cast<int>(spec.curves_->size()), "Curve slot index is out of range");
        return spec.declarationLabel_ + " '" + (*spec.curves_)[index].curveName_ + "'";
    }

    inline String_ ForwardDeclarationOffendingInstrument(const JointCurveDeclaration_& declaration) {
        for (const auto& instrument : declaration.instruments_) {
            const RateIndexConvention_* convention = FloatConventionOf(*instrument);
            if (!convention || !convention->useProjectionCurve_)
                return instrument->Name();
        }
        return String_();
    }

    inline void ValidateInstrumentHandles(const JointCurveDeclaration_& declaration, const String_& slotName) {
        for (int i = 0; i < static_cast<int>(declaration.instruments_.size()); ++i)
            REQUIRE(declaration.instruments_[i], slotName + " has an empty instrument at index " + String::FromInt(i));
    }

    inline std::pair<std::set<CollateralType_>, std::set<PeriodLength_>> ValidateDeclarationIdentities(const CurveCollectionSpec_& spec) {
        REQUIRE(spec.curves_, spec.context_ + " requires curve declarations");
        REQUIRE(!spec.curves_->empty(), spec.context_ + " requires at least one curve declaration");
        std::set<CollateralType_> collaterals;
        std::set<PeriodLength_> tenors;
        bool hasDiscount = false;
        for (int i = 0; i < static_cast<int>(spec.curves_->size()); ++i) {
            const JointCurveDeclaration_& declaration = (*spec.curves_)[i];
            if (spec.requireCurveNames_)
                REQUIRE(!declaration.curveName_.empty(), spec.declarationLabel_ + " at index " + String::FromInt(i) + " requires a name");
            if (declaration.calibrateDiscountCurve_) {
                hasDiscount = true;
                REQUIRE(collaterals.insert(declaration.targetCollateral_).second, spec.context_ + " has duplicate declaration '" +
                                                                                      declaration.curveName_ + "' for discount slot " +
                                                                                      declaration.targetCollateral_.String());
                REQUIRE(!declaration.baseLayeredOverDiscount_, SlotName(spec, i) + " cannot be base-layered because it is a discount slot");
            } else {
                REQUIRE(tenors.insert(declaration.targetTenor_).second, spec.context_ + " has duplicate declaration '" + declaration.curveName_ +
                                                                            "' for forward slot " + declaration.targetTenor_.String());
            }
        }
        REQUIRE(hasDiscount, spec.context_ + " requires at least one discount-curve declaration");
        return {std::move(collaterals), std::move(tenors)};
    }

    inline std::vector<CurveSlot_> ValidateAndBuildSlots(const CurveCollectionSpec_& spec, int parameterOffset = 0, int residualOffset = 0) {
        REQUIRE(spec.today_.IsValid(), spec.context_ + " requires a valid valuation date");
        REQUIRE(!spec.ccy_.empty(), spec.context_ + " requires a currency");
        const auto produced = ValidateDeclarationIdentities(spec);
        const auto& producedCollaterals = produced.first;

        std::vector<CurveSlot_> slots;
        slots.reserve(spec.curves_->size());
        for (int i = 0; i < static_cast<int>(spec.curves_->size()); ++i) {
            const JointCurveDeclaration_& declaration = (*spec.curves_)[i];
            const String_ slotName = SlotName(spec, i);
            REQUIRE(!declaration.instruments_.empty(), slotName + " requires at least one instrument");
            ValidateInstrumentHandles(declaration, slotName);
            REQUIRE(!declaration.knotDates_.empty(), slotName + " requires at least one knot date");
            for (int k = 1; k < static_cast<int>(declaration.knotDates_.size()); ++k)
                REQUIRE(declaration.knotDates_[k] > declaration.knotDates_[k - 1], slotName + " knot dates must be strictly increasing");
            REQUIRE(declaration.knotDates_.front() > spec.today_, slotName + " knot dates must be after the valuation date");
            REQUIRE(declaration.smoothingWeight_ > 0.0, slotName + " smoothing weight must be positive");

            if (!declaration.calibrateDiscountCurve_) {
                REQUIRE(declaration.targetTenor_ != PeriodLength_(), slotName + " requires a target tenor");
                REQUIRE(producedCollaterals.count(declaration.targetCollateral_) > 0,
                        slotName + " target collateral " + declaration.targetCollateral_.String() +
                            " is not produced by any discount-curve declaration in this spec");
                const String_ offender = ForwardDeclarationOffendingInstrument(declaration);
                REQUIRE(offender.empty(),
                        slotName + " forward instrument " + offender + " has useProjectionCurve_ = false and leaves the forward curve unconstrained");
            }

            CurveSlot_ slot;
            slot.curveIndex_ = i;
            slot.paramOffset_ = parameterOffset;
            slot.residualOffset_ = residualOffset;
            slot.smoothingWeight_ = declaration.smoothingWeight_;
            slot.definition_ = MakeCurveDefinition(declaration.curveName_, spec.ccy_, declaration.parameterization_, declaration.logDfScheme_,
                                                   declaration.knotDates_, spec.today_, spec.liborBasis_);
            slot.layout_ = BuildCurveParameterLayout(slot.definition_);
            slot.nParams_ = slot.layout_.parameterCount_;
            slot.instruments_ = OrderInstruments(declaration.instruments_);
            slot.nInstruments_ = static_cast<int>(slot.instruments_.size());
            slot.rates_.reserve(slot.instruments_.size());
            slot.marketRates_.reserve(slot.instruments_.size());
            for (const auto& instrument : slot.instruments_) {
                slot.rates_.push_back(instrument->Precompute(Handle_<YieldCurve_>()));
                slot.marketRates_.push_back(instrument->MarketRate());
            }
            parameterOffset += slot.nParams_;
            residualOffset += slot.nInstruments_;
            slots.push_back(std::move(slot));
        }
        return slots;
    }

    template <class T_> Vector_<T_> SliceParameters(const Vector_<T_>& parameters, const CurveSlot_& slot) {
        Vector_<T_> result(slot.nParams_);
        for (int i = 0; i < slot.nParams_; ++i)
            result[i] = parameters[slot.paramOffset_ + i];
        return result;
    }

    inline void AddCurveSmoothing(const std::vector<CurveSlot_>& slots, Sparse::TriDiagonal_* weights) {
        REQUIRE(weights, "Curve smoothing requires an output matrix");
        for (const auto& slot : slots) {
            Vector_<DateTime_> expandedKnots;
            expandedKnots.reserve(slot.layout_.parameterCount_);
            const int firstFreeNode = slot.layout_.pinnedAnchor_ ? 1 : 0;
            for (int node = firstFreeNode; node < slot.layout_.storageNodeCount_; ++node)
                for (int parameter = 0; parameter < slot.layout_.paramsPerDeclaredKnot_; ++parameter)
                    expandedKnots.push_back(DateTime_(slot.definition_.nodeDates_[node]));
            REQUIRE(static_cast<int>(expandedKnots.size()) == slot.layout_.parameterCount_,
                    "Joint curve smoothing dates must match the curve parameter layout");
            Underdetermined::SelfCouplePWC(weights, expandedKnots, slot.smoothingWeight_, slot.paramOffset_);
        }
    }

    inline std::unique_ptr<Sparse::TriDiagonal_> BuildJointSmoothing(const std::vector<CurveSlot_>& slots) {
        int totalParameters = 0;
        for (const auto& slot : slots)
            totalParameters = std::max(totalParameters, slot.paramOffset_ + slot.nParams_);
        auto result = std::make_unique<Sparse::TriDiagonal_>(totalParameters);
        AddCurveSmoothing(slots, result.get());
        return result;
    }

    inline Vector_<> BuildGuessSlice(const JointCurveDeclaration_& declaration, int parameterCount, double defaultGuess, const String_& context) {
        if (!declaration.initialGuessPerNode_.empty()) {
            REQUIRE(static_cast<int>(declaration.initialGuessPerNode_.size()) == parameterCount,
                    context + " initialGuessPerNode_ length must equal its parameter count");
            return declaration.initialGuessPerNode_;
        }
        return Vector_<>(parameterCount, defaultGuess);
    }

    template <class T_> struct TypedCurveBlockStorage_ {
        Tape::JointCurveBlock_<T_> block_;
        std::map<CollateralType_, std::shared_ptr<Tape::DiscountCurve_<T_>>> discountCurves_;
        std::map<PeriodLength_, std::shared_ptr<Tape::DiscountCurve_<T_>>> forwardCurves_;
    };

    template <class T_>
    TypedCurveBlockStorage_<T_>
    BuildTypedCurveBlock(const CurveCollectionSpec_& spec, const std::vector<CurveSlot_>& slots, const Vector_<T_>& parameters) {
        TypedCurveBlockStorage_<T_> result;
        for (const auto& slot : slots) {
            const JointCurveDeclaration_& declaration = (*spec.curves_)[slot.curveIndex_];
            if (declaration.calibrateDiscountCurve_)
                result.discountCurves_[declaration.targetCollateral_] = BuildDiscountCurveT<T_>(slot.definition_, SliceParameters(parameters, slot));
        }
        for (const auto& slot : slots) {
            const JointCurveDeclaration_& declaration = (*spec.curves_)[slot.curveIndex_];
            if (declaration.calibrateDiscountCurve_)
                continue;
            if (declaration.baseLayeredOverDiscount_) {
                Handle_<Tape::DiscountCurve_<T_>> base(result.discountCurves_.at(declaration.targetCollateral_));
                result.forwardCurves_[declaration.targetTenor_] =
                    BuildDiscountCurveT<T_, Tape::DiscountCurve_<T_>>(slot.definition_, SliceParameters(parameters, slot), base);
            } else {
                result.forwardCurves_[declaration.targetTenor_] = BuildDiscountCurveT<T_>(slot.definition_, SliceParameters(parameters, slot));
            }
        }
        for (const auto& entry : result.discountCurves_)
            result.block_.discountCurves[entry.first] = entry.second.get();
        for (const auto& entry : result.forwardCurves_)
            result.block_.forwardCurves[entry.first] = entry.second.get();
        return result;
    }

    inline Handle_<CurveBlock_> ReferenceCurveBlock(const CurveCollectionSpec_& spec, const TypedCurveBlockStorage_<double>& storage) {
        std::map<CollateralType_, Handle_<DiscountCurve_>> discounts;
        std::map<PeriodLength_, Handle_<DiscountCurve_>> forwards;
        for (const auto& entry : storage.discountCurves_)
            discounts[entry.first] = Handle_<DiscountCurve_>(entry.second);
        for (const auto& entry : storage.forwardCurves_)
            forwards[entry.first] = Handle_<DiscountCurve_>(entry.second);
        return Handle_<CurveBlock_>(new CurveBlock_(spec.context_, spec.ccy_, discounts, forwards, spec.liborBasis_));
    }

    inline void AppendDoubleResiduals(const std::vector<CurveSlot_>& slots, const CurveBlock_& block, Vector_<>* residuals) {
        REQUIRE(residuals, "Curve residual assembly requires an output vector");
        for (const auto& slot : slots)
            for (int i = 0; i < slot.nInstruments_; ++i)
                (*residuals)[slot.residualOffset_ + i] = (*slot.rates_[i])(block)-slot.marketRates_[i];
    }

    struct CurveMaps_ {
        std::map<CollateralType_, Handle_<DiscountCurve_>> discountCurves_;
        std::map<PeriodLength_, Handle_<DiscountCurve_>> forwardCurves_;
    };

    inline CurveMaps_ BuildCurveMaps(const CurveCollectionSpec_& spec, const std::vector<CurveSlot_>& slots, const Vector_<>& parameters) {
        CurveMaps_ result;
        for (const auto& slot : slots) {
            const JointCurveDeclaration_& declaration = (*spec.curves_)[slot.curveIndex_];
            if (declaration.calibrateDiscountCurve_)
                result.discountCurves_[declaration.targetCollateral_] =
                    Handle_<DiscountCurve_>(BuildDiscountCurveUniqueT<double>(slot.definition_, SliceParameters(parameters, slot)).release());
        }
        for (const auto& slot : slots) {
            const JointCurveDeclaration_& declaration = (*spec.curves_)[slot.curveIndex_];
            if (declaration.calibrateDiscountCurve_)
                continue;
            Handle_<DiscountCurve_> base;
            if (declaration.baseLayeredOverDiscount_)
                base = result.discountCurves_.at(declaration.targetCollateral_);
            result.forwardCurves_[declaration.targetTenor_] =
                Handle_<DiscountCurve_>(BuildDiscountCurveUniqueT<double>(slot.definition_, SliceParameters(parameters, slot), base).release());
        }
        return result;
    }

    inline Handle_<CurveBlock_> BuildCurveBlock(const CurveCollectionSpec_& spec, const std::vector<CurveSlot_>& slots, const Vector_<>& parameters) {
        const CurveMaps_ maps = BuildCurveMaps(spec, slots, parameters);
        return Handle_<CurveBlock_>(new CurveBlock_(spec.context_, spec.ccy_, maps.discountCurves_, maps.forwardCurves_, spec.liborBasis_));
    }

    inline bool SupportedInstrumentType(const YCInstrument_& instrument) {
        return VisitRate(
            instrument, [](const Deposit_&) { return true; }, [](const FRA_&) { return true; }, [](const Future_&) { return true; },
            [](const Swap_&) { return true; });
    }

    inline String_ InstrumentAnalyticIneligibilityReason(const YCInstrument_& instrument, bool onDiscountDeclaration) {
        const RateIndexConvention_* convention = FloatConventionOf(instrument);
        if (!convention)
            return String_("instrument '") + instrument.Name() + "' has no floating-rate convention";
        if (!SupportedInstrumentType(instrument))
            return String_("instrument '") + instrument.Name() + "' has no templated rate implementation";
        if (onDiscountDeclaration && convention->useProjectionCurve_)
            return String_("discount-declaration instrument '") + instrument.Name() + "' projects from a forward slot";
        return String_();
    }

    inline String_ AnalyticIneligibilityReason(const CurveCollectionSpec_& spec, const std::vector<CurveSlot_>& slots) {
        if (spec.liborBasis_.String() != String_("ACT_365F"))
            return spec.context_ + " requires liborBasis_ == ACT_365F for an analytic Jacobian";
        for (const auto& slot : slots) {
            const JointCurveDeclaration_& declaration = (*spec.curves_)[slot.curveIndex_];
            for (const auto& instrument : slot.instruments_) {
                const String_ reason = InstrumentAnalyticIneligibilityReason(*instrument, declaration.calibrateDiscountCurve_);
                if (!reason.empty())
                    return SlotName(spec, slot.curveIndex_) + " is analytically ineligible: " + reason;
            }
        }
        return String_();
    }

    inline bool HasTypedDiscountRoute(const CurveCollectionSpec_& spec, const std::vector<CurveSlot_>& slots, const CollateralType_& collateral) {
        bool hasOis = false;
        for (const auto& slot : slots) {
            const JointCurveDeclaration_& declaration = (*spec.curves_)[slot.curveIndex_];
            if (!declaration.calibrateDiscountCurve_)
                continue;
            if (declaration.targetCollateral_ == collateral)
                return true;
            hasOis = hasOis || declaration.targetCollateral_ == CollateralType_(CollateralType_::Value_::OIS);
        }
        return hasOis;
    }

    inline bool HasTypedForwardRoute(const CurveCollectionSpec_& spec, const std::vector<CurveSlot_>& slots, const RateIndexConvention_& index) {
        for (const auto& slot : slots) {
            const JointCurveDeclaration_& declaration = (*spec.curves_)[slot.curveIndex_];
            if (!declaration.calibrateDiscountCurve_ && declaration.targetTenor_ == index.forecastTenor_)
                return true;
        }
        return HasTypedDiscountRoute(spec, slots, index.collateral_);
    }

    inline String_ TypedIndexAnalyticIneligibilityReason(const CurveCollectionSpec_& spec,
                                                         const std::vector<CurveSlot_>& slots,
                                                         const RateIndexConvention_& index,
                                                         const String_& leg) {
        if (!HasTypedDiscountRoute(spec, slots, index.collateral_))
            return leg + " discount route " + index.collateral_.String() + " is absent from the typed curve block";
        if (index.useProjectionCurve_ && !HasTypedForwardRoute(spec, slots, index))
            return leg + " projection route " + index.forecastTenor_.String() + " is absent from the typed curve block";
        return String_();
    }

    inline String_ XccyResetPlanAnalyticIneligibilityReason(const XccyCashflowPlan_& plan) {
        for (int i = 0; i < static_cast<int>(plan.resets_.size()); ++i) {
            if (plan.resets_[i].domesticPeriodIndex_ != i + 1 ||
                plan.resets_[i].domesticPeriodIndex_ >= static_cast<int>(plan.domesticPeriods_.size()))
                return "reset event " + String::FromInt(i) + " must map consecutively from the second domestic period";
        }
        return String_();
    }

    inline String_ XccyNotionalModeAnalyticIneligibilityReason(const XccyCashflowPlan_& plan) {
        switch (plan.config_.notionalMode_.Switch()) {
        case XccyNotionalMode_::Value_::FIXED:
            return String_();
        case XccyNotionalMode_::Value_::RESETTABLE:
        case XccyNotionalMode_::Value_::MARK_TO_MARKET:
            return XccyResetPlanAnalyticIneligibilityReason(plan);
        default:
            return String_("typed pricing does not support notional mode ") + plan.config_.notionalMode_.String();
        }
    }

    inline String_ XccyPlanAnalyticIneligibilityReason(const CurveCollectionSpec_& domestic,
                                                       const std::vector<CurveSlot_>& domesticSlots,
                                                       const CurveCollectionSpec_& foreign,
                                                       const std::vector<CurveSlot_>& foreignSlots,
                                                       const XccyCashflowPlan_& plan) {
        if (domestic.ccy_ != plan.config_.pair_.domestic_.String())
            return "domestic typed curve block currency does not match the plan pair";
        if (foreign.ccy_ != plan.config_.pair_.foreign_.String())
            return "foreign typed curve block currency does not match the plan pair";
        if (plan.domesticPeriods_.empty() || plan.foreignPeriods_.empty())
            return "typed pricing requires coupon periods on both legs";
        const String_ modeReason = XccyNotionalModeAnalyticIneligibilityReason(plan);
        if (!modeReason.empty())
            return modeReason;

        String_ reason = TypedIndexAnalyticIneligibilityReason(domestic, domesticSlots, plan.config_.convention_.domesticIndex_, "domestic");
        if (!reason.empty())
            return reason;
        return TypedIndexAnalyticIneligibilityReason(foreign, foreignSlots, plan.config_.convention_.foreignIndex_, "foreign");
    }

    inline String_ XccyPlansAnalyticIneligibilityReason(const CurveCollectionSpec_& domestic,
                                                        const std::vector<CurveSlot_>& domesticSlots,
                                                        const CurveCollectionSpec_& foreign,
                                                        const std::vector<CurveSlot_>& foreignSlots,
                                                        const Vector_<XccyCashflowPlan_>& plans,
                                                        const Vector_<Handle_<CrossCurrencySwap_>>& instruments) {
        REQUIRE(plans.size() == instruments.size(), "XCCY analytic eligibility requires one instrument per cashflow plan");
        for (int i = 0; i < static_cast<int>(plans.size()); ++i) {
            const String_ reason = XccyPlanAnalyticIneligibilityReason(domestic, domesticSlots, foreign, foreignSlots, plans[i]);
            if (!reason.empty())
                return "XCCY instrument " + String::FromInt(i) + " ('" + instruments[i]->Name() + "') is analytically ineligible: " + reason;
        }
        return String_();
    }

    template <class T_> Handle_<Tape::Rate_<T_>> DiscountRate(const YCInstrument_& instrument) {
        return VisitRate(
            instrument, [](const Deposit_& value) { return value.PrecomputeT<T_>(); }, [](const FRA_& value) { return value.PrecomputeT<T_>(); },
            [](const Future_& value) { return value.PrecomputeT<T_>(); }, [](const Swap_& value) { return value.PrecomputeT<T_>(); });
    }

    template <class T_>
    void AppendTemplatedResiduals(const std::vector<CurveSlot_>& slots, const Tape::JointCurveBlock_<T_>& block, Vector_<T_>* residuals) {
        REQUIRE(residuals, "Curve residual assembly requires an output vector");
        for (const auto& slot : slots) {
            for (int i = 0; i < slot.nInstruments_; ++i) {
                const YCInstrument_& instrument = *slot.instruments_[i];
                const RateIndexConvention_& convention = *FloatConventionOf(instrument);
                T_ modelRate;
                if (convention.useProjectionCurve_)
                    modelRate = (*Tape::ProjectionRateAt<T_>(instrument))(block);
                else
                    modelRate = (*DiscountRate<T_>(instrument))(Tape::YCCtx_<T_>(block.Discount(convention.collateral_)));
                (*residuals)[slot.residualOffset_ + i] = modelRate - static_cast<double>(slot.marketRates_[i]);
            }
        }
    }

    inline JointCurveCalibrationDiagnostics_ BuildCurveDiagnostics(const JointCurveDeclaration_& declaration,
                                                                   const CurveSlot_& slot,
                                                                   const CurveBlock_& solvedBlock,
                                                                   bool usedApproximateFit) {
        JointCurveCalibrationDiagnostics_ result;
        result.curveName_ = declaration.curveName_;
        result.curveIndex_ = slot.curveIndex_;
        result.usedApproximateFit_ = usedApproximateFit;
        result.instrumentNames_.reserve(slot.nInstruments_);
        result.marketRates_.reserve(slot.nInstruments_);
        result.modelRates_.reserve(slot.nInstruments_);
        result.residuals_.reserve(slot.nInstruments_);
        for (int i = 0; i < slot.nInstruments_; ++i) {
            const double modelRate = (*slot.rates_[i])(solvedBlock);
            const double marketRate = slot.marketRates_[i];
            result.instrumentNames_.push_back(slot.instruments_[i]->Name());
            result.marketRates_.push_back(marketRate);
            result.modelRates_.push_back(modelRate);
            result.residuals_.push_back(modelRate - marketRate);
        }
        const ResidualStats_ stats = ResidualStats(result.residuals_);
        result.maxAbsResidual_ = stats.maxAbsResidual_;
        result.rmsResidual_ = stats.rmsResidual_;
        return result;
    }
} // namespace Dal::JointCalibrationInternal

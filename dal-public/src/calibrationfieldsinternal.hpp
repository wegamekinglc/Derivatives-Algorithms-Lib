#pragma once

#include <dal-public/src/calibrationinstrument.hpp>

namespace Dal::BindingInternal {
    template <class F_> CurveInstrumentGeometry_ ReadCurveInstrumentGeometry(F_& fields) {
        CurveInstrumentGeometry_ value;
        value.today_ = fields.template Required<Date_>("today");
        value.ccy_ = fields.template Required<String_>("currency");
        value.curveName_ = fields.template Default<String_>("curve_name", value.curveName_);
        value.knotDates_ = fields.template Required<Vector_<Date_>>("knot_dates");
        value.parameterization_ = fields.template Default<CurveParameterization_>("parameterization", value.parameterization_);
        value.knotPolicy_ = fields.template Default<CurveKnotPolicy_>("knot_policy", value.knotPolicy_);
        value.logDfScheme_ = fields.template Default<LogDfScheme_>("log_df_scheme", value.logDfScheme_);
        value.baseCurve_ = fields.template Maybe<Handle_<DiscountCurve_>>("base_curve").value_or(Handle_<DiscountCurve_>());
        value.calibrateDiscountCurve_ = fields.template Default<bool>("calibrate_discount_curve", value.calibrateDiscountCurve_);
        value.targetCollateral_ = fields.template Default<CollateralType_>("target_collateral", value.targetCollateral_);
        value.targetTenor_ = fields.template Default<PeriodLength_>("target_tenor", value.targetTenor_);
        value.targetOvernightRoute_ = fields.template Default<String_>("target_overnight_route", value.targetOvernightRoute_);
        value.liborBasis_ = fields.template Default<DayBasis_>("libor_basis", value.liborBasis_);
        return value;
    }
    template <class F_> CompoundedOisObservationConvention_ ReadCompoundedOisObservationConvention(F_& f) {
        CompoundedOisObservationConvention_ c;
        c.mode_ = f.template Required<CompoundedOisObservationMode_>("mode");
        c.lookbackBusinessDays_ = f.template Default<int>("lookback_business_days", 0);
        c.lockoutBusinessDays_ = f.template Default<int>("lockout_business_days", 0);
        const bool lookback =
            c.mode_ == CompoundedOisObservationMode_::Value_::Lookback || c.mode_ == CompoundedOisObservationMode_::Value_::ObservationShift;
        f.Check("lookback_business_days", c.lookbackBusinessDays_ >= 0 && (c.lookbackBusinessDays_ > 0) == lookback,
                "positive for Lookback/ObservationShift, otherwise zero", String_(std::to_string(c.lookbackBusinessDays_)));
        f.Check("lockout_business_days",
                c.lockoutBusinessDays_ >= 0 && (c.lockoutBusinessDays_ > 0) == (c.mode_ == CompoundedOisObservationMode_::Value_::Lockout),
                "positive for Lockout, otherwise zero", String_(std::to_string(c.lockoutBusinessDays_)));
        return c;
    }
    template <class F_> CompoundedOisScriptSpec_ ReadCompoundedOisSpec(F_& f) {
        CompoundedOisScriptSpec_ v;
        v.instrumentId_ = f.template Required<String_>("instrument_id");
        v.direction_ = f.template Required<String_>("direction");
        v.discountRoute_ = f.template Required<String_>("discount_route");
        v.projectionRoute_ = f.template Required<String_>("projection_route");
        v.fixingIndex_ = f.template Required<String_>("fixing_index");
        v.notional_ = f.template Required<double>("notional");
        v.coupon_ = f.template Required<double>("coupon");
        v.currency_ = f.template Required<Ccy_>("currency");
        v.collateralCurrency_ = f.template Required<Ccy_>("collateral_currency");
        v.collateralType_ = f.template Required<CollateralType_>("collateral_type");
        v.observationBasis_ = f.template Required<DayBasis_>("observation_basis");
        v.fixedBasis_ = f.template Required<DayBasis_>("fixed_basis");
        v.calendar_ = f.template Required<HolidayCenterData_>("calendar");
        v.fixedLeg_ = f.template Required<Vector_<CompoundedOisFixedRow_>>("fixed_leg");
        v.coupons_ = f.template Required<Vector_<CompoundedOisCouponRow_>>("coupons");
        v.minAbsAnnuity_ = f.template Default<double>("min_abs_annuity", v.minAbsAnnuity_);
        v.ruleId_ = f.template Default<String_>("rule_id", v.ruleId_);
        const std::string rule(v.ruleId_.data(), v.ruleId_.size());
        const bool versionTwo = rule == "dal.compounded-ois-cashflows/2";
        f.Check("rule_id", versionTwo || rule == "dal.compounded-ois-cashflows/1", "supported exact OIS rule id", v.ruleId_);
        v.observationConvention_ = f.template Maybe<CompoundedOisObservationConvention_>("observation_convention");
        f.Check("observation_convention", versionTwo == v.observationConvention_.has_value(), "explicit convention only for /2");
        v.observations_ = versionTwo ? f.template Default<Vector_<CompoundedOisObservationRow_>>("observations", {})
                                     : f.template Required<Vector_<CompoundedOisObservationRow_>>("observations");
        f.Check("observations", !versionTwo || v.observations_.empty(), "empty manual observations for /2");
        return v;
    }
    template <class F_> CurveScriptEvaluationRequest_ ReadCurveScriptEvaluationRequest(F_& f) {
        CurveScriptEvaluationRequest_ v;
        v.geometry_ = f.template Required<CurveInstrumentGeometry_>("geometry");
        v.market_ = f.template Required<CurveInstrumentMarket_>("market");
        v.valuation_ = f.template Required<Script::ScriptValuationSettings_>("valuation");
        v.parameters_ = f.template Required<Vector_<>>("parameters");
        v.compoundedOis_ = f.template Required<Handle_<PreparedCompoundedOis_>>("compounded_ois");
        v.measureIds_ = f.template Default<Vector_<String_>>("measure_ids", v.measureIds_);
        v.evaluation_ = f.template Default<CalibrationEvaluation_>("evaluation", v.evaluation_);
        v.derivativeMethod_ = f.template Default<CalibrationDerivative_>("derivative_method", v.derivativeMethod_);
        v.differenceStep_ = f.template Default<double>("difference_step", v.differenceStep_);
        return v;
    }
} // namespace Dal::BindingInternal

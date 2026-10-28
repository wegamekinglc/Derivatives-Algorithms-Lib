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
        value.liborBasis_ = fields.template Default<DayBasis_>("libor_basis", value.liborBasis_);
        return value;
    }
} // namespace Dal::BindingInternal

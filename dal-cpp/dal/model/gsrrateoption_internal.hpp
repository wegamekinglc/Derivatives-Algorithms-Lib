#pragma once

#include <dal/model/gsrrateoption.hpp>

namespace Dal::GaussianRateOptionInternal {
    FloatingCoupon_ Coupon(const Date_& fixing,
                           const Date_& start,
                           const Date_& payment,
                           double accrual,
                           const String_& tenor,
                           const Ccy_& currency,
                           const CalibrationDiagnostic_& diagnostic);
    GSRPriceResult_ NativePrice(const CalibrationRateOptionDescriptor_& descriptor,
                                const CalibrationPreparationContext_& context,
                                const GSRPricingSettings_& settings = {});
    void RequireDomain(const CalibrationRateOptionDescriptor_& descriptor,
                       const CalibrationPreparationContext_& context,
                       bool aad,
                       const CalibrationDiagnostic_& diagnostic);
} // namespace Dal::GaussianRateOptionInternal

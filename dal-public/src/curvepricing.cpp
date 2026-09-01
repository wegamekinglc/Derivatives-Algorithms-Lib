//
// Created by dal-implementer on 2026/7/28.
//

#include <dal-public/src/curvepricing.hpp>

namespace Dal {
    const Vector_<RateInstrumentType_>& CurvePricingFamilyRegistry() {
        static const Vector_<RateInstrumentType_> result = RateInstrumentTypeListAll();
        return result;
    }

    RateQuoteRiskProvenance_ BuildSingleCurveQuoteRiskProvenance(const CurveCalibrationSpec_& spec,
                                                                 const CalibrationResult_& result,
                                                                 const CurveCalibrationOptions_& options,
                                                                 const RatePricingMarket_& boundMarket,
                                                                 const RateQuoteRiskProvenanceConfig_& config) {
        REQUIRE(result.curve_, "QUOTE_RISK_CALIBRATION_RESULT_CURVE_EMPTY");
        std::unique_ptr<YCComponent_> cloned = result.curve_->Clone(result.curve_->Name(), {});
        auto* curve = dynamic_cast<DiscountCurve_*>(cloned.get());
        REQUIRE(curve, "QUOTE_RISK_CALIBRATION_RESULT_CURVE_INVALID");

        CurveCalibrationResult_ coreResult;
        coreResult.curve_.reset(static_cast<DiscountCurve_*>(cloned.release()));
        coreResult.diagnostics_ = result.diagnostics_;
        return BuildSingleCurveQuoteRiskProvenance(spec, coreResult, options, boundMarket, config);
    }
} // namespace Dal

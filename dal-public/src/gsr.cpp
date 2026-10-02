//
// Created by Codex on 2026/10/2.
//

#include <dal/platform/platform.hpp>

#include <dal-public/src/gsr.hpp>

namespace Dal {
    namespace {
        const GSRSLVModelData_& SLV(const Handle_<ModelData_>& model) {
            REQUIRE(model, "InvalidGSRSLV: model is required");
            const auto* typed = dynamic_cast<const GSRSLVModelData_*>(model.get());
            REQUIRE(typed, "InvalidGSRSLV: model must be GSRSLVModelData");
            return *typed;
        }
    } // namespace
    Vector_<GSRMarketQuoteValue_> ConvertGSRMarketQuotes(const Handle_<Storable_>& snapshot, const Vector_<GSRMarketQuote_>& quotes) {
        const auto typed = handle_cast<GSRCurveData_>(snapshot);
        REQUIRE(typed, "InvalidGSRMarketQuote: snapshot must be GSRCurveData");
        return ConvertGSRMarketQuotes(*typed, quotes);
    }

    GSRSLVCalibrationResult_ CalibrateGSRSLVMarket(const Handle_<ModelData_>& initial,
                                                   const Vector_<GSRMarketQuote_>& quotes,
                                                   const Vector_<GSRSLVCalibrationParameter_>& parameters,
                                                   const GSRSLVCalibrationSettings_& settings,
                                                   const Vector_<GSRMarketQuote_>& heldOut) {
        return CalibrateGSRSLVMarket(SLV(initial), quotes, parameters, settings, heldOut);
    }

    GSRSLVQuoteRiskResult_ GSRSLVMarketQuoteRisk(const Handle_<ModelData_>& initial,
                                                 const Vector_<GSRMarketQuote_>& quotes,
                                                 const Vector_<GSRSLVCalibrationParameter_>& parameters,
                                                 const Vector_<GSREuropeanOption_>& targets,
                                                 const GSRSLVCalibrationSettings_& settings,
                                                 const GSRSLVQuoteRiskSettings_& riskSettings,
                                                 const GSRCurveQuoteRisk_* curveRisk) {
        return GSRSLVMarketQuoteRisk(SLV(initial), quotes, parameters, targets, settings, riskSettings, curveRisk);
    }
    GSRPriceResult_ PriceGSREuropeanOption(const Handle_<ModelData_>& model, const GSREuropeanOption_& option, const GSRPricingSettings_& settings) {
        REQUIRE(model, "InvalidGSRPricing: model is required");
        if (const auto* data = dynamic_cast<const MultiFactorGSRModelData_*>(model.get()))
            return PriceGSREuropeanOption(*data, option, settings);
        if (const auto* data = dynamic_cast<const GSRModelData_*>(model.get()))
            return PriceGSREuropeanOption(*data, option, settings);
        THROW("InvalidGSRPricing: model must be GSRModelData or MultiFactorGSRModelData");
    }

    GSRCalibrationResult_ CalibrateGSRVolatility(const Handle_<ModelData_>& initial,
                                                 const Vector_<GSRCalibrationQuote_>& quotes,
                                                 const Vector_<GSRCalibrationParameter_>& parameters,
                                                 const GSRCalibrationSettings_& settings) {
        REQUIRE(initial, "InvalidGSRCalibration: initial model is required");
        const auto* data = dynamic_cast<const MultiFactorGSRModelData_*>(initial.get());
        REQUIRE(data, "InvalidGSRCalibration: initial model must be MultiFactorGSRModelData");
        return CalibrateGSRVolatility(*data, quotes, parameters, settings);
    }

    Vector_<GSRMonteCarloPrice_>
    PriceGSRSLVEuropeanOptions(const Handle_<ModelData_>& model, const Vector_<GSREuropeanOption_>& options, const GSRMonteCarloSettings_& settings) {
        return PriceGSRSLVEuropeanOptions(SLV(model), options, settings);
    }

    GSRSLVCalibrationResult_ CalibrateGSRSLV(const Handle_<ModelData_>& initial,
                                             const Vector_<GSRCalibrationQuote_>& quotes,
                                             const Vector_<GSRSLVCalibrationParameter_>& parameters,
                                             const GSRSLVCalibrationSettings_& settings,
                                             const Vector_<GSRCalibrationQuote_>& heldOut) {
        return CalibrateGSRSLV(SLV(initial), quotes, parameters, settings, heldOut);
    }

    GSRSLVQuoteRiskResult_ GSRSLVQuoteRisk(const Handle_<ModelData_>& initial,
                                           const Vector_<GSRCalibrationQuote_>& quotes,
                                           const Vector_<GSRSLVCalibrationParameter_>& parameters,
                                           const Vector_<GSREuropeanOption_>& targets,
                                           const GSRSLVCalibrationSettings_& calibrationSettings,
                                           const GSRSLVQuoteRiskSettings_& riskSettings,
                                           const GSRCurveQuoteRisk_* curveRisk) {
        return GSRSLVQuoteRisk(SLV(initial), quotes, parameters, targets, calibrationSettings, riskSettings, curveRisk);
    }

    GSRCurveQuoteRisk_ BuildGSRCurveQuoteRisk(const Handle_<Storable_>& snapshot,
                                              const RatePricingMarket_& market,
                                              const RateQuoteRiskProvenance_& provenance,
                                              const String_& discountComponent,
                                              const Vector_<String_>& projectionComponents) {
        const auto typed = handle_cast<GSRCurveData_>(snapshot);
        REQUIRE(typed, "InvalidGSRCurveRisk: snapshot must be GSRCurveData");
        return BuildGSRCurveQuoteRisk(*typed, market, provenance, discountComponent, projectionComponents);
    }
} // namespace Dal

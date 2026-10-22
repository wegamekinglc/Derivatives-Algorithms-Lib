#pragma once

#include <dal-public/src/calibrationinstrument.hpp>
#include <dal/model/gsrbondoption.hpp>
#include <dal/model/gsrcalibration.hpp>
#include <dal/model/gsrmarketcalibration.hpp>
#include <dal/model/gsrscriptcalibration.hpp>
#include <dal/model/gsrslvcalibration.hpp>

namespace Dal {
    struct GSRInstrumentCalibrationSettings_ {
        GSRCalibrationSettings_ solver_{};
        Script::ScriptValuationSettings_ valuation_;
        CalibrationEvaluationOptions_ evaluation_{};
        std::optional<CalibrationMonteCarloSampling_> fitSampling_, validationSampling_, heldOutSampling_;
        Vector_<Handle_<CalibrationObservation_>> heldOutObservations_;
        int maxPathsPerView_ = 0, maxBasketEvaluations_ = 1000;
        double validationSigma_ = 3., epsilonValue_ = 1e-10, epsilonGradient_ = 1e-8;
        bool compiled_ = Script::DEFAULT_COMPILED;
    };
    struct GSRInstrumentCalibrationResult_ : GSRScriptCalibrationResult_ {
        String_ backend_;
        bool nativeJacobianAvailable_ = false;
        GaussianBondOptionCosts_ fastCosts_;
        size_t reportPreparations_ = 0;
    };
    // Shared semantic checks for typed settings factories and direct C++ callers.
    void ValidateGSRInstrumentSolverSettings(const GSRCalibrationSettings_& settings);
    void ValidateGSRInstrumentCalibrationSettings(const GSRInstrumentCalibrationSettings_& settings);
    GSRInstrumentCalibrationResult_ CalibrateGSRInstruments(const Handle_<ModelData_>& initial,
                                                            const Vector_<Handle_<CalibrationObservation_>>& observations,
                                                            const Vector_<GSRCalibrationParameter_>& parameters,
                                                            const GSRInstrumentCalibrationSettings_& settings);
    Vector_<VolQuoteValue_> ConvertVolQuotes(const Handle_<Storable_>& snapshot, const Vector_<VolQuote_>& quotes);
    GSRSLVCalibrationResult_ CalibrateGSRSLVMarket(const Handle_<ModelData_>& initial,
                                                   const Vector_<VolQuote_>& quotes,
                                                   const Vector_<GSRSLVCalibrationParameter_>& parameters,
                                                   const GSRSLVCalibrationSettings_& settings = {},
                                                   const Vector_<VolQuote_>& heldOut = {});
    GSRSLVQuoteRiskResult_ GSRSLVMarketQuoteRisk(const Handle_<ModelData_>& initial,
                                                 const Vector_<VolQuote_>& quotes,
                                                 const Vector_<GSRSLVCalibrationParameter_>& parameters,
                                                 const Vector_<EuropeanRateOption_>& targets,
                                                 const GSRSLVCalibrationSettings_& calibrationSettings = {},
                                                 const GSRSLVQuoteRiskSettings_& riskSettings = {},
                                                 const GSRCurveQuoteRisk_* curveRisk = nullptr);
    GSRPriceResult_
    PriceGSREuropeanOption(const Handle_<ModelData_>& model, const EuropeanRateOption_& option, const GSRPricingSettings_& settings = {});
    GSRCalibrationResult_ CalibrateGSRVolatility(const Handle_<ModelData_>& initial,
                                                 const Vector_<CalibrationQuote_>& quotes,
                                                 const Vector_<GSRCalibrationParameter_>& parameters,
                                                 const GSRCalibrationSettings_& settings = {});
    Vector_<GSRMonteCarloPrice_> PriceGSRSLVEuropeanOptions(const Handle_<ModelData_>& model,
                                                            const Vector_<EuropeanRateOption_>& options,
                                                            const GSRMonteCarloSettings_& settings = {});
    GSRSLVCalibrationResult_ CalibrateGSRSLV(const Handle_<ModelData_>& initial,
                                             const Vector_<CalibrationQuote_>& quotes,
                                             const Vector_<GSRSLVCalibrationParameter_>& parameters,
                                             const GSRSLVCalibrationSettings_& settings = {},
                                             const Vector_<CalibrationQuote_>& heldOut = {});
    GSRSLVQuoteRiskResult_ GSRSLVQuoteRisk(const Handle_<ModelData_>& initial,
                                           const Vector_<CalibrationQuote_>& quotes,
                                           const Vector_<GSRSLVCalibrationParameter_>& parameters,
                                           const Vector_<EuropeanRateOption_>& targets,
                                           const GSRSLVCalibrationSettings_& calibrationSettings = {},
                                           const GSRSLVQuoteRiskSettings_& riskSettings = {},
                                           const GSRCurveQuoteRisk_* curveRisk = nullptr);
    GSRCurveQuoteRisk_ BuildGSRCurveQuoteRisk(const Handle_<Storable_>& snapshot,
                                              const RatePricingMarket_& market,
                                              const RateQuoteRiskProvenance_& provenance,
                                              const String_& discountComponent,
                                              const Vector_<String_>& projectionComponents = {});
} // namespace Dal

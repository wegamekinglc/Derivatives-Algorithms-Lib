#pragma once

#include <dal-public/src/calibrationinstrument.hpp>
#include <dal-public/src/xccycalibration.hpp>
#include <dal/curve/xccygaussianmoments.hpp>
#include <dal/script/xccymc.hpp>

namespace Dal {
    struct XccyMonteCarloRequest_ {
        String_ measureId_;
        XccyMonteCarloPhase_ phase_;
        CalibrationMonteCarloSampling_ sampling_;
        Handle_<MarketFixingSnapshot_> fixings_;
        CalibrationSimulationSettings_ simulation_{false, {}, 0., "Precise"};
        CalibrationDerivative_ derivativeMethod_ = CalibrationDerivative_::Value_::None;
        Vector_<String_> parameterLabels_;
    };
    struct XccyMonteCarloResult_ : CalibrationMonteCarloResult_ {
        std::shared_ptr<const XccyMcResult_> details_;
    };
    struct XccyGaussianValue_ {
        double a_ = 0., b_ = 0., value_ = 0.;
        Handle_<CalibrationReport_> report_;
    };
    [[nodiscard]] Handle_<XccyGaussianModelData_> NewXccyGaussianModel(const String_& name, const XccyGaussianModelSpec_& spec);
    [[nodiscard]] XccyMonteCarloResult_ ValueXccyInstrumentByMonteCarlo(const Handle_<CalibrationInstrument_>& instrument,
                                                                        const Handle_<XccyGaussianModelData_>& model,
                                                                        const XccyMonteCarloRequest_& request);
    [[nodiscard]] XccyGaussianValue_ ValueXccyInstrumentByGaussian(const Handle_<CalibrationInstrument_>& instrument,
                                                                   const Handle_<XccyGaussianModelData_>& model,
                                                                   const String_& measure,
                                                                   const Handle_<MarketFixingSnapshot_>& fixings = {});
    [[nodiscard]] const XccyMcResult_& XccyMonteCarloDetails(const XccyMonteCarloResult_& result);
    [[nodiscard]] String_ DumpXccyMonteCarloResultJson(const XccyMonteCarloResult_& result);
    [[nodiscard]] XccyMcValidation_
    ValidateXccyMonteCarlo(const XccyMonteCarloResult_& result, const CalibrationMcReference_& reference, const CalibrationMcPrecision_& precision);
    [[nodiscard]] Handle_<CalibrationReport_> XccyGaussianInstrumentCapabilities(const Handle_<CalibrationInstrument_>& instrument,
                                                                                 const Handle_<XccyGaussianModelData_>& model,
                                                                                 const XccyMonteCarloRequest_& request);
    [[nodiscard]] const Handle_<XccyGaussianModelData_>& XccyInstrumentResultGaussianModel(const XccyInstrumentCalibrationResult_& result);
} // namespace Dal

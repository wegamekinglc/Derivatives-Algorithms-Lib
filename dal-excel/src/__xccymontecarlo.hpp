#pragma once

#include "__xccyinstrument.hpp"
#include <dal-public/src/xccymontecarlo.hpp>

#if defined(_WIN32) && defined(DAL_EXCEL_TEST_API_EXPORTS)
#define DAL_XCCY_MC_API __declspec(dllexport)
#elif defined(_WIN32) && defined(DAL_EXCEL_TEST_API_IMPORTS)
#define DAL_XCCY_MC_API __declspec(dllimport)
#else
#define DAL_XCCY_MC_API
#endif

namespace Dal {
    using StorableXccyGaussianMarketSpec_ = StorableCalibrationValue_<XccyGaussianMarketSpec_>;
    using StorableXccyGaussianDynamicsSpec_ = StorableCalibrationValue_<XccyGaussianDynamicsSpec_>;
    using StorableXccyGaussianModelSpec_ = StorableCalibrationValue_<XccyGaussianModelSpec_>;
    using StorableXccyMonteCarloRequest_ = StorableCalibrationValue_<XccyMonteCarloRequest_>;
    using StorableXccyParameterLabels_ = StorableCalibrationValue_<Vector_<String_>>;
    using StorableXccyMonteCarloReference_ = StorableCalibrationValue_<CalibrationMcReference_>;
    using StorableXccyMonteCarloPrecision_ = StorableCalibrationValue_<CalibrationMcPrecision_>;
    using StorableXccyMonteCarloValidation_ = StorableCalibrationValue_<XccyMcValidation_>;
    struct XccyMonteCarloOutcome_ {
        String_ status_;
        std::optional<XccyMonteCarloResult_> result_;
        Handle_<CalibrationReport_> report_;
        Handle_<Storable_> candidate_;
    };
    using StorableXccyMonteCarloOutcome_ = StorableCalibrationValue_<XccyMonteCarloOutcome_>;
    DAL_XCCY_MC_API void XccyGaussianMarketSpec_New(const Matrix_<Cell_>& settings,
                                                    const Handle_<StorableCurveBlock_>& domestic,
                                                    const Handle_<StorableCurveBlock_>& foreign,
                                                    const Handle_<StorableDiscountCurve_>& basis,
                                                    Handle_<StorableXccyGaussianMarketSpec_>* value);
    DAL_XCCY_MC_API void XccyGaussianDynamicsSpec_New(const Matrix_<Cell_>& settings,
                                                      const Matrix_<Cell_>& buckets,
                                                      const Matrix_<Cell_>& correlation,
                                                      Handle_<StorableXccyGaussianDynamicsSpec_>* value);
    DAL_XCCY_MC_API void XccyGaussianModelSpec_New(const Handle_<StorableXccyGaussianMarketSpec_>& market,
                                                   const Handle_<StorableXccyGaussianDynamicsSpec_>& dynamics,
                                                   Handle_<StorableXccyGaussianModelSpec_>* value);
    DAL_XCCY_MC_API void
    XccyGaussianDynamics_New(const String_& name, const Handle_<StorableXccyGaussianDynamicsSpec_>& spec, Handle_<XccyGaussianDynamics_>* value);
    DAL_XCCY_MC_API void
    XccyGaussianModel_New(const String_& name, const Handle_<StorableXccyGaussianModelSpec_>& spec, Handle_<XccyGaussianModelData_>* value);
    DAL_XCCY_MC_API void XccyParameterLabels_New(const Vector_<String_>& labels, Handle_<StorableXccyParameterLabels_>* value);
    DAL_XCCY_MC_API void XccyMonteCarloRequest_New(const String_& measureId,
                                                   const Cell_& phase,
                                                   const Handle_<StorableCalibrationMonteCarloSampling_>& sampling,
                                                   const Matrix_<Cell_>& settings,
                                                   Handle_<StorableXccyMonteCarloRequest_>* value);
    DAL_XCCY_MC_API void XccyInstrument_ValueByMonteCarlo(const Handle_<CalibrationInstrument_>& instrument,
                                                          const Handle_<XccyGaussianModelData_>& model,
                                                          const Handle_<StorableXccyMonteCarloRequest_>& request,
                                                          Handle_<StorableXccyMonteCarloOutcome_>* outcome);
    DAL_XCCY_MC_API void XccyInstrument_ValueByGaussian(const Handle_<CalibrationInstrument_>& instrument,
                                                        const Handle_<XccyGaussianModelData_>& model,
                                                        const String_& measureId,
                                                        const Handle_<StorableMarketFixingSnapshot_>& fixings,
                                                        Handle_<StorableCalibrationOutcome_>* outcome);
    DAL_XCCY_MC_API void XccyMonteCarloOutcome_Get_Json(const Handle_<StorableXccyMonteCarloOutcome_>& outcome, Vector_<String_>* json);
    DAL_XCCY_MC_API void XccyMonteCarloOutcome_Get_Status(const Handle_<StorableXccyMonteCarloOutcome_>& outcome, String_* status);
    DAL_XCCY_MC_API void XccyMonteCarloOutcome_Get_Report(const Handle_<StorableXccyMonteCarloOutcome_>& outcome,
                                                          Handle_<CalibrationReport_>* report);
    DAL_XCCY_MC_API void XccyMonteCarloOutcome_Get_Mean(const Handle_<StorableXccyMonteCarloOutcome_>& outcome, double* mean);
    DAL_XCCY_MC_API void XccyMonteCarloOutcome_Get_StandardError(const Handle_<StorableXccyMonteCarloOutcome_>& outcome, double* error);
    DAL_XCCY_MC_API void XccyMonteCarloOutcome_Get_Partials(const Handle_<StorableXccyMonteCarloOutcome_>& outcome, Matrix_<Cell_>* partials);
    DAL_XCCY_MC_API void XccyMonteCarloOutcome_Get_Model(const Handle_<StorableXccyMonteCarloOutcome_>& outcome,
                                                         Handle_<XccyGaussianModelData_>* model);
    DAL_XCCY_MC_API void XccyMonteCarloReference_New(const Matrix_<Cell_>& settings, Handle_<StorableXccyMonteCarloReference_>* value);
    DAL_XCCY_MC_API void XccyMonteCarloPrecision_New(const Matrix_<Cell_>& settings, Handle_<StorableXccyMonteCarloPrecision_>* value);
    DAL_XCCY_MC_API void XccyMonteCarloOutcome_Validate(const Handle_<StorableXccyMonteCarloOutcome_>& outcome,
                                                        const Handle_<StorableXccyMonteCarloReference_>& reference,
                                                        const Handle_<StorableXccyMonteCarloPrecision_>& precision,
                                                        Handle_<StorableXccyMonteCarloValidation_>* value);
    DAL_XCCY_MC_API void XccyMonteCarloValidation_Get_Flags(const Handle_<StorableXccyMonteCarloValidation_>& value, Matrix_<Cell_>* flags);
    DAL_XCCY_MC_API void XccyMonteCarloValidation_Get_Report(const Handle_<StorableXccyMonteCarloValidation_>& value,
                                                             Handle_<CalibrationReport_>* report);
    DAL_XCCY_MC_API void CalibrationInstrument_Get_XccyGaussianCapabilities(const Handle_<CalibrationInstrument_>& instrument,
                                                                            const Handle_<XccyGaussianModelData_>& model,
                                                                            const Handle_<StorableXccyMonteCarloRequest_>& request,
                                                                            Handle_<CalibrationReport_>* report);
} // namespace Dal

#undef DAL_XCCY_MC_API

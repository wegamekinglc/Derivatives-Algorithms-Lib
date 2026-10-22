#pragma once

#include "__curve_storable.hpp"
#include "__gsr_test_api.hpp"
#include "__script_storable.hpp"
#include <dal-public/src/calibrationinstrument.hpp>

#if defined(_WIN32) && defined(DAL_EXCEL_TEST_API_EXPORTS)
#define DAL_CALIBRATION_EXCEL_API __declspec(dllexport)
#elif defined(_WIN32) && defined(DAL_EXCEL_TEST_API_IMPORTS)
#define DAL_CALIBRATION_EXCEL_API __declspec(dllimport)
#else
#define DAL_CALIBRATION_EXCEL_API
#endif

namespace Dal {
    template <class T_> class StorableCalibrationValue_ final : public Storable_ {
    public:
        const T_ val_;
        explicit StorableCalibrationValue_(const T_& value) : Storable_("CalibrationSessionValue", ""), val_(value) {}
        void Write(Archive::Store_&) const override {
            CalibrationInputFailure("CalibrationPersistenceUnsupported", "/session", "instrument or definition persistence only");
        }
    };
    using StorableScriptInstrumentImportSettings_ = StorableCalibrationValue_<ScriptInstrumentImportSettings_>;
    using StorableCurveInstrumentGeometry_ = StorableCalibrationValue_<CurveInstrumentGeometry_>;
    using StorableCurveInstrumentMarket_ = StorableCalibrationValue_<CurveInstrumentMarket_>;
    using StorableCurveInstrumentSolver_ = StorableCalibrationValue_<CurveInstrumentSolver_>;
    using StorableCurveInstrumentCalibrationSpec_ = StorableCalibrationValue_<CurveInstrumentCalibrationSpec_>;
    using StorableCurveInstrumentCalibrationOptions_ = StorableCalibrationValue_<CurveInstrumentCalibrationOptions_>;
    using StorableCalibrationSamplingSpec_ = StorableCalibrationValue_<CalibrationSamplingSpec_>;
    using StorableCalibrationEvaluationOptions_ = StorableCalibrationValue_<CalibrationEvaluationOptions_>;
    using StorableCalibrationPreparationContext_ = StorableCalibrationValue_<CalibrationPreparationContext_>;
    using StorableCalibrationMonteCarloSampling_ = StorableCalibrationValue_<CalibrationMonteCarloSampling_>;
    using StorableCalibrationSimulationSettings_ = StorableCalibrationValue_<CalibrationSimulationSettings_>;
    using StorableCalibrationMonteCarloRequest_ = StorableCalibrationValue_<CalibrationMonteCarloRequest_>;
    using StorableCalibrationMarketQuote_ = StorableCalibrationValue_<Handle_<CalibrationMarketQuote_>>;
    using StorableCalibrationObservation_ = StorableCalibrationValue_<Handle_<CalibrationObservation_>>;
    using StorableCalibrationObservations_ = StorableCalibrationValue_<Vector_<Handle_<CalibrationObservation_>>>;
    using StorableCalibrationDates_ = StorableCalibrationValue_<Vector_<Date_>>;
    using StorableCalibrationNumbers_ = StorableCalibrationValue_<Vector_<>>;
    using StorableCurveScriptRoutes_ = StorableCalibrationValue_<Vector_<CurveScriptRoute_>>;
    using StorableCalibrationDiscountCurves_ = StorableCalibrationValue_<std::map<CollateralType_, Handle_<DiscountCurve_>>>;
    using StorableCalibrationForwardCurves_ = StorableCalibrationValue_<std::map<PeriodLength_, Handle_<DiscountCurve_>>>;
    using StorablePreparedCalibrationInstrument_ = StorableCalibrationValue_<PreparedCalibrationInstrument_>;
    struct StorableCalibrationOutcome_ final : Storable_ {
        const String_ status_;
        const Handle_<CalibrationReport_> report_;
        const Handle_<DiscountCurve_> curve_;
        const std::optional<double> mean_;
        StorableCalibrationOutcome_(String_ status,
                                    Handle_<CalibrationReport_> report,
                                    Handle_<DiscountCurve_> curve = {},
                                    std::optional<double> mean = {})
            : Storable_("CalibrationOutcome", ""), status_(std::move(status)), report_(std::move(report)), curve_(std::move(curve)), mean_(mean) {}
        void Write(Archive::Store_&) const override {
            CalibrationInputFailure("CalibrationPersistenceUnsupported", "/outcome", "instrument or definition persistence only");
        }
    };
    DAL_CALIBRATION_EXCEL_API void ScriptInstrumentImportSettings_New(const Matrix_<Cell_>& settings,
                                                                      Handle_<StorableScriptInstrumentImportSettings_>* value);
    DAL_CALIBRATION_EXCEL_API void CurveInstrumentGeometry_New(const Matrix_<Cell_>& settings, Handle_<StorableCurveInstrumentGeometry_>* value);
    DAL_CALIBRATION_EXCEL_API void CurveInstrumentMarket_New(const Matrix_<Cell_>& settings, Handle_<StorableCurveInstrumentMarket_>* value);
    DAL_CALIBRATION_EXCEL_API void CurveInstrumentSolver_New(const Matrix_<Cell_>& settings, Handle_<StorableCurveInstrumentSolver_>* value);
    DAL_CALIBRATION_EXCEL_API void CurveInstrumentCalibrationSpec_New(const Matrix_<Cell_>& settings,
                                                                      Handle_<StorableCurveInstrumentCalibrationSpec_>* value);
    DAL_CALIBRATION_EXCEL_API void CurveInstrumentCalibrationOptions_New(const Matrix_<Cell_>& settings,
                                                                         Handle_<StorableCurveInstrumentCalibrationOptions_>* value);
    DAL_CALIBRATION_EXCEL_API void CalibrationSamplingSpec_New(const Matrix_<Cell_>& settings, Handle_<StorableCalibrationSamplingSpec_>* value);
    DAL_CALIBRATION_EXCEL_API void CalibrationEvaluationOptions_New(const Matrix_<Cell_>& settings,
                                                                    Handle_<StorableCalibrationEvaluationOptions_>* value);
    DAL_CALIBRATION_EXCEL_API void CalibrationPreparationContext_New(const Matrix_<Cell_>& settings,
                                                                     Handle_<StorableCalibrationPreparationContext_>* value);
    DAL_CALIBRATION_EXCEL_API void CalibrationMonteCarloSampling_New(const Matrix_<Cell_>& settings,
                                                                     Handle_<StorableCalibrationMonteCarloSampling_>* value);
    DAL_CALIBRATION_EXCEL_API void CalibrationSimulationSettings_New(const Matrix_<Cell_>& settings,
                                                                     Handle_<StorableCalibrationSimulationSettings_>* value);
    DAL_CALIBRATION_EXCEL_API void CalibrationMonteCarloRequest_New(const Matrix_<Cell_>& settings,
                                                                    Handle_<StorableCalibrationMonteCarloRequest_>* value);
    DAL_CALIBRATION_EXCEL_API void CalibrationDates_New(const Vector_<Cell_>& dates, Handle_<StorableCalibrationDates_>* value);
    DAL_CALIBRATION_EXCEL_API void CalibrationNumbers_New(const Vector_<Cell_>& numbers, Handle_<StorableCalibrationNumbers_>* value);
    DAL_CALIBRATION_EXCEL_API void CurveScriptRoutes_New(const Matrix_<Cell_>& routes, Handle_<StorableCurveScriptRoutes_>* value);
    DAL_CALIBRATION_EXCEL_API void CalibrationDiscountCurves_New(const Matrix_<Cell_>& bindings, Handle_<StorableCalibrationDiscountCurves_>* value);
    DAL_CALIBRATION_EXCEL_API void CalibrationForwardCurves_New(const Matrix_<Cell_>& bindings, Handle_<StorableCalibrationForwardCurves_>* value);
    DAL_CALIBRATION_EXCEL_API void CalibrationObservations_New(const Vector_<Handle_<Storable_>>& observations,
                                                               Handle_<StorableCalibrationObservations_>* value);
    DAL_CALIBRATION_EXCEL_API void ScriptInstrumentDefinition_New(const Matrix_<Cell_>& source,
                                                                  const Matrix_<Cell_>& header,
                                                                  const Matrix_<Cell_>& parameters,
                                                                  const Matrix_<Cell_>& measures,
                                                                  const Handle_<StorableScriptProductSettings_>& settings,
                                                                  Handle_<ScriptInstrumentDefinition_>* definition);
    DAL_CALIBRATION_EXCEL_API void ScriptCalibrationInstrument_New(const String_& id,
                                                                   const Handle_<ScriptInstrumentDefinition_>& definition,
                                                                   const Matrix_<Cell_>& bindings,
                                                                   const Cell_& pillar,
                                                                   Handle_<CalibrationInstrument_>* instrument);
    DAL_CALIBRATION_EXCEL_API void CalibrationInstrument_BindParameters(const Handle_<CalibrationInstrument_>& instrument,
                                                                        const Matrix_<Cell_>& bindings,
                                                                        Handle_<CalibrationInstrument_>* result);
    DAL_CALIBRATION_EXCEL_API void DepositScriptInstrument_New(const Handle_<StorableYCInstrument_>& deposit,
                                                               const Handle_<StorableScriptInstrumentImportSettings_>& settings,
                                                               Handle_<CalibrationInstrument_>* instrument);
    DAL_CALIBRATION_EXCEL_API void EuropeanRateOptionScriptInstrument_New(const Handle_<StorableEuropeanRateOption_>& option,
                                                                          const Handle_<StorableScriptInstrumentImportSettings_>& settings,
                                                                          Handle_<CalibrationInstrument_>* instrument);
    DAL_CALIBRATION_EXCEL_API void CalibrationMarketQuote_New(const String_& id,
                                                              const Cell_& value,
                                                              const String_& unit,
                                                              const Matrix_<Cell_>& convention,
                                                              Handle_<StorableCalibrationMarketQuote_>* quote);
    DAL_CALIBRATION_EXCEL_API void CalibrationObservation_New(const String_& id,
                                                              const Handle_<CalibrationInstrument_>& instrument,
                                                              const String_& measureId,
                                                              const Handle_<StorableCalibrationMarketQuote_>& quote,
                                                              const Cell_& errorScale,
                                                              Handle_<StorableCalibrationObservation_>* observation);
    DAL_CALIBRATION_EXCEL_API void Calibrate_CurveInstruments(const Handle_<StorableCurveInstrumentCalibrationSpec_>& spec,
                                                              const Handle_<StorableCurveInstrumentCalibrationOptions_>& options,
                                                              Handle_<StorableCalibrationOutcome_>* outcome);
    DAL_CALIBRATION_EXCEL_API void CalibrationInstrument_ValueByMonteCarlo(const Handle_<CalibrationInstrument_>& instrument,
                                                                           const Handle_<ModelData_>& model,
                                                                           const Handle_<StorableCalibrationMonteCarloRequest_>& request,
                                                                           Handle_<StorableCalibrationOutcome_>* outcome);
    DAL_CALIBRATION_EXCEL_API void CalibrationPlan_New(const Handle_<CalibrationInstrument_>& instrument,
                                                       const String_& measureId,
                                                       const Handle_<StorableCalibrationPreparationContext_>& context,
                                                       const Handle_<StorableCalibrationEvaluationOptions_>& options,
                                                       Handle_<StorablePreparedCalibrationInstrument_>* plan);
    DAL_CALIBRATION_EXCEL_API void CalibrationOutcome_Get_Report(const Handle_<StorableCalibrationOutcome_>& outcome,
                                                                 Handle_<CalibrationReport_>* report);
    DAL_CALIBRATION_EXCEL_API void CalibrationOutcome_Get_Status(const Handle_<StorableCalibrationOutcome_>& outcome, String_* status);
    DAL_CALIBRATION_EXCEL_API void CalibrationOutcome_Get_Curve(const Handle_<StorableCalibrationOutcome_>& outcome,
                                                                Handle_<StorableDiscountCurve_>* curve);
    DAL_CALIBRATION_EXCEL_API void CalibrationOutcome_Get_Mean(const Handle_<StorableCalibrationOutcome_>& outcome, double* mean);
    DAL_CALIBRATION_EXCEL_API void CalibrationReport_Get_Json(const Handle_<CalibrationReport_>& report, Vector_<String_>* json);
    DAL_CALIBRATION_EXCEL_API void CalibrationReport_Get_QuoteJacobian(const Handle_<CalibrationReport_>& report, Matrix_<>* value);
    DAL_CALIBRATION_EXCEL_API void CalibrationReport_Get_QuoteMap(const Handle_<CalibrationReport_>& report, Matrix_<>* value);
    DAL_CALIBRATION_EXCEL_API void ScriptInstrumentDefinition_Get_Json(const Handle_<ScriptInstrumentDefinition_>& definition,
                                                                       Vector_<String_>* json);
    DAL_CALIBRATION_EXCEL_API void ScriptInstrumentDefinition_LoadJson(const String_& json, Handle_<ScriptInstrumentDefinition_>* definition);
    DAL_CALIBRATION_EXCEL_API void CalibrationInstrument_Get_Json(const Handle_<CalibrationInstrument_>& instrument, Vector_<String_>* json);
    DAL_CALIBRATION_EXCEL_API void CalibrationInstrument_LoadJson(const String_& json, Handle_<CalibrationInstrument_>* instrument);
    DAL_CALIBRATION_EXCEL_API void CalibrationReport_Get_Summary(const Handle_<CalibrationReport_>& report, Matrix_<Cell_>* value);
    DAL_CALIBRATION_EXCEL_API void CalibrationReport_Get_Observations(const Handle_<CalibrationReport_>& report, Matrix_<Cell_>* value);
    DAL_CALIBRATION_EXCEL_API void CalibrationReport_Get_Quotes(const Handle_<CalibrationReport_>& report, Matrix_<Cell_>* value);
    DAL_CALIBRATION_EXCEL_API void CalibrationReport_Get_Parameters(const Handle_<CalibrationReport_>& report, Matrix_<Cell_>* value);
    DAL_CALIBRATION_EXCEL_API void CalibrationReport_Get_Plans(const Handle_<CalibrationReport_>& report, Matrix_<Cell_>* value);
    DAL_CALIBRATION_EXCEL_API void CalibrationReport_Get_Errors(const Handle_<CalibrationReport_>& report, Matrix_<Cell_>* value);
    DAL_CALIBRATION_EXCEL_API void CalibrationPlan_Get_PlanIdentity(const Handle_<StorablePreparedCalibrationInstrument_>& plan, String_* value);
    DAL_CALIBRATION_EXCEL_API void FraScriptInstrument_New(const Handle_<StorableYCInstrument_>& native,
                                                           const Cell_& payment,
                                                           const Handle_<StorableScriptInstrumentImportSettings_>& settings,
                                                           Handle_<CalibrationInstrument_>* instrument);
    DAL_CALIBRATION_EXCEL_API void SwapScriptInstrument_New(const Handle_<StorableYCInstrument_>& native,
                                                            const Handle_<StorableScriptInstrumentImportSettings_>& settings,
                                                            Handle_<CalibrationInstrument_>* instrument);
    DAL_CALIBRATION_EXCEL_API void PeriodForwardOisScriptInstrument_New(const Handle_<StorableYCInstrument_>& native,
                                                                        const Handle_<StorableScriptInstrumentImportSettings_>& settings,
                                                                        Handle_<CalibrationInstrument_>* instrument);
    DAL_CALIBRATION_EXCEL_API void BasisSwapScriptInstrument_New(const Handle_<StorableYCInstrument_>& native,
                                                                 const Handle_<StorableScriptInstrumentImportSettings_>& settings,
                                                                 Handle_<CalibrationInstrument_>* instrument);
} // namespace Dal

#undef DAL_CALIBRATION_EXCEL_API

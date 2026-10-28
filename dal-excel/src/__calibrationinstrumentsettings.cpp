#include "__calibrationinstrumentinput.hpp"
#include "__platform.hpp"
#include "__scriptinput.hpp"
#include <dal-public/src/calibrationfieldsinternal.hpp>

/*IF--------------------------------------------------------------------------
public ScriptInstrumentImportSettings_New
    Create immutable typed calibration settings
&inputs
settings is cell[][]
    Exact field/value rows; nested values are typed handles
+const Excel::ScriptSettingsInput_ normalized_settings(xl_settings); xl_settings = normalized_settings.Get();
+argName = "settings"; Excel::ValidateScriptSettingsRange(xl_settings, "ScriptInstrumentImportSettings_New", "settings");
&outputs
value is handle StorableScriptInstrumentImportSettings
    Owning session settings
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CurveInstrumentGeometry_New
    Create immutable typed calibration settings
&inputs
settings is cell[][]
    Exact field/value rows; nested values are typed handles
+const Excel::ScriptSettingsInput_ normalized_settings(xl_settings); xl_settings = normalized_settings.Get();
+argName = "settings"; Excel::ValidateScriptSettingsRange(xl_settings, "CurveInstrumentGeometry_New", "settings");
&outputs
value is handle StorableCurveInstrumentGeometry
    Owning session settings
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CurveInstrumentMarket_New
    Create immutable typed calibration settings
&inputs
settings is cell[][]
    Exact field/value rows; nested values are typed handles
+const Excel::ScriptSettingsInput_ normalized_settings(xl_settings); xl_settings = normalized_settings.Get();
+argName = "settings"; Excel::ValidateScriptSettingsRange(xl_settings, "CurveInstrumentMarket_New", "settings");
&outputs
value is handle StorableCurveInstrumentMarket
    Owning session settings
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CurveInstrumentSolver_New
    Create immutable typed calibration settings
&inputs
settings is cell[][]
    Exact field/value rows; nested values are typed handles
+const Excel::ScriptSettingsInput_ normalized_settings(xl_settings); xl_settings = normalized_settings.Get();
+argName = "settings"; Excel::ValidateScriptSettingsRange(xl_settings, "CurveInstrumentSolver_New", "settings");
&outputs
value is handle StorableCurveInstrumentSolver
    Owning session settings
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CurveInstrumentCalibrationSpec_New
    Create immutable typed calibration settings
&inputs
settings is cell[][]
    Exact field/value rows; nested values are typed handles
+const Excel::ScriptSettingsInput_ normalized_settings(xl_settings); xl_settings = normalized_settings.Get();
+argName = "settings"; Excel::ValidateScriptSettingsRange(xl_settings, "CurveInstrumentCalibrationSpec_New", "settings");
&outputs
value is handle StorableCurveInstrumentCalibrationSpec
    Owning session settings
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CurveInstrumentCalibrationOptions_New
    Create immutable typed calibration settings
&inputs
settings is cell[][]
    Exact field/value rows; nested values are typed handles
+const Excel::ScriptSettingsInput_ normalized_settings(xl_settings); xl_settings = normalized_settings.Get();
+argName = "settings"; Excel::ValidateScriptSettingsRange(xl_settings, "CurveInstrumentCalibrationOptions_New", "settings");
&outputs
value is handle StorableCurveInstrumentCalibrationOptions
    Owning session settings
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CalibrationSamplingSpec_New
    Create immutable typed calibration settings
&inputs
settings is cell[][]
    Exact field/value rows; nested values are typed handles
+const Excel::ScriptSettingsInput_ normalized_settings(xl_settings); xl_settings = normalized_settings.Get();
+argName = "settings"; Excel::ValidateScriptSettingsRange(xl_settings, "CalibrationSamplingSpec_New", "settings");
&outputs
value is handle StorableCalibrationSamplingSpec
    Owning session settings
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CalibrationEvaluationOptions_New
    Create immutable typed calibration settings
&inputs
settings is cell[][]
    Exact field/value rows; nested values are typed handles
+const Excel::ScriptSettingsInput_ normalized_settings(xl_settings); xl_settings = normalized_settings.Get();
+argName = "settings"; Excel::ValidateScriptSettingsRange(xl_settings, "CalibrationEvaluationOptions_New", "settings");
&outputs
value is handle StorableCalibrationEvaluationOptions
    Owning session settings
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CalibrationPreparationContext_New
    Create immutable typed calibration settings
&inputs
settings is cell[][]
    Exact field/value rows; nested values are typed handles
+const Excel::ScriptSettingsInput_ normalized_settings(xl_settings); xl_settings = normalized_settings.Get();
+argName = "settings"; Excel::ValidateScriptSettingsRange(xl_settings, "CalibrationPreparationContext_New", "settings");
&outputs
value is handle StorableCalibrationPreparationContext
    Owning session settings
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CalibrationMonteCarloSampling_New
    Create immutable typed calibration settings
&inputs
settings is cell[][]
    Exact field/value rows; nested values are typed handles
+const Excel::ScriptSettingsInput_ normalized_settings(xl_settings); xl_settings = normalized_settings.Get();
+argName = "settings"; Excel::ValidateScriptSettingsRange(xl_settings, "CalibrationMonteCarloSampling_New", "settings");
&outputs
value is handle StorableCalibrationMonteCarloSampling
    Owning session settings
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CalibrationSimulationSettings_New
    Create immutable typed calibration settings
&inputs
settings is cell[][]
    Exact field/value rows; nested values are typed handles
+const Excel::ScriptSettingsInput_ normalized_settings(xl_settings); xl_settings = normalized_settings.Get();
+argName = "settings"; Excel::ValidateScriptSettingsRange(xl_settings, "CalibrationSimulationSettings_New", "settings");
&outputs
value is handle StorableCalibrationSimulationSettings
    Owning session settings
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CalibrationMonteCarloRequest_New
    Create immutable typed calibration settings
&inputs
settings is cell[][]
    Exact field/value rows; nested values are typed handles
+const Excel::ScriptSettingsInput_ normalized_settings(xl_settings); xl_settings = normalized_settings.Get();
+argName = "settings"; Excel::ValidateScriptSettingsRange(xl_settings, "CalibrationMonteCarloRequest_New", "settings");
&outputs
value is handle StorableCalibrationMonteCarloRequest
    Owning session settings
-IF-------------------------------------------------------------------------*/

namespace Dal {
    using Excel::Calibration::Fields_;
    void ScriptInstrumentImportSettings_New(const Matrix_<Cell_>& settings, Handle_<StorableScriptInstrumentImportSettings_>* value) {
        Fields_ f(settings);
        ScriptInstrumentImportSettings_ v;
        v.instrumentId_ = f.Required<String_>("instrument_id");
        v.currency_ = f.Required<Ccy_>("currency");
        v.notional_ = f.Required<double>("notional");
        v.discountRoute_ = f.Required<String_>("discount_route");
        v.minAbsAnnuity_ = f.Maybe<double>("min_abs_annuity");
        v.pillar_ = f.Maybe<Date_>("pillar");
        f.Finish();
        value->reset(new StorableScriptInstrumentImportSettings_(v));
    }
    void CurveInstrumentGeometry_New(const Matrix_<Cell_>& settings, Handle_<StorableCurveInstrumentGeometry_>* value) {
        Fields_ f(settings);
        const auto v = BindingInternal::ReadCurveInstrumentGeometry(f);
        f.Finish();
        value->reset(new StorableCurveInstrumentGeometry_(v));
    }
    void CurveInstrumentMarket_New(const Matrix_<Cell_>& settings, Handle_<StorableCurveInstrumentMarket_>* value) {
        Fields_ f(settings);
        CurveInstrumentMarket_ v;
        v.routes_ = f.Required<Vector_<CurveScriptRoute_>>("routes");
        v.discountCurves_ = f.Default<std::map<CollateralType_, Handle_<DiscountCurve_>>>("discount_curves", v.discountCurves_);
        v.forwardCurves_ = f.Default<std::map<PeriodLength_, Handle_<DiscountCurve_>>>("forward_curves", v.forwardCurves_);
        f.Finish();
        value->reset(new StorableCurveInstrumentMarket_(v));
    }
    void CurveInstrumentSolver_New(const Matrix_<Cell_>& settings, Handle_<StorableCurveInstrumentSolver_>* value) {
        Fields_ f(settings);
        CurveInstrumentSolver_ v;
        v.solveMode_ = f.Default<CurveSolveMode_>("solve_mode", v.solveMode_);
        v.initialGuess_ = f.Default<double>("initial_guess", v.initialGuess_);
        v.smoothingWeight_ = f.Default<double>("smoothing_weight", v.smoothingWeight_);
        v.initialGuessPerNode_ = f.Default<Vector_<>>("initial_guess_per_node", v.initialGuessPerNode_);
        v.maxEvaluations_ = f.Default<int>("max_evaluations", v.maxEvaluations_);
        v.maxRestarts_ = f.Default<int>("max_restarts", v.maxRestarts_);
        v.parameterScales_ = f.Default<Vector_<>>("parameter_scales", v.parameterScales_);
        v.fitWeight_ = f.Maybe<double>("fit_weight");
        v.fitNormTolerance_ = f.Maybe<double>("fit_norm_tolerance");
        f.Finish();
        value->reset(new StorableCurveInstrumentSolver_(v));
    }
    void CurveInstrumentCalibrationSpec_New(const Matrix_<Cell_>& settings, Handle_<StorableCurveInstrumentCalibrationSpec_>* value) {
        Fields_ f(settings);
        CurveInstrumentCalibrationSpec_ v;
        v.geometry_ = f.Required<CurveInstrumentGeometry_>("geometry");
        v.market_ = f.Required<CurveInstrumentMarket_>("market");
        v.valuation_ = f.Required<ScriptValuationSettings_>("valuation");
        v.observations_ = f.Required<Vector_<Handle_<CalibrationObservation_>>>("observations");
        v.solver_ = f.Default<CurveInstrumentSolver_>("solver", v.solver_);
        f.Finish();
        value->reset(new StorableCurveInstrumentCalibrationSpec_(v));
    }
    void CurveInstrumentCalibrationOptions_New(const Matrix_<Cell_>& settings, Handle_<StorableCurveInstrumentCalibrationOptions_>* value) {
        Fields_ f(settings);
        CurveInstrumentCalibrationOptions_ v;
        v.evaluation_ = f.Default<CalibrationEvaluation_>("evaluation", v.evaluation_);
        v.jacobianMode_ = f.Default<CalibrationDerivative_>("jacobian_mode", v.jacobianMode_);
        v.differenceStep_ = f.Default<double>("difference_step", v.differenceStep_);
        v.computeQuoteJacobian_ = f.Default<bool>("compute_quote_jacobian", v.computeQuoteJacobian_);
        f.Finish();
        value->reset(new StorableCurveInstrumentCalibrationOptions_(v));
    }
    void CalibrationSamplingSpec_New(const Matrix_<Cell_>& settings, Handle_<StorableCalibrationSamplingSpec_>* value) {
        Fields_ f(settings);
        CalibrationSamplingSpec_ v;
        v.paths_ = f.Required<int>("paths");
        v.seed_ = f.Required<int>("seed");
        v.generator_ = f.Default<String_>("generator", v.generator_);
        f.Finish();
        value->reset(new StorableCalibrationSamplingSpec_(v));
    }
    void CalibrationEvaluationOptions_New(const Matrix_<Cell_>& settings, Handle_<StorableCalibrationEvaluationOptions_>* value) {
        Fields_ f(settings);
        CalibrationEvaluationOptions_ v;
        v.evaluation_ = f.Default<CalibrationEvaluation_>("evaluation", v.evaluation_);
        v.derivativeMethod_ = f.Default<CalibrationDerivative_>("derivative_method", v.derivativeMethod_);
        v.allowMonteCarloCalibration_ = f.Default<bool>("allow_monte_carlo_calibration", v.allowMonteCarloCalibration_);
        v.sampling_ = f.Maybe<CalibrationSamplingSpec_>("sampling");
        f.Finish();
        value->reset(new StorableCalibrationEvaluationOptions_(v));
    }
    void CalibrationPreparationContext_New(const Matrix_<Cell_>& settings, Handle_<StorableCalibrationPreparationContext_>* value) {
        Fields_ f(settings);
        CalibrationPreparationContext_ v;
        v.valuation_ = f.Required<ScriptValuationSettings_>("valuation");
        v.currency_ = f.Required<Ccy_>("currency");
        v.discountRoute_ = f.Required<String_>("discount_route");
        v.curve_ = f.Required<Handle_<GSRCurveData_>>("curve");
        v.model_ = f.Maybe<Handle_<ModelData_>>("model").value_or(Handle_<ModelData_>());
        v.conventionState_ = f.Default<String_>("convention_state", v.conventionState_);
        v.smoothing_ = f.Default<double>("smoothing", v.smoothing_);
        f.Finish();
        value->reset(new StorableCalibrationPreparationContext_(v));
    }
    void CalibrationMonteCarloSampling_New(const Matrix_<Cell_>& settings, Handle_<StorableCalibrationMonteCarloSampling_>* value) {
        Fields_ f(settings);
        CalibrationMonteCarloSampling_ v;
        v.mode_ = f.Default<String_>("mode", v.mode_);
        v.pathsPerBatch_ = f.Required<int>("paths_per_batch");
        v.batches_ = f.Required<int>("batches");
        v.seed_ = f.Required<int>("seed");
        v.streamVersion_ = f.Required<String_>("stream_version");
        f.Finish();
        value->reset(new StorableCalibrationMonteCarloSampling_(v));
    }
    void CalibrationSimulationSettings_New(const Matrix_<Cell_>& settings, Handle_<StorableCalibrationSimulationSettings_>* value) {
        Fields_ f(settings);
        CalibrationSimulationSettings_ v;
        v.useBb_ = f.Default<bool>("use_bb", v.useBb_);
        v.compiled_ = f.Maybe<bool>("compiled");
        v.smooth_ = f.Default<double>("smooth", v.smooth_);
        v.normalPrecision_ = f.Default<String_>("normal_precision", v.normalPrecision_);
        f.Finish();
        value->reset(new StorableCalibrationSimulationSettings_(v));
    }
    void CalibrationMonteCarloRequest_New(const Matrix_<Cell_>& settings, Handle_<StorableCalibrationMonteCarloRequest_>* value) {
        Fields_ f(settings);
        CalibrationMonteCarloRequest_ v;
        v.measureId_ = f.Required<String_>("measure_id");
        v.valuation_ = f.Required<ScriptValuationSettings_>("valuation");
        v.sampling_ = f.Required<CalibrationMonteCarloSampling_>("sampling");
        v.simulation_ = f.Default<CalibrationSimulationSettings_>("simulation", v.simulation_);
        v.derivativeMethod_ = f.Default<CalibrationDerivative_>("derivative_method", v.derivativeMethod_);
        f.Finish();
        value->reset(new StorableCalibrationMonteCarloRequest_(v));
    }
#ifdef _WIN32
#include <dal-excel/auto/MG_CalibrationEvaluationOptions_New_public.inc>
#include <dal-excel/auto/MG_CalibrationMonteCarloRequest_New_public.inc>
#include <dal-excel/auto/MG_CalibrationMonteCarloSampling_New_public.inc>
#include <dal-excel/auto/MG_CalibrationPreparationContext_New_public.inc>
#include <dal-excel/auto/MG_CalibrationSamplingSpec_New_public.inc>
#include <dal-excel/auto/MG_CalibrationSimulationSettings_New_public.inc>
#include <dal-excel/auto/MG_CurveInstrumentCalibrationOptions_New_public.inc>
#include <dal-excel/auto/MG_CurveInstrumentCalibrationSpec_New_public.inc>
#include <dal-excel/auto/MG_CurveInstrumentGeometry_New_public.inc>
#include <dal-excel/auto/MG_CurveInstrumentMarket_New_public.inc>
#include <dal-excel/auto/MG_CurveInstrumentSolver_New_public.inc>
#include <dal-excel/auto/MG_ScriptInstrumentImportSettings_New_public.inc>
#endif
} // namespace Dal

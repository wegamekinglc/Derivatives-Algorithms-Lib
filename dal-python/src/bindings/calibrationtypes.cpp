#include "bindings.h"
#include "calibrationinput.hpp"

using namespace Dal;
using namespace Dal::Python;
using namespace Dal::Python::Calibration;

namespace {
    void RegisterDefinitionTypes(py::module_& m) {
        {
            auto cls = Struct<CalibrationValueUnit_>(m, "CalibrationValueUnit_", [](Fields_& f) {
                CalibrationValueUnit_ v;
                v.kind_ = f.Required<CalibrationUnitKind_>("kind");
                v.currency_ = f.Maybe<Ccy_>("currency");
                v.baseCurrency_ = f.Maybe<Ccy_>("base_currency");
                v.quoteCurrency_ = f.Maybe<Ccy_>("quote_currency");
                static_cast<void>(v.Token());
                return v;
            });
            Property(cls, "kind", &CalibrationValueUnit_::kind_);
            Property(cls, "currency", &CalibrationValueUnit_::currency_);
            Property(cls, "base_currency", &CalibrationValueUnit_::baseCurrency_);
            Property(cls, "quote_currency", &CalibrationValueUnit_::quoteCurrency_);
            cls.def_property_readonly("token", [](const CalibrationValueUnit_& v) { return Text(v.Token()); });
        }
        {
            auto cls = Struct<CalibrationQuoteConvention_>(m, "CalibrationQuoteConvention_", [](Fields_& f) {
                return CalibrationQuoteConvention_{f.Required<CalibrationQuoteConventionId_>("id"), f.Default<int>("version", 1)};
            });
            Property(cls, "id", &CalibrationQuoteConvention_::id_);
            Property(cls, "version", &CalibrationQuoteConvention_::version_);
        }
        {
            auto cls = Struct<ScriptSourceRow_>(m, "ScriptSourceRow_", [](Fields_& f) {
                ScriptSourceRow_ v;
                v.kind_ = f.Required<ScriptSourceRowKind_>("kind");
                v.key_ = f.Required<String_>("key");
                v.text_ = f.Required<String_>("text");
                return v;
            });
            Property(cls, "kind", &ScriptSourceRow_::kind_);
            Property(cls, "key", &ScriptSourceRow_::key_);
            Property(cls, "text", &ScriptSourceRow_::text_);
        }
        {
            auto cls = Struct<ScriptInstrumentSource_>(m, "ScriptInstrumentSource_", [](Fields_& f) {
                ScriptInstrumentSource_ v;
                v.rows_ = f.Required<Vector_<ScriptSourceRow_>>("rows");
                v.settings_ = f.Default<ScriptProductSettings_>("settings", v.settings_);
                return v;
            });
            Property(cls, "rows", &ScriptInstrumentSource_::rows_);
            Property(cls, "settings", &ScriptInstrumentSource_::settings_);
        }
        {
            auto cls = Struct<ScriptInstrumentHeader_>(m, "ScriptInstrumentHeader_", [](Fields_& f) {
                ScriptInstrumentHeader_ v;
                v.payCurrency_ = f.Required<Ccy_>("pay_currency");
                v.resultCurrency_ = f.Required<Ccy_>("result_currency");
                v.collateralCurrency_ = f.Required<Ccy_>("collateral_currency");
                v.collateralType_ = f.Default<CollateralType_>("collateral_type", v.collateralType_);
                v.receiver_ = f.Default<String_>("receiver", v.receiver_);
                v.notionalParameter_ = f.Default<String_>("notional_parameter", v.notionalParameter_);
                v.settlement_ = f.Default<ScriptSettlement_>("settlement", v.settlement_);
                v.valuationCashflows_ = f.Default<ScriptValuationCashflows_>("valuation_cashflows", v.valuationCashflows_);
                v.discountRoute_ = f.Required<String_>("discount_route");
                v.conventionSet_ = f.Default<String_>("convention_set", v.conventionSet_);
                return v;
            });
            Property(cls, "pay_currency", &ScriptInstrumentHeader_::payCurrency_);
            Property(cls, "result_currency", &ScriptInstrumentHeader_::resultCurrency_);
            Property(cls, "collateral_currency", &ScriptInstrumentHeader_::collateralCurrency_);
            Property(cls, "collateral_type", &ScriptInstrumentHeader_::collateralType_);
            Property(cls, "receiver", &ScriptInstrumentHeader_::receiver_);
            Property(cls, "notional_parameter", &ScriptInstrumentHeader_::notionalParameter_);
            Property(cls, "settlement", &ScriptInstrumentHeader_::settlement_);
            Property(cls, "valuation_cashflows", &ScriptInstrumentHeader_::valuationCashflows_);
            Property(cls, "discount_route", &ScriptInstrumentHeader_::discountRoute_);
            Property(cls, "convention_set", &ScriptInstrumentHeader_::conventionSet_);
        }
        {
            auto cls = Struct<ScriptParameterDeclaration_>(m, "ScriptParameterDeclaration_", [](Fields_& f) {
                ScriptParameterDeclaration_ v;
                v.key_ = f.Required<String_>("key");
                v.type_ = f.Required<ScriptParameterType_>("type");
                v.unit_ = f.Required<CalibrationValueUnit_>("unit");
                v.role_ = f.Required<ScriptParameterRole_>("role");
                v.bindable_ = f.Required<bool>("bindable");
                v.lower_ = f.Maybe<double>("lower");
                v.upper_ = f.Maybe<double>("upper");
                v.lowerInclusive_ = f.Default<bool>("lower_inclusive", v.lowerInclusive_);
                v.upperInclusive_ = f.Default<bool>("upper_inclusive", v.upperInclusive_);
                return v;
            });
            Property(cls, "key", &ScriptParameterDeclaration_::key_);
            Property(cls, "type", &ScriptParameterDeclaration_::type_);
            Property(cls, "unit", &ScriptParameterDeclaration_::unit_);
            Property(cls, "role", &ScriptParameterDeclaration_::role_);
            Property(cls, "bindable", &ScriptParameterDeclaration_::bindable_);
            Property(cls, "lower", &ScriptParameterDeclaration_::lower_);
            Property(cls, "upper", &ScriptParameterDeclaration_::upper_);
            Property(cls, "lower_inclusive", &ScriptParameterDeclaration_::lowerInclusive_);
            Property(cls, "upper_inclusive", &ScriptParameterDeclaration_::upperInclusive_);
        }
        {
            auto cls = Struct<CalibrationMeasureSpec_>(m, "CalibrationMeasureSpec_", [](Fields_& f) {
                CalibrationMeasureSpec_ v;
                v.id_ = f.Required<String_>("id");
                v.kind_ = f.Required<CalibrationMeasureKind_>("kind");
                v.receiver_ = f.Required<String_>("receiver");
                v.parameter_ = f.Maybe<String_>("parameter");
                v.unit_ = f.Required<CalibrationValueUnit_>("unit");
                v.annuitySign_ = f.Maybe<int>("annuity_sign");
                v.minAbsAnnuity_ = f.Maybe<double>("min_abs_annuity");
                return v;
            });
            Property(cls, "id", &CalibrationMeasureSpec_::id_);
            Property(cls, "kind", &CalibrationMeasureSpec_::kind_);
            Property(cls, "receiver", &CalibrationMeasureSpec_::receiver_);
            Property(cls, "parameter", &CalibrationMeasureSpec_::parameter_);
            Property(cls, "unit", &CalibrationMeasureSpec_::unit_);
            Property(cls, "annuity_sign", &CalibrationMeasureSpec_::annuitySign_);
            Property(cls, "min_abs_annuity", &CalibrationMeasureSpec_::minAbsAnnuity_);
        }
        {
            auto cls = Struct<ScriptInstrumentDefinitionSpec_>(m, "ScriptInstrumentDefinitionSpec_", [](Fields_& f) {
                ScriptInstrumentDefinitionSpec_ v;
                v.source_ = f.Required<ScriptInstrumentSource_>("source");
                v.header_ = f.Required<ScriptInstrumentHeader_>("header");
                v.parameters_ = f.Required<Vector_<ScriptParameterDeclaration_>>("parameters");
                v.measures_ = f.Required<Vector_<CalibrationMeasureSpec_>>("measures");
                return v;
            });
            Property(cls, "source", &ScriptInstrumentDefinitionSpec_::source_);
            Property(cls, "header", &ScriptInstrumentDefinitionSpec_::header_);
            Property(cls, "parameters", &ScriptInstrumentDefinitionSpec_::parameters_);
            Property(cls, "measures", &ScriptInstrumentDefinitionSpec_::measures_);
        }
    }
    void RegisterBindingTypes(py::module_& m) {
        {
            auto cls = Struct<CalibrationParameterValue_>(m, "CalibrationParameterValue_", [](Fields_& f) {
                CalibrationParameterValue_ v;
                v.key_ = f.Required<String_>("key");
                v.value_ = f.Required<double>("value");
                v.unit_ = f.Required<CalibrationValueUnit_>("unit");
                return v;
            });
            Property(cls, "key", &CalibrationParameterValue_::key_);
            Property(cls, "value", &CalibrationParameterValue_::value_);
            Property(cls, "unit", &CalibrationParameterValue_::unit_);
        }
        {
            auto cls = Struct<CalibrationQuoteCopy_>(m, "CalibrationQuoteCopy_", [](Fields_& f) {
                CalibrationQuoteCopy_ v;
                v.parameterKey_ = f.Required<String_>("parameter_key");
                v.quoteId_ = f.Required<String_>("quote_id");
                v.quoteFingerprint_ = f.Required<String_>("quote_fingerprint");
                return v;
            });
            Property(cls, "parameter_key", &CalibrationQuoteCopy_::parameterKey_);
            Property(cls, "quote_id", &CalibrationQuoteCopy_::quoteId_);
            Property(cls, "quote_fingerprint", &CalibrationQuoteCopy_::quoteFingerprint_);
        }
        {
            auto cls = Struct<CalibrationParameterBindings_>(m, "CalibrationParameterBindings_", [](Fields_& f) {
                CalibrationParameterBindings_ v;
                v.values_ = f.Default<Vector_<CalibrationParameterValue_>>("values", v.values_);
                v.quoteCopies_ = f.Default<Vector_<CalibrationQuoteCopy_>>("quote_copies", v.quoteCopies_);
                return v;
            });
            Property(cls, "values", &CalibrationParameterBindings_::values_);
            Property(cls, "quote_copies", &CalibrationParameterBindings_::quoteCopies_);
        }
        {
            auto cls = Struct<ScriptCalibrationInstrumentSpec_>(m, "ScriptCalibrationInstrumentSpec_", [](Fields_& f) {
                ScriptCalibrationInstrumentSpec_ v;
                v.id_ = f.Required<String_>("id");
                v.definition_ = f.Required<Handle_<ScriptInstrumentDefinition_>>("definition");
                v.bindings_ = f.Default<CalibrationParameterBindings_>("bindings", v.bindings_);
                v.pillar_ = f.Maybe<Date_>("pillar");
                return v;
            });
            Property(cls, "id", &ScriptCalibrationInstrumentSpec_::id_);
            Property(cls, "definition", &ScriptCalibrationInstrumentSpec_::definition_);
            Property(cls, "bindings", &ScriptCalibrationInstrumentSpec_::bindings_);
            Property(cls, "pillar", &ScriptCalibrationInstrumentSpec_::pillar_);
        }
        {
            auto cls = Struct<ScriptInstrumentImportSettings_>(m, "ScriptInstrumentImportSettings_", [](Fields_& f) {
                ScriptInstrumentImportSettings_ v;
                v.instrumentId_ = f.Required<String_>("instrument_id");
                v.currency_ = f.Required<Ccy_>("currency");
                v.notional_ = f.Required<double>("notional");
                v.discountRoute_ = f.Required<String_>("discount_route");
                v.minAbsAnnuity_ = f.Maybe<double>("min_abs_annuity");
                v.pillar_ = f.Maybe<Date_>("pillar");
                return v;
            });
            Property(cls, "instrument_id", &ScriptInstrumentImportSettings_::instrumentId_);
            Property(cls, "currency", &ScriptInstrumentImportSettings_::currency_);
            Property(cls, "notional", &ScriptInstrumentImportSettings_::notional_);
            Property(cls, "discount_route", &ScriptInstrumentImportSettings_::discountRoute_);
            Property(cls, "min_abs_annuity", &ScriptInstrumentImportSettings_::minAbsAnnuity_);
            Property(cls, "pillar", &ScriptInstrumentImportSettings_::pillar_);
        }
    }
    void RegisterCurveTypes(py::module_& m) {
        {
            auto cls = Struct<CurveScriptRoute_>(m, "CurveScriptRoute_", [](Fields_& f) {
                CurveScriptRoute_ v;
                v.id_ = f.Required<String_>("id");
                v.projection_ = f.Default<bool>("projection", v.projection_);
                v.collateral_ = f.Default<CollateralType_>("collateral", v.collateral_);
                v.tenor_ = f.Default<PeriodLength_>("tenor", v.tenor_);
                return v;
            });
            Property(cls, "id", &CurveScriptRoute_::id_);
            Property(cls, "projection", &CurveScriptRoute_::projection_);
            Property(cls, "collateral", &CurveScriptRoute_::collateral_);
            Property(cls, "tenor", &CurveScriptRoute_::tenor_);
        }
        {
            auto cls = Struct<CurveInstrumentGeometry_>(m, "CurveInstrumentGeometry_", [](Fields_& f) {
                CurveInstrumentGeometry_ v;
                v.today_ = f.Required<Date_>("today");
                v.ccy_ = f.Required<String_>("currency");
                v.curveName_ = f.Default<String_>("curve_name", v.curveName_);
                v.knotDates_ = f.Required<Vector_<Date_>>("knot_dates");
                v.parameterization_ = f.Default<CurveParameterization_>("parameterization", v.parameterization_);
                v.knotPolicy_ = f.Default<CurveKnotPolicy_>("knot_policy", v.knotPolicy_);
                v.logDfScheme_ = f.Default<LogDfScheme_>("log_df_scheme", v.logDfScheme_);
                v.baseCurve_ = f.Maybe<Handle_<DiscountCurve_>>("base_curve").value_or(Handle_<DiscountCurve_>());
                v.calibrateDiscountCurve_ = f.Default<bool>("calibrate_discount_curve", v.calibrateDiscountCurve_);
                v.targetCollateral_ = f.Default<CollateralType_>("target_collateral", v.targetCollateral_);
                v.targetTenor_ = f.Default<PeriodLength_>("target_tenor", v.targetTenor_);
                v.liborBasis_ = f.Default<DayBasis_>("libor_basis", v.liborBasis_);
                return v;
            });
            Property(cls, "today", &CurveInstrumentGeometry_::today_);
            Property(cls, "currency", &CurveInstrumentGeometry_::ccy_);
            Property(cls, "curve_name", &CurveInstrumentGeometry_::curveName_);
            Property(cls, "knot_dates", &CurveInstrumentGeometry_::knotDates_);
            Property(cls, "parameterization", &CurveInstrumentGeometry_::parameterization_);
            Property(cls, "knot_policy", &CurveInstrumentGeometry_::knotPolicy_);
            Property(cls, "log_df_scheme", &CurveInstrumentGeometry_::logDfScheme_);
            Property(cls, "base_curve", &CurveInstrumentGeometry_::baseCurve_);
            Property(cls, "calibrate_discount_curve", &CurveInstrumentGeometry_::calibrateDiscountCurve_);
            Property(cls, "target_collateral", &CurveInstrumentGeometry_::targetCollateral_);
            Property(cls, "target_tenor", &CurveInstrumentGeometry_::targetTenor_);
            Property(cls, "libor_basis", &CurveInstrumentGeometry_::liborBasis_);
        }
        {
            auto cls = Struct<CurveInstrumentMarket_>(m, "CurveInstrumentMarket_", [](Fields_& f) {
                CurveInstrumentMarket_ v;
                v.routes_ = f.Required<Vector_<CurveScriptRoute_>>("routes");
                v.discountCurves_ = f.Default<std::map<CollateralType_, Handle_<DiscountCurve_>>>("discount_curves", v.discountCurves_);
                v.forwardCurves_ = f.Default<std::map<PeriodLength_, Handle_<DiscountCurve_>>>("forward_curves", v.forwardCurves_);
                return v;
            });
            Property(cls, "routes", &CurveInstrumentMarket_::routes_);
            Property(cls, "discount_curves", &CurveInstrumentMarket_::discountCurves_);
            Property(cls, "forward_curves", &CurveInstrumentMarket_::forwardCurves_);
        }
        {
            auto cls = Struct<CurveInstrumentSolver_>(m, "CurveInstrumentSolver_", [](Fields_& f) {
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
                return v;
            });
            Property(cls, "solve_mode", &CurveInstrumentSolver_::solveMode_);
            Property(cls, "initial_guess", &CurveInstrumentSolver_::initialGuess_);
            Property(cls, "smoothing_weight", &CurveInstrumentSolver_::smoothingWeight_);
            Property(cls, "initial_guess_per_node", &CurveInstrumentSolver_::initialGuessPerNode_);
            Property(cls, "max_evaluations", &CurveInstrumentSolver_::maxEvaluations_);
            Property(cls, "max_restarts", &CurveInstrumentSolver_::maxRestarts_);
            Property(cls, "parameter_scales", &CurveInstrumentSolver_::parameterScales_);
            Property(cls, "fit_weight", &CurveInstrumentSolver_::fitWeight_);
            Property(cls, "fit_norm_tolerance", &CurveInstrumentSolver_::fitNormTolerance_);
        }
        {
            auto cls = Struct<CurveInstrumentCalibrationSpec_>(m, "CurveInstrumentCalibrationSpec_", [](Fields_& f) {
                CurveInstrumentCalibrationSpec_ v;
                v.geometry_ = f.Required<CurveInstrumentGeometry_>("geometry");
                v.market_ = f.Required<CurveInstrumentMarket_>("market");
                v.valuation_ = f.Required<ScriptValuationSettings_>("valuation");
                v.observations_ = f.Required<Vector_<Handle_<CalibrationObservation_>>>("observations");
                v.solver_ = f.Default<CurveInstrumentSolver_>("solver", v.solver_);
                return v;
            });
            Property(cls, "geometry", &CurveInstrumentCalibrationSpec_::geometry_);
            Property(cls, "market", &CurveInstrumentCalibrationSpec_::market_);
            Property(cls, "valuation", &CurveInstrumentCalibrationSpec_::valuation_);
            Property(cls, "observations", &CurveInstrumentCalibrationSpec_::observations_);
            Property(cls, "solver", &CurveInstrumentCalibrationSpec_::solver_);
        }
    }
    void RegisterExecutionTypes(py::module_& m) {
        {
            auto cls = Struct<CurveInstrumentCalibrationOptions_>(m, "CurveInstrumentCalibrationOptions_", [](Fields_& f) {
                CurveInstrumentCalibrationOptions_ v;
                v.evaluation_ = f.Default<CalibrationEvaluation_>("evaluation", v.evaluation_);
                v.jacobianMode_ = f.Default<CalibrationDerivative_>("jacobian_mode", v.jacobianMode_);
                v.differenceStep_ = f.Default<double>("difference_step", v.differenceStep_);
                v.computeQuoteJacobian_ = f.Default<bool>("compute_quote_jacobian", v.computeQuoteJacobian_);
                return v;
            });
            Property(cls, "evaluation", &CurveInstrumentCalibrationOptions_::evaluation_);
            Property(cls, "jacobian_mode", &CurveInstrumentCalibrationOptions_::jacobianMode_);
            Property(cls, "difference_step", &CurveInstrumentCalibrationOptions_::differenceStep_);
            Property(cls, "compute_quote_jacobian", &CurveInstrumentCalibrationOptions_::computeQuoteJacobian_);
        }
        {
            auto cls = Struct<CalibrationSamplingSpec_>(m, "CalibrationSamplingSpec_", [](Fields_& f) {
                CalibrationSamplingSpec_ v;
                v.paths_ = f.Required<int>("paths");
                v.seed_ = f.Required<int>("seed");
                v.generator_ = f.Default<String_>("generator", v.generator_);
                return v;
            });
            Property(cls, "paths", &CalibrationSamplingSpec_::paths_);
            Property(cls, "seed", &CalibrationSamplingSpec_::seed_);
            Property(cls, "generator", &CalibrationSamplingSpec_::generator_);
        }
        {
            auto cls = Struct<CalibrationEvaluationOptions_>(m, "CalibrationEvaluationOptions_", [](Fields_& f) {
                CalibrationEvaluationOptions_ v;
                v.evaluation_ = f.Default<CalibrationEvaluation_>("evaluation", v.evaluation_);
                v.derivativeMethod_ = f.Default<CalibrationDerivative_>("derivative_method", v.derivativeMethod_);
                v.allowMonteCarloCalibration_ = f.Default<bool>("allow_monte_carlo_calibration", v.allowMonteCarloCalibration_);
                v.sampling_ = f.Maybe<CalibrationSamplingSpec_>("sampling");
                return v;
            });
            Property(cls, "evaluation", &CalibrationEvaluationOptions_::evaluation_);
            Property(cls, "derivative_method", &CalibrationEvaluationOptions_::derivativeMethod_);
            Property(cls, "allow_monte_carlo_calibration", &CalibrationEvaluationOptions_::allowMonteCarloCalibration_);
            Property(cls, "sampling", &CalibrationEvaluationOptions_::sampling_);
        }
        {
            auto cls = Struct<CalibrationPreparationContext_>(m, "CalibrationPreparationContext_", [](Fields_& f) {
                CalibrationPreparationContext_ v;
                v.valuation_ = f.Required<ScriptValuationSettings_>("valuation");
                v.currency_ = f.Required<Ccy_>("currency");
                v.discountRoute_ = f.Required<String_>("discount_route");
                v.curve_ = f.Required<Handle_<GSRCurveData_>>("curve");
                v.model_ = f.Maybe<Handle_<ModelData_>>("model").value_or(Handle_<ModelData_>());
                v.conventionState_ = f.Default<String_>("convention_state", v.conventionState_);
                v.smoothing_ = f.Default<double>("smoothing", v.smoothing_);
                return v;
            });
            Property(cls, "valuation", &CalibrationPreparationContext_::valuation_);
            Property(cls, "currency", &CalibrationPreparationContext_::currency_);
            Property(cls, "discount_route", &CalibrationPreparationContext_::discountRoute_);
            Property(cls, "curve", &CalibrationPreparationContext_::curve_);
            Property(cls, "model", &CalibrationPreparationContext_::model_);
            Property(cls, "convention_state", &CalibrationPreparationContext_::conventionState_);
            Property(cls, "smoothing", &CalibrationPreparationContext_::smoothing_);
        }
        {
            auto cls = Struct<CalibrationMonteCarloSampling_>(m, "CalibrationMonteCarloSampling_", [](Fields_& f) {
                CalibrationMonteCarloSampling_ v;
                v.mode_ = f.Default<String_>("mode", v.mode_);
                v.pathsPerBatch_ = f.Required<int>("paths_per_batch");
                v.batches_ = f.Required<int>("batches");
                v.seed_ = f.Required<int>("seed");
                v.streamVersion_ = f.Required<String_>("stream_version");
                return v;
            });
            Property(cls, "mode", &CalibrationMonteCarloSampling_::mode_);
            Property(cls, "paths_per_batch", &CalibrationMonteCarloSampling_::pathsPerBatch_);
            Property(cls, "batches", &CalibrationMonteCarloSampling_::batches_);
            Property(cls, "seed", &CalibrationMonteCarloSampling_::seed_);
            Property(cls, "stream_version", &CalibrationMonteCarloSampling_::streamVersion_);
        }
        {
            auto cls = Struct<CalibrationSimulationSettings_>(m, "CalibrationSimulationSettings_", [](Fields_& f) {
                CalibrationSimulationSettings_ v;
                v.useBb_ = f.Default<bool>("use_bb", v.useBb_);
                v.compiled_ = f.Maybe<bool>("compiled");
                v.smooth_ = f.Default<double>("smooth", v.smooth_);
                v.normalPrecision_ = f.Default<String_>("normal_precision", v.normalPrecision_);
                return v;
            });
            Property(cls, "use_bb", &CalibrationSimulationSettings_::useBb_);
            Property(cls, "compiled", &CalibrationSimulationSettings_::compiled_);
            Property(cls, "smooth", &CalibrationSimulationSettings_::smooth_);
            Property(cls, "normal_precision", &CalibrationSimulationSettings_::normalPrecision_);
        }
        {
            auto cls = Struct<CalibrationMonteCarloRequest_>(m, "CalibrationMonteCarloRequest_", [](Fields_& f) {
                CalibrationMonteCarloRequest_ v;
                v.measureId_ = f.Required<String_>("measure_id");
                v.valuation_ = f.Required<ScriptValuationSettings_>("valuation");
                v.sampling_ = f.Required<CalibrationMonteCarloSampling_>("sampling");
                v.simulation_ = f.Default<CalibrationSimulationSettings_>("simulation", v.simulation_);
                v.derivativeMethod_ = f.Default<CalibrationDerivative_>("derivative_method", v.derivativeMethod_);
                return v;
            });
            Property(cls, "measure_id", &CalibrationMonteCarloRequest_::measureId_);
            Property(cls, "valuation", &CalibrationMonteCarloRequest_::valuation_);
            Property(cls, "sampling", &CalibrationMonteCarloRequest_::sampling_);
            Property(cls, "simulation", &CalibrationMonteCarloRequest_::simulation_);
            Property(cls, "derivative_method", &CalibrationMonteCarloRequest_::derivativeMethod_);
        }
    }
} // namespace

void init_calibration_types(py::module_& m) {
    RegisterDefinitionTypes(m);
    RegisterBindingTypes(m);
    RegisterCurveTypes(m);
    RegisterExecutionTypes(m);
}

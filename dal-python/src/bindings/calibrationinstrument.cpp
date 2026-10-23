#include "bindings.h"

#include "calibrationinput.hpp"
#include "giltestbarrier.hpp"
#include <dal-public/src/xccycalibration.hpp>
#include <dal-public/src/xccymontecarlo.hpp>
#include <pybind11/eval.h>

using namespace Dal;
using namespace Dal::Python;
using namespace Dal::Python::Calibration;

void init_calibration_types(py::module_& m);
void init_xccy_instruments(py::module_& m);
void init_xccy_monte_carlo(py::module_& m);

namespace {
    PyObject* calibrationErrorType = nullptr;
    thread_local bool testBarrierArmed = false;
    GilTestBarrier_ testBarrier;
    constexpr const char* GIL_TIMEOUT = "CalibrationMC: Python GIL test barrier timed out";

    void GilTestHooks(py::module_& m) {
        m.def("_CalibrationMcGilBarrier_ArmForTesting", [] { testBarrier.Arm(&testBarrierArmed); });
        m.def("_CalibrationMcGilBarrier_AwaitEntryForTesting", [] {
            py::gil_scoped_release release;
            return testBarrier.AwaitEntry();
        });
        m.def("_CalibrationMcGilBarrier_ReleaseForTesting", [] { return testBarrier.Release(); });
        m.def("_CalibrationMcGilBarrier_BlockWithGilForTesting", [] { testBarrier.Wait(&testBarrierArmed, GIL_TIMEOUT); });
    }

    template <class T_> std::shared_ptr<T_> Mutable(const Handle_<T_>& value) { return std::const_pointer_cast<T_>(value); }
    template <class T_> T_ Read(const py::handle& value, const char* field) { return Input_<T_>::Read(value, field); }
    template <class T_> T_ DefaultInput(const py::handle& value, const char* field) { return value.is_none() ? T_() : Read<T_>(value, field); }

    py::object Candidate(const Handle_<Storable_>& value) {
        if (const auto* xccy = dynamic_cast<const XccyInstrumentCalibrationCandidate_*>(value.get()))
            return Output(xccy->Result().curve_);
        if (const auto* curve = dynamic_cast<const CurveInstrumentCalibrationCandidate_*>(value.get()))
            return Output(curve->Result().curve_);
        if (const auto model = handle_cast<ModelData_>(value))
            return Output(model);
        return py::none();
    }
    void TranslateCalibrationError(std::exception_ptr exception) {
        try {
            if (exception)
                std::rethrow_exception(exception);
        } catch (const CalibrationError_& error) {
            const auto& failure = error.Failure();
            const auto type = py::reinterpret_borrow<py::object>(calibrationErrorType);
            const auto instance = type(py::str(error.what()), Text(failure.diagnostic_.code_), py::cast(failure.diagnostic_), Output(failure.report_),
                                       Candidate(failure.candidate_));
            if (const auto* candidate = dynamic_cast<const XccyInstrumentCalibrationCandidate_*>(failure.candidate_.get()))
                instance.attr("_gaussian_model") = Output(candidate->Result().gaussianModel_);
            PyErr_SetObject(calibrationErrorType, instance.ptr());
        }
    }
    py::list Table(const Handle_<CalibrationReport_>& report, const String_& group) {
        Matrix_<Cell_> cells;
        {
            py::gil_scoped_release release;
            cells = CalibrationReportTable(report, group);
        }
        py::list rows;
        for (int i = 0; i < cells.Rows(); ++i) {
            py::list row;
            for (int j = 0; j < cells.Cols(); ++j)
                row.append(std::visit(
                    [](const auto& value) -> py::object {
                        if constexpr (std::is_same_v<std::decay_t<decltype(value)>, std::monostate>)
                            return py::none();
                        else
                            return Output(value);
                    },
                    cells(i, j).val_));
            rows.append(row);
        }
        return rows;
    }
    void ErrorType(py::module_& m) {
        auto cls = py::class_<CalibrationDiagnostic_>(m, "CalibrationDiagnostic_");
        Property(cls, "code", &CalibrationDiagnostic_::code_);
        Property(cls, "message", &CalibrationDiagnostic_::message_);
        Property(cls, "stage", &CalibrationDiagnostic_::stage_);
        Property(cls, "field", &CalibrationDiagnostic_::field_);
        Property(cls, "observation_id", &CalibrationDiagnostic_::observationId_);
        Property(cls, "instrument_id", &CalibrationDiagnostic_::instrumentId_);
        Property(cls, "quote_id", &CalibrationDiagnostic_::quoteId_);
        Property(cls, "measure_id", &CalibrationDiagnostic_::measureId_);
        Property(cls, "parameter_key", &CalibrationDiagnostic_::parameterKey_);
        Property(cls, "source_row", &CalibrationDiagnostic_::sourceRow_);
        Property(cls, "event_date", &CalibrationDiagnostic_::eventDate_);
        Property(cls, "statement", &CalibrationDiagnostic_::statement_);
        Property(cls, "actual", &CalibrationDiagnostic_::actual_);
        Property(cls, "expected", &CalibrationDiagnostic_::expected_);
        py::exec(R"PY(
class CalibrationError(RuntimeError):
    def __init__(self, message, code, diagnostic, report, candidate):
        super().__init__(message)
        self._code, self._diagnostic = code, diagnostic
        self._report, self._candidate = report, candidate
        self._gaussian_model = None
    code = property(lambda self: self._code)
    diagnostic = property(lambda self: self._diagnostic)
    report = property(lambda self: self._report)
    candidate = property(lambda self: self._candidate)
    gaussian_model = property(lambda self: self._gaussian_model)
)PY",
                 m.attr("__dict__"));
        calibrationErrorType = m.attr("CalibrationError").ptr();
        py::register_exception_translator(TranslateCalibrationError);
    }
    void Handles(py::module_& m) {
        py::class_<ScriptInstrumentDefinition_, std::shared_ptr<ScriptInstrumentDefinition_>>(m, "ScriptInstrumentDefinition_")
            .def_property_readonly("source_identity", [](const ScriptInstrumentDefinition_& v) { return Text(v.SourceIdentity()); })
            .def_property_readonly("definition_identity", [](const ScriptInstrumentDefinition_& v) { return Text(v.DefinitionIdentity()); })
            .def_property_readonly("spec", [](const ScriptInstrumentDefinition_& v) { return v.Spec(); });
        py::class_<CalibrationInstrument_, std::shared_ptr<CalibrationInstrument_>>(m, "CalibrationInstrument_")
            .def_property_readonly("id", [](const CalibrationInstrument_& v) { return Text(v.Id()); })
            .def_property_readonly("definition", [](const CalibrationInstrument_& v) { return Mutable(v.Definition()); })
            .def_property_readonly("binding_identity", [](const CalibrationInstrument_& v) { return Text(v.BindingIdentity()); })
            .def_property_readonly("instrument_identity", [](const CalibrationInstrument_& v) { return Text(v.InstrumentIdentity()); })
            .def_property_readonly("pillar", [](const CalibrationInstrument_& v) { return v.Pillar(); })
            .def_property_readonly("binding", [](const CalibrationInstrument_& v) { return Output(v.Binding()); });
        auto binding = py::class_<CalibrationEffectiveParameter_>(m, "CalibrationEffectiveParameter_");
        binding.def_property_readonly("key", [](const CalibrationEffectiveParameter_& v) { return Text(v.key_); })
            .def_property_readonly("value", [](const CalibrationEffectiveParameter_& v) { return v.value_; })
            .def_property_readonly("unit", [](const CalibrationEffectiveParameter_& v) { return v.unit_; });
        Property(binding, "origin", &CalibrationEffectiveParameter_::origin_);
        Property(binding, "quote_id", &CalibrationEffectiveParameter_::quoteId_);
        Property(binding, "quote_fingerprint", &CalibrationEffectiveParameter_::quoteFingerprint_);
        py::class_<CalibrationMarketQuote_, std::shared_ptr<CalibrationMarketQuote_>>(m, "CalibrationMarketQuote_")
            .def_property_readonly("id", [](const CalibrationMarketQuote_& v) { return Text(v.Id()); })
            .def_property_readonly("value", &CalibrationMarketQuote_::Value)
            .def_property_readonly("unit", &CalibrationMarketQuote_::Unit, py::return_value_policy::copy)
            .def_property_readonly("convention", &CalibrationMarketQuote_::Convention, py::return_value_policy::copy)
            .def_property_readonly("fingerprint", [](const CalibrationMarketQuote_& v) { return Text(v.Fingerprint()); });
        py::class_<CalibrationObservation_, std::shared_ptr<CalibrationObservation_>>(m, "CalibrationObservation_")
            .def_property_readonly("id", [](const CalibrationObservation_& v) { return Text(v.Id()); })
            .def_property_readonly("instrument", [](const CalibrationObservation_& v) { return Mutable(v.Instrument()); })
            .def_property_readonly("measure_id", [](const CalibrationObservation_& v) { return Text(v.MeasureId()); })
            .def_property_readonly("quote", [](const CalibrationObservation_& v) { return Mutable(v.Quote()); })
            .def_property_readonly("error_scale", &CalibrationObservation_::ErrorScale);
        py::class_<CalibrationReport_, std::shared_ptr<CalibrationReport_>>(m, "CalibrationReport_")
            .def_property_readonly("quote_risk_available", &CalibrationReport_::QuoteRiskAvailable)
            .def_property_readonly("stage", [](const CalibrationReport_& v) { return std::string(v.Stage().String()); });
        auto result = py::class_<CurveInstrumentCalibrationResult_>(m, "CurveInstrumentCalibrationResult_");
        Property(result, "curve", &CurveInstrumentCalibrationResult_::curve_);
        Property(result, "report", &CurveInstrumentCalibrationResult_::report_);
        Property(result, "parameters", &CurveInstrumentCalibrationResult_::parameters_);
        Property(result, "model_measures", &CurveInstrumentCalibrationResult_::modelMeasures_);
        Property(result, "raw_residuals", &CurveInstrumentCalibrationResult_::rawResiduals_);
        Property(result, "scaled_residuals", &CurveInstrumentCalibrationResult_::scaledResiduals_);
        Property(result, "evaluations", &CurveInstrumentCalibrationResult_::evaluations_);
        Property(result, "jacobian_evaluations", &CurveInstrumentCalibrationResult_::gradients_);
        Property(result, "fit_within_tolerance", &CurveInstrumentCalibrationResult_::fitWithinTolerance_);
        Property(result, "termination_reason", &CurveInstrumentCalibrationResult_::termination_);
        auto mc = py::class_<CalibrationMonteCarloResult_>(m, "CalibrationMonteCarloResult_");
        Property(mc, "mean", &CalibrationMonteCarloResult_::mean_);
        Property(mc, "standard_error", &CalibrationMonteCarloResult_::standardError_);
        Property(mc, "report", &CalibrationMonteCarloResult_::report_);
        py::class_<PreparedCalibrationInstrument_>(m, "PreparedCalibrationInstrument_")
            .def_property_readonly("instrument", [](const PreparedCalibrationInstrument_& v) { return Mutable(v.Instrument()); })
            .def_property_readonly("plan_identity", [](const PreparedCalibrationInstrument_& v) { return Text(v.PlanIdentity()); })
            .def_property_readonly("context_identity", [](const PreparedCalibrationInstrument_& v) { return Text(v.ContextIdentity()); })
            .def_property_readonly("compiler_identity", [](const PreparedCalibrationInstrument_& v) { return Text(v.CompilerIdentity()); })
            .def_property_readonly("backend", [](const PreparedCalibrationInstrument_& v) { return std::string(v.Capability().backend_.String()); })
            .def_property_readonly("derivative_method",
                                   [](const PreparedCalibrationInstrument_& v) { return std::string(v.Capability().derivativeMethod_.String()); });
    }
    void Factories(py::module_& m) {
        using Import_ = Handle_<CalibrationInstrument_> (*)(const Handle_<YCInstrument_>&, const ScriptInstrumentImportSettings_&);
        for (const auto& factory :
             std::initializer_list<std::pair<const char*, Import_>>{{"SwapScriptInstrument_New", NewSwapScriptInstrument},
                                                                    {"PeriodForwardOisScriptInstrument_New", NewPeriodForwardOisScriptInstrument},
                                                                    {"BasisSwapScriptInstrument_New", NewBasisSwapScriptInstrument}})
            m.def(
                factory.first,
                [function = factory.second](const py::object& native, const py::object& settings) {
                    const auto input = Read<Handle_<YCInstrument_>>(native, "/native");
                    const auto configuration = Read<ScriptInstrumentImportSettings_>(settings, "/settings");
                    py::gil_scoped_release release;
                    return Mutable(function(input, configuration));
                },
                py::arg("native"), py::arg("settings"));
        m.def(
            "FraScriptInstrument_New",
            [](const py::object& native, const py::object& payment, const py::object& settings) {
                const auto input = Read<Handle_<YCInstrument_>>(native, "/native");
                const auto date = Read<Date_>(payment, "/payment");
                const auto configuration = Read<ScriptInstrumentImportSettings_>(settings, "/settings");
                py::gil_scoped_release release;
                return Mutable(NewFraScriptInstrument(input, date, configuration));
            },
            py::arg("native"), py::arg("payment"), py::arg("settings"));
        m.def(
            "ScriptInstrumentDefinition_New",
            [](const py::object& spec) {
                const auto value = Read<ScriptInstrumentDefinitionSpec_>(spec, "/spec");
                py::gil_scoped_release release;
                return Mutable(NewScriptInstrumentDefinition(value));
            },
            py::arg("spec"));
        m.def(
            "ScriptCalibrationInstrument_New",
            [](const py::object& spec) {
                const auto value = Read<ScriptCalibrationInstrumentSpec_>(spec, "/spec");
                py::gil_scoped_release release;
                return Mutable(NewScriptCalibrationInstrument(value));
            },
            py::arg("spec"));
        m.def(
            "CalibrationInstrument_BindParameters",
            [](const py::object& instrument, const py::object& bindings) {
                const auto value = Read<Handle_<CalibrationInstrument_>>(instrument, "/instrument");
                const auto updates = Read<CalibrationParameterBindings_>(bindings, "/bindings");
                py::gil_scoped_release release;
                return Mutable(BindCalibrationInstrumentParameters(value, updates));
            },
            py::arg("instrument"), py::arg("bindings"));
        m.def(
            "DepositScriptInstrument_New",
            [](const py::object& deposit, const py::object& settings) {
                const auto native = Read<Handle_<YCInstrument_>>(deposit, "/deposit");
                const auto options = Read<ScriptInstrumentImportSettings_>(settings, "/settings");
                py::gil_scoped_release release;
                return Mutable(NewDepositScriptInstrument(native, options));
            },
            py::arg("deposit"), py::arg("settings"));
        m.def(
            "EuropeanRateOptionScriptInstrument_New",
            [](const EuropeanRateOption_& option, const py::object& settings) {
                const auto owned = option;
                const auto options = Read<ScriptInstrumentImportSettings_>(settings, "/settings");
                py::gil_scoped_release release;
                return Mutable(NewEuropeanRateOptionScriptInstrument(owned, options));
            },
            py::arg("option"), py::arg("settings"));
        m.def(
            "CalibrationMarketQuote_New",
            [](const py::object& id, const py::object& number, const py::object& unit, const py::object& convention) {
                const auto key = Read<String_>(id, "/id");
                const auto value = Number(number, "/value", "InvalidCalibrationQuote", false);
                const auto amount = Read<CalibrationValueUnit_>(unit, "/unit");
                const auto quotation = convention.is_none()
                                           ? std::nullopt
                                           : std::optional<CalibrationQuoteConvention_>(Read<CalibrationQuoteConvention_>(convention, "/convention"));
                py::gil_scoped_release release;
                return Mutable(NewCalibrationMarketQuote(key, value, amount, quotation));
            },
            py::arg("id"), py::arg("value"), py::arg("unit"), py::kw_only(), py::arg("convention") = py::none());
        m.def(
            "CalibrationObservation_New",
            [](const py::object& id, const py::object& instrument, const py::object& measure, const py::object& quote, const py::object& scale) {
                const auto key = Read<String_>(id, "/id"), selected = Read<String_>(measure, "/measure_id");
                const auto contract = Read<Handle_<CalibrationInstrument_>>(instrument, "/instrument");
                const auto target = Read<Handle_<CalibrationMarketQuote_>>(quote, "/quote");
                const auto tolerance = Number(scale, "/error_scale", "InvalidCalibrationObservation", false);
                py::gil_scoped_release release;
                return Mutable(NewCalibrationObservation(key, contract, selected, target, tolerance));
            },
            py::arg("id"), py::arg("instrument"), py::arg("measure_id"), py::arg("quote"), py::arg("error_scale"));
        m.def(
            "Calibrate_CurveInstruments",
            [](const py::object& spec, const py::object& options) {
                const auto input = Read<CurveInstrumentCalibrationSpec_>(spec, "/spec");
                const auto settings = DefaultInput<CurveInstrumentCalibrationOptions_>(options, "/options");
                py::gil_scoped_release release;
                return CalibrateCurveInstruments(input, settings);
            },
            py::arg("spec"), py::kw_only(), py::arg("options") = py::none());
        m.def(
            "CalibrationInstrument_ValueByMonteCarlo",
            [](const py::object& instrument, const py::object& model, const py::object& request) {
                const auto contract = Read<Handle_<CalibrationInstrument_>>(instrument, "/instrument");
                const auto market = Read<Handle_<ModelData_>>(model, "/model");
                const auto settings = Read<CalibrationMonteCarloRequest_>(request, "/request");
                py::gil_scoped_release release;
                testBarrier.Wait(&testBarrierArmed, GIL_TIMEOUT);
                return ValueCalibrationInstrumentByMonteCarlo(contract, market, settings);
            },
            py::arg("instrument"), py::arg("model"), py::arg("request"));
        m.def(
            "CalibrationPlan_New",
            [](const py::object& instrument, const py::object& measure, const py::object& context, const py::object& options) {
                const auto contract = Read<Handle_<CalibrationInstrument_>>(instrument, "/instrument");
                const auto selected = Read<String_>(measure, "/measure_id");
                const auto market = Read<CalibrationPreparationContext_>(context, "/context");
                const auto settings = DefaultInput<CalibrationEvaluationOptions_>(options, "/options");
                py::gil_scoped_release release;
                return PrepareCalibrationInstrument(contract, selected, market, settings);
            },
            py::arg("instrument"), py::arg("measure_id"), py::arg("context"), py::kw_only(), py::arg("options") = py::none());
    }
    void Getters(py::module_& m) {
        m.def(
            "CalibrationPlan_Get_PlanIdentity", [](const PreparedCalibrationInstrument_& plan) { return Text(plan.PlanIdentity()); },
            py::arg("plan"));
        m.def(
            "ScriptInstrumentDefinition_Get_Json",
            [](const py::object& input) {
                const auto value = Read<Handle_<ScriptInstrumentDefinition_>>(input, "/handle");
                py::gil_scoped_release release;
                return Text(DumpScriptInstrumentDefinitionJson(value));
            },
            py::arg("handle"));
        m.def(
            "ScriptInstrumentDefinition_LoadJson",
            [](const py::object& input) {
                const auto value = Read<String_>(input, "/json");
                py::gil_scoped_release release;
                return Mutable(LoadScriptInstrumentDefinitionJson(value));
            },
            py::arg("json"));
        m.def(
            "CalibrationInstrument_Get_Json",
            [](const py::object& input) {
                const auto value = Read<Handle_<CalibrationInstrument_>>(input, "/handle");
                py::gil_scoped_release release;
                return Text(DumpCalibrationInstrumentJson(value));
            },
            py::arg("handle"));
        m.def(
            "CalibrationInstrument_LoadJson",
            [](const py::object& input) {
                const auto value = Read<String_>(input, "/json");
                py::gil_scoped_release release;
                return Mutable(LoadCalibrationInstrumentJson(value));
            },
            py::arg("json"));
        m.def(
            "CalibrationReport_Get_Json",
            [](const py::object& input) {
                const auto value = Read<Handle_<CalibrationReport_>>(input, "/report");
                py::gil_scoped_release release;
                return Text(DumpCalibrationReportJson(value));
            },
            py::arg("report"));
        m.def(
            "CalibrationReport_Get_Summary",
            [](const py::object& input) { return Table(Read<Handle_<CalibrationReport_>>(input, "/report"), "summary"); }, py::arg("report"));
        m.def(
            "CalibrationReport_Get_Observations",
            [](const py::object& input) { return Table(Read<Handle_<CalibrationReport_>>(input, "/report"), "observations"); }, py::arg("report"));
        m.def(
            "CalibrationReport_Get_Quotes",
            [](const py::object& input) { return Table(Read<Handle_<CalibrationReport_>>(input, "/report"), "quotes"); }, py::arg("report"));
        m.def(
            "CalibrationReport_Get_Parameters",
            [](const py::object& input) { return Table(Read<Handle_<CalibrationReport_>>(input, "/report"), "parameters"); }, py::arg("report"));
        m.def(
            "CalibrationReport_Get_Plans",
            [](const py::object& input) { return Table(Read<Handle_<CalibrationReport_>>(input, "/report"), "plans"); }, py::arg("report"));
        m.def(
            "CalibrationReport_Get_Errors",
            [](const py::object& input) { return Table(Read<Handle_<CalibrationReport_>>(input, "/report"), "errors"); }, py::arg("report"));
        m.def(
            "CalibrationReport_Get_QuoteJacobian",
            [](const py::object& input) {
                const auto value = Read<Handle_<CalibrationReport_>>(input, "/report");
                py::gil_scoped_release release;
                return CalibrationReportQuoteJacobian(value);
            },
            py::arg("report"));
        m.def(
            "CalibrationReport_Get_QuoteMap",
            [](const py::object& input) {
                const auto value = Read<Handle_<CalibrationReport_>>(input, "/report");
                return value->Axes().quoteMap_;
            },
            py::arg("report"));
    }
} // namespace

void init_bindings_calibrationinstrument(py::module_& m) {
    GilTestHooks(m);
    init_calibration_types(m);
    Handles(m);
    ErrorType(m);
    Factories(m);
    Getters(m);
    init_xccy_instruments(m);
    init_xccy_monte_carlo(m);
    for (const char* type :
         {"ScriptInstrumentImportSettings", "CurveInstrumentGeometry", "CurveInstrumentMarket", "CurveInstrumentSolver",
          "CurveInstrumentCalibrationSpec", "CurveInstrumentCalibrationOptions", "CalibrationSamplingSpec", "CalibrationEvaluationOptions",
          "CalibrationPreparationContext", "CalibrationMonteCarloSampling", "CalibrationSimulationSettings", "CalibrationMonteCarloRequest"})
        m.attr((std::string(type) + "_New").c_str()) = m.attr((std::string(type) + "_").c_str());
}

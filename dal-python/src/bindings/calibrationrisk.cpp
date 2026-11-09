#include "bindings.h"

#include <cmath>
#include <optional>
#include <type_traits>
#include <variant>

#include <pybind11/stl.h>

#include <dal-public/src/calibrationrisk.hpp>
#include <dal-public/src/scriptcurvequoterisk.hpp>

#include "calibrationinput.hpp"
#include "giltestbarrier.hpp"
#include "scriptsettings.hpp"

using namespace Dal;
using namespace Dal::Python;

namespace {
    thread_local bool scriptRiskBarrierArmed = false;
    GilTestBarrier_ scriptRiskBarrier;
    constexpr const char* SCRIPT_RISK_GIL_TIMEOUT = "ScriptQuoteRisk: Python GIL test barrier timed out";
    void ScriptRiskGilHooks(py::module_& m) {
        m.def("_ScriptQuoteRiskGilBarrier_ArmForTesting", [] { scriptRiskBarrier.Arm(&scriptRiskBarrierArmed); });
        m.def("_ScriptQuoteRiskGilBarrier_AwaitEntryForTesting", [] {
            py::gil_scoped_release release;
            return scriptRiskBarrier.AwaitEntry();
        });
        m.def("_ScriptQuoteRiskGilBarrier_ReleaseForTesting", [] { return scriptRiskBarrier.Release(); });
        m.def("_ScriptQuoteRiskGilBarrier_BlockWithGilForTesting", [] { scriptRiskBarrier.Wait(&scriptRiskBarrierArmed, SCRIPT_RISK_GIL_TIMEOUT); });
    }
    template <class T_> T_ RequiredInput(const py::handle& value, const std::string& field, const char* type) {
        if (!py::isinstance<T_>(value))
            throw py::type_error(InputContext(value, field, type, "InvalidCalibrationPullback"));
        return py::cast<T_>(value);
    }

    CalibrationPullback_ BoundaryInput(const py::object& calibration) {
        if (py::isinstance<DupireCalibrationSnapshot_>(calibration))
            return NewCalibrationPullback(py::cast<const DupireCalibrationSnapshot_&>(calibration));
        if (py::isinstance<RateQuoteRiskProvenance_>(calibration))
            return NewCalibrationPullback(py::cast<const RateQuoteRiskProvenance_&>(calibration));
        throw py::type_error(InputContext(calibration, "CalibrationPullback_New; calibration",
                                          "DupireCalibrationSnapshot_ or RateQuoteRiskProvenance_", "InvalidCalibrationPullback"));
    }

    bool IsSequence(const py::handle& value) { return py::isinstance<py::list>(value) || py::isinstance<py::tuple>(value); }

    double NumericSeed(const py::handle& value, const std::string& field, int row, int column) {
        const auto error = [&] {
            return InputContext(value, field + "; row=" + std::to_string(row) + "; column=" + std::to_string(column),
                                "finite int or float, excluding bool and enums", "InvalidCalibrationPullback");
        };
        if (PyBool_Check(value.ptr()) || IsEnum(value) || (!PyLong_Check(value.ptr()) && !PyFloat_Check(value.ptr())))
            throw py::type_error(error());
        const double result = PyFloat_AsDouble(value.ptr());
        if (PyErr_Occurred()) {
            PyErr_Clear();
            throw py::value_error(error());
        }
        REQUIRE(std::isfinite(result), String_(error()));
        return result;
    }

    Matrix_<> SeedMatrixInput(const py::object& value, const std::string& field, int rows, int columns) {
        if (py::isinstance<Matrix_<>>(value))
            return py::cast<Matrix_<>>(value);
        if (!IsSequence(value))
            throw py::type_error(InputContext(value, field, "DoubleMatrix_ or rectangular list/tuple of numeric rows", "InvalidCalibrationPullback"));
        const auto outer = py::reinterpret_borrow<py::sequence>(value);
        REQUIRE(py::len(outer) == static_cast<size_t>(rows), "InvalidCalibrationPullback: seed dimensions disagree; field=" + String_(field));
        Matrix_<> result(rows, columns);
        for (int row = 0; row < rows; ++row) {
            const auto cells = outer[row];
            if (!IsSequence(cells))
                throw py::type_error(
                    InputContext(cells, field + "; row=" + std::to_string(row), "list/tuple of numeric cells", "InvalidCalibrationPullback"));
            const auto inner = py::reinterpret_borrow<py::sequence>(cells);
            REQUIRE(py::len(inner) == static_cast<size_t>(columns),
                    "InvalidCalibrationPullback: seed dimensions disagree; field=" + String_(field) + "; row=" + String::FromInt(row));
            for (int column = 0; column < columns; ++column)
                result(row, column) = NumericSeed(inner[column], field, row, column);
        }
        return result;
    }

    template <class T_> void BindSeeds(py::module_& m, const char* type, const char* factory) {
        WithCopies(py::class_<T_>(m, type))
            .def_property_readonly("calibration", [](const T_& value) { return value.Calibration(); })
            .def_property_readonly("adjoints", [](const T_& value) { return Matrix_<>(value.Adjoints()); });
        m.def(
            factory,
            [factory](const py::object& calibration, const py::object& adjoints) {
                const auto source = RequiredInput<CalibrationPullback_>(calibration, std::string(factory) + "; calibration", "CalibrationPullback_");
                constexpr bool parameters = std::is_same_v<T_, CalibrationParameterAdjoints_>;
                const int rows = parameters ? source.ParameterRows() : source.QuoteRows();
                const int columns = parameters ? source.ParameterCols() : source.QuoteCols();
                return T_(source, SeedMatrixInput(adjoints, std::string(factory) + "; adjoints", rows, columns));
            },
            py::arg("calibration"), py::arg("adjoints"));
    }

    std::optional<CalibrationDirectQuoteAdjoints_> DirectInput(const py::object& direct) {
        return direct.is_none() ? std::nullopt
                                : std::optional<CalibrationDirectQuoteAdjoints_>(RequiredInput<CalibrationDirectQuoteAdjoints_>(
                                      direct, "PullbackCalibration; direct", "CalibrationDirectQuoteAdjoints_ or None"));
    }

    void ScriptRiskSettings(py::module_& m) {
        using namespace Dal::Python::Calibration;
        auto accuracy = Struct<ScriptCurveQuoteRiskAccuracy_>(m, "ScriptCurveQuoteRiskAccuracy_", [](Fields_& fields) {
            ScriptCurveQuoteRiskAccuracy_ value;
            value.residualAbsoluteLimits_ = fields.Required<Vector_<>>("residual_absolute_limits");
            value.minReciprocalCondition_ = fields.Required<double>("min_reciprocal_condition");
            value.responseBackwardErrorLimit_ = fields.Required<double>("response_backward_error_limit");
            value.relativePivotTolerance_ = fields.Default<double>("relative_pivot_tolerance", value.relativePivotTolerance_);
            value.pvOnly_ = fields.Default<bool>("pv_only", value.pvOnly_);
            return value;
        });
        Property(accuracy, "residual_absolute_limits", &ScriptCurveQuoteRiskAccuracy_::residualAbsoluteLimits_);
        Property(accuracy, "min_reciprocal_condition", &ScriptCurveQuoteRiskAccuracy_::minReciprocalCondition_);
        Property(accuracy, "response_backward_error_limit", &ScriptCurveQuoteRiskAccuracy_::responseBackwardErrorLimit_);
        Property(accuracy, "relative_pivot_tolerance", &ScriptCurveQuoteRiskAccuracy_::relativePivotTolerance_);
        Property(accuracy, "pv_only", &ScriptCurveQuoteRiskAccuracy_::pvOnly_);
        auto request = Struct<ScriptCurveQuoteRiskRequest_>(m, "ScriptCurveQuoteRiskRequest_", [](Fields_& fields) {
            return ScriptCurveQuoteRiskRequest_{fields.Default<Vector_<String_>>("quote_ids", {}), fields.Default<String_>("measure_id", "Price"),
                                                fields.Default<bool>("live_quote_payoff", false)};
        });
        Property(request, "quote_ids", &ScriptCurveQuoteRiskRequest_::quoteIds_);
        Property(request, "measure_id", &ScriptCurveQuoteRiskRequest_::measureId_);
        Property(request, "live_quote_payoff", &ScriptCurveQuoteRiskRequest_::liveQuotePayoff_);
        m.attr("ScriptCurveQuoteRiskAccuracy_New") = m.attr("ScriptCurveQuoteRiskAccuracy_");
        m.attr("ScriptCurveQuoteRiskRequest_New") = m.attr("ScriptCurveQuoteRiskRequest_");
    }
    void ScriptRiskAxes(py::module_& m) {
        using namespace Dal::Python::Calibration;
        auto quote = py::class_<ScriptCurveQuoteRiskQuote_>(m, "ScriptCurveQuoteRiskQuote_");
        Property(quote, "ordinal", &ScriptCurveQuoteRiskQuote_::ordinal_);
        Property(quote, "id", &ScriptCurveQuoteRiskQuote_::id_);
        Property(quote, "value", &ScriptCurveQuoteRiskQuote_::value_);
        Property(quote, "unit", &ScriptCurveQuoteRiskQuote_::unit_);
        Property(quote, "convention", &ScriptCurveQuoteRiskQuote_::convention_);
        auto observation = py::class_<ScriptCurveQuoteRiskObservation_>(m, "ScriptCurveQuoteRiskObservation_");
        Property(observation, "ordinal", &ScriptCurveQuoteRiskObservation_::ordinal_);
        Property(observation, "quote_ordinal", &ScriptCurveQuoteRiskObservation_::quoteOrdinal_);
        Property(observation, "id", &ScriptCurveQuoteRiskObservation_::id_);
        Property(observation, "instrument_id", &ScriptCurveQuoteRiskObservation_::instrumentId_);
        Property(observation, "measure_id", &ScriptCurveQuoteRiskObservation_::measureId_);
        Property(observation, "source_identity", &ScriptCurveQuoteRiskObservation_::sourceIdentity_);
        Property(observation, "definition_identity", &ScriptCurveQuoteRiskObservation_::definitionIdentity_);
        Property(observation, "binding_identity", &ScriptCurveQuoteRiskObservation_::bindingIdentity_);
        Property(observation, "error_scale", &ScriptCurveQuoteRiskObservation_::errorScale_);
        auto parameter = py::class_<ScriptCurveQuoteRiskParameter_>(m, "ScriptCurveQuoteRiskParameter_");
        Property(parameter, "ordinal", &ScriptCurveQuoteRiskParameter_::ordinal_);
        Property(parameter, "id", &ScriptCurveQuoteRiskParameter_::id_);
        Property(parameter, "unit", &ScriptCurveQuoteRiskParameter_::unit_);
        Property(parameter, "coordinate", &ScriptCurveQuoteRiskParameter_::coordinate_);
        Property(parameter, "value", &ScriptCurveQuoteRiskParameter_::value_);
        auto numerics = py::class_<ScriptCurveQuoteRiskNumerics_>(m, "ScriptCurveQuoteRiskNumerics_");
        Property(numerics, "model_measures", &ScriptCurveQuoteRiskNumerics_::modelMeasures_);
        Property(numerics, "raw_residuals", &ScriptCurveQuoteRiskNumerics_::rawResiduals_);
        Property(numerics, "native_jacobian", &ScriptCurveQuoteRiskNumerics_::nativeJacobian_);
        Property(numerics, "reciprocal_condition", &ScriptCurveQuoteRiskNumerics_::reciprocalCondition_);
        Property(numerics, "transpose_backward_errors", &ScriptCurveQuoteRiskNumerics_::transposeBackwardErrors_);
        Property(numerics, "response_backward_errors", &ScriptCurveQuoteRiskNumerics_::responseBackwardErrors_);
        Property(numerics, "factorization_count", &ScriptCurveQuoteRiskNumerics_::factorizationCount_);
    }
    void ScriptRiskSnapshot(py::module_& m) {
        using namespace Dal::Python::Calibration;
        WithCopies(py::class_<ScriptCurveQuoteRiskSnapshot_>(m, "ScriptCurveQuoteRiskSnapshot_"))
            .def_property_readonly("structure_valid", &ScriptCurveQuoteRiskSnapshot_::StructureValid)
            .def_property_readonly("mapping_available", &ScriptCurveQuoteRiskSnapshot_::MappingAvailable)
            .def_property_readonly("risk_available", &ScriptCurveQuoteRiskSnapshot_::RiskAvailable)
            .def_property_readonly("status", [](const ScriptCurveQuoteRiskSnapshot_& v) { return v.RiskAvailable() ? "Available" : "Unavailable"; })
            .def_property_readonly("identity", [](const ScriptCurveQuoteRiskSnapshot_& v) { return Text(v.Identity()); })
            .def_property_readonly("context_identity", [](const ScriptCurveQuoteRiskSnapshot_& v) { return Text(v.ContextIdentity()); })
            .def_property_readonly("accuracy", [](const ScriptCurveQuoteRiskSnapshot_& v) { return v.Accuracy(); })
            .def_property_readonly("quote_axis", [](const ScriptCurveQuoteRiskSnapshot_& v) { return Output(v.QuoteAxis()); })
            .def_property_readonly("observation_axis", [](const ScriptCurveQuoteRiskSnapshot_& v) { return Output(v.ObservationAxis()); })
            .def_property_readonly("parameter_axis", [](const ScriptCurveQuoteRiskSnapshot_& v) { return Output(v.ParameterAxis()); })
            .def_property_readonly("numerics", [](const ScriptCurveQuoteRiskSnapshot_& v) { return v.Numerics(); })
            .def_property_readonly("diagnostics", [](const ScriptCurveQuoteRiskSnapshot_& v) { return Output(v.Diagnostics()); });
        m.def(
            "ScriptCurveQuoteRiskSnapshot_New",
            [](const py::object& calibration, const py::object& accuracy) {
                const auto fit = Input_<CurveInstrumentCalibrationResult_>::Read(calibration, "ScriptCurveQuoteRiskSnapshot_New; calibration");
                const auto limits = Input_<ScriptCurveQuoteRiskAccuracy_>::Read(accuracy, "ScriptCurveQuoteRiskSnapshot_New; accuracy");
                py::gil_scoped_release release;
                scriptRiskBarrier.Wait(&scriptRiskBarrierArmed, SCRIPT_RISK_GIL_TIMEOUT);
                return NewScriptCurveQuoteRiskSnapshot(fit, limits);
            },
            py::arg("calibration"), py::arg("accuracy"));
    }
    void ScriptRiskResult(py::module_& m) {
        using namespace Dal::Python::Calibration;
        WithCopies(py::class_<ScriptCurveQuoteRiskResult_>(m, "ScriptCurveQuoteRiskResult_"))
            .def_property_readonly("available", &ScriptCurveQuoteRiskResult_::Available)
            .def_property_readonly("status", [](const ScriptCurveQuoteRiskResult_& v) { return v.Available() ? "Available" : "Unavailable"; })
            .def_property_readonly("currency", [](const ScriptCurveQuoteRiskResult_& v) { return v.Currency().String(); })
            .def_property_readonly("pv", &ScriptCurveQuoteRiskResult_::PV)
            .def_property_readonly("raw_sensitivity", [](const ScriptCurveQuoteRiskResult_& v) { return Output(v.RawSensitivity()); })
            .def_property_readonly("dv01", [](const ScriptCurveQuoteRiskResult_& v) { return Output(v.DV01()); })
            .def_property_readonly("native_gradient", [](const ScriptCurveQuoteRiskResult_& v) { return Output(v.NativeGradient()); })
            .def_property_readonly("quote_axis", [](const ScriptCurveQuoteRiskResult_& v) { return Output(v.Axes().quotes_); })
            .def_property_readonly("observation_axis", [](const ScriptCurveQuoteRiskResult_& v) { return Output(v.Axes().observations_); })
            .def_property_readonly("parameter_axis", [](const ScriptCurveQuoteRiskResult_& v) { return Output(v.Axes().parameters_); })
            .def_property_readonly("diagnostics", [](const ScriptCurveQuoteRiskResult_& v) { return Output(v.Diagnostics()); })
            .def_property_readonly("numerics", [](const ScriptCurveQuoteRiskResult_& v) { return v.Numerics(); })
            .def_property_readonly("method", [](const ScriptCurveQuoteRiskResult_& v) { return Text(v.Method()); })
            .def_property_readonly("snapshot_identity", [](const ScriptCurveQuoteRiskResult_& v) { return Text(v.SnapshotIdentity()); })
            .def_property_readonly("context_identity", [](const ScriptCurveQuoteRiskResult_& v) { return Text(v.ContextIdentity()); })
            .def_property_readonly("source_identity", [](const ScriptCurveQuoteRiskResult_& v) { return Text(v.SourceIdentity()); })
            .def_property_readonly("definition_identity", [](const ScriptCurveQuoteRiskResult_& v) { return Text(v.DefinitionIdentity()); })
            .def_property_readonly("binding_identity", [](const ScriptCurveQuoteRiskResult_& v) { return Text(v.BindingIdentity()); });
        m.def(
            "CalibrationInstrument_ValueWithQuoteRisk",
            [](const py::object& instrument, const py::object& snapshot, const py::object& context, const py::object& request) {
                const auto contract =
                    Input_<Handle_<CalibrationInstrument_>>::Read(instrument, "CalibrationInstrument_ValueWithQuoteRisk; instrument");
                const auto source = Input_<ScriptCurveQuoteRiskSnapshot_>::Read(snapshot, "CalibrationInstrument_ValueWithQuoteRisk; snapshot");
                const auto spec = Input_<CurveInstrumentCalibrationSpec_>::Read(context, "CalibrationInstrument_ValueWithQuoteRisk; context");
                const auto selection = request.is_none()
                                           ? ScriptCurveQuoteRiskRequest_()
                                           : Input_<ScriptCurveQuoteRiskRequest_>::Read(request, "CalibrationInstrument_ValueWithQuoteRisk; request");
                py::gil_scoped_release release;
                scriptRiskBarrier.Wait(&scriptRiskBarrierArmed, SCRIPT_RISK_GIL_TIMEOUT);
                return ValueCalibrationInstrumentWithQuoteRisk(contract, source, spec, selection);
            },
            py::arg("instrument"), py::arg("snapshot"), py::arg("context"), py::kw_only(), py::arg("request") = py::none());
    }
    template <class T_> void ScriptRiskGetters(py::module_& m, const char* prefix) {
        using namespace Dal::Python::Calibration;
        const std::string type(prefix);
        m.def((type + "_Get_Diagnostics").c_str(), [](const T_& v) { return Output(v.Diagnostics()); }, py::arg("value"));
        m.def((type + "_Get_Status").c_str(),
              [](const T_& v) {
                  if constexpr (std::is_same_v<T_, ScriptCurveQuoteRiskSnapshot_>)
                      return v.RiskAvailable() ? "Available" : "Unavailable";
                  else
                      return v.Available() ? "Available" : "Unavailable";
              },
              py::arg("value"));
        m.def((type + "_Get_QuoteAxis").c_str(),
              [](const T_& v) {
                  if constexpr (std::is_same_v<T_, ScriptCurveQuoteRiskSnapshot_>)
                      return Output(v.QuoteAxis());
                  else
                      return Output(v.Axes().quotes_);
              },
              py::arg("value"));
    }
    void ScriptRiskValueGetters(py::module_& m) {
        using namespace Dal::Python::Calibration;
        m.def("ScriptCurveQuoteRiskResult_Get_PV", &ScriptCurveQuoteRiskResult_::PV, py::arg("result"));
        m.def(
            "ScriptCurveQuoteRiskResult_Get_RawSensitivity", [](const ScriptCurveQuoteRiskResult_& v) { return Output(v.RawSensitivity()); },
            py::arg("result"));
        m.def("ScriptCurveQuoteRiskResult_Get_DV01", [](const ScriptCurveQuoteRiskResult_& v) { return Output(v.DV01()); }, py::arg("result"));
    }
} // namespace

void init_bindings_calibrationrisk(py::module_& m) {
    ScriptRiskSettings(m);
    ScriptRiskAxes(m);
    ScriptRiskSnapshot(m);
    ScriptRiskResult(m);
    ScriptRiskGilHooks(m);
    ScriptRiskGetters<ScriptCurveQuoteRiskSnapshot_>(m, "ScriptCurveQuoteRiskSnapshot");
    ScriptRiskGetters<ScriptCurveQuoteRiskResult_>(m, "ScriptCurveQuoteRiskResult");
    ScriptRiskValueGetters(m);
    WithCopies(py::class_<CalibrationPullback_>(m, "CalibrationPullback_"))
        .def_property_readonly("source",
                               [](const CalibrationPullback_& value) {
                                   return std::visit([](const auto& source) { return py::cast(source, py::return_value_policy::copy); },
                                                     value.Source());
                               })
        .def_property_readonly("domain", [](const CalibrationPullback_& value) { return Text(value.Domain()); })
        .def_property_readonly("parameter_rows", &CalibrationPullback_::ParameterRows)
        .def_property_readonly("parameter_cols", &CalibrationPullback_::ParameterCols)
        .def_property_readonly("quote_rows", &CalibrationPullback_::QuoteRows)
        .def_property_readonly("quote_cols", &CalibrationPullback_::QuoteCols)
        .def_property_readonly("method", [](const CalibrationPullback_& value) { return Text(value.Method()); })
        .def_property_readonly("unit", [](const CalibrationPullback_& value) { return Text(value.Unit()); })
        .def_property_readonly("boundary", [](const CalibrationPullback_& value) { return Text(value.Boundary()); })
        .def("matches", &CalibrationPullback_::Matches, py::arg("other"));
    m.def("CalibrationPullback_New", &BoundaryInput, py::arg("calibration"));
    BindSeeds<CalibrationParameterAdjoints_>(m, "CalibrationParameterAdjoints_", "CalibrationParameterAdjoints_New");
    BindSeeds<CalibrationDirectQuoteAdjoints_>(m, "CalibrationDirectQuoteAdjoints_", "CalibrationDirectQuoteAdjoints_New");
    WithCopies(py::class_<CalibrationQuoteRisk_>(m, "CalibrationQuoteRisk_"))
        .def_property_readonly("calibration", [](const CalibrationQuoteRisk_& value) { return value.Calibration(); })
        .def_property_readonly("calibration_adjoints", [](const CalibrationQuoteRisk_& value) { return Matrix_<>(value.CalibrationAdjoints()); })
        .def_property_readonly("direct_adjoints", [](const CalibrationQuoteRisk_& value) { return Matrix_<>(value.DirectAdjoints()); })
        .def_property_readonly("total_adjoints", [](const CalibrationQuoteRisk_& value) { return Matrix_<>(value.TotalAdjoints()); })
        .def_property_readonly("method", [](const CalibrationQuoteRisk_& value) { return Text(value.Method()); })
        .def_property_readonly("unit", [](const CalibrationQuoteRisk_& value) { return Text(value.Unit()); })
        .def_property_readonly("boundary", [](const CalibrationQuoteRisk_& value) { return Text(value.Boundary()); });
    m.def(
        "PullbackCalibration",
        [](const py::object& calibration, const py::object& parameters, const py::object& direct) {
            const auto source = RequiredInput<CalibrationPullback_>(calibration, "PullbackCalibration; calibration", "CalibrationPullback_");
            const auto seeds =
                RequiredInput<CalibrationParameterAdjoints_>(parameters, "PullbackCalibration; parameter_adjoints", "CalibrationParameterAdjoints_");
            const auto contribution = DirectInput(direct);
            py::gil_scoped_release release;
            return Dal::PullbackCalibration(source, seeds, contribution);
        },
        py::arg("calibration"), py::arg("parameter_adjoints"), py::kw_only(), py::arg("direct") = py::none());
}

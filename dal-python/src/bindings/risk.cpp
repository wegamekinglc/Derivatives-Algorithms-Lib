//
// Created by Codex on 2026/10/5.
//

#include "bindings.h"

#include <limits>
#include <pybind11/stl.h>

#include <dal-public/src/value.hpp>

#include "scriptsettings.hpp"

using namespace Dal;
using namespace Dal::Python;
using namespace Dal::Script;

namespace {
    template <class T_> std::vector<T_> CopyVector(const Vector_<T_>& values) { return {values.begin(), values.end()}; }

    std::vector<std::string> TextVector(const Vector_<String_>& values) {
        std::vector<std::string> result;
        result.reserve(values.size());
        for (const auto& value : values)
            result.push_back(Text(value));
        return result;
    }

    std::optional<std::vector<std::string>> TextVector(const std::optional<Vector_<String_>>& values) {
        return values ? std::optional<std::vector<std::string>>(TextVector(*values)) : std::nullopt;
    }

    py::sequence RequestSequence(const py::handle& value, const char* field) {
        if (!py::isinstance<py::list>(value) && !py::isinstance<py::tuple>(value))
            throw py::type_error(InputContext(value, std::string("RiskRequest_; ") + field, "list or tuple, or None", "InvalidRiskRequest"));
        return py::reinterpret_borrow<py::sequence>(value);
    }

    std::optional<Vector_<String_>> Ids(const py::handle& value, const char* field) {
        if (value.is_none())
            return std::nullopt;
        Vector_<String_> ids;
        for (const auto& id : RequestSequence(value, field))
            ids.push_back(SettingStringInput(id, std::string("RiskRequest_; ") + field, "InvalidRiskRequest"));
        return ids;
    }

    std::optional<Vector_<>> Factors(const py::handle& value) {
        if (value.is_none())
            return std::nullopt;
        Vector_<> factors;
        for (const auto& item : RequestSequence(value, "report_factors")) {
            if (PyBool_Check(item.ptr()) || IsEnum(item) || (!PyLong_Check(item.ptr()) && !PyFloat_Check(item.ptr())))
                throw py::type_error(InputContext(item, "RiskRequest_; report_factors", "int or float, excluding bool", "InvalidRiskRequest"));
            const double factor = PyFloat_AsDouble(item.ptr());
            if (PyErr_Occurred()) {
                PyErr_Clear();
                THROW2("InvalidRiskRequest: report_factors; expected representable floating-point values", ScriptError_);
            }
            factors.push_back(factor);
        }
        return factors;
    }

    std::optional<size_t> Budget(const py::handle& value) {
        if (value.is_none())
            return std::nullopt;
        const auto context = InputContext(value, "RiskRequest_; numeric_payload_budget_bytes", "nonnegative size_t integer or None, excluding bool",
                                          "InvalidRiskRequest");
        if (PyBool_Check(value.ptr()) || IsEnum(value) || !PyIndex_Check(value.ptr()))
            throw py::type_error(context);
        const auto integer = py::reinterpret_steal<py::object>(PyNumber_Index(value.ptr()));
        if (!integer)
            throw py::error_already_set();
        const auto budget = PyLong_AsUnsignedLongLong(integer.ptr());
        if (PyErr_Occurred()) {
            PyErr_Clear();
            THROW2(String_(context), ScriptError_);
        }
        REQUIRE2(budget <= std::numeric_limits<size_t>::max(), String_(context), ScriptError_);
        return static_cast<size_t>(budget);
    }

    std::map<std::string, double> LegacyValues(const RiskResult_& result) {
        std::map<std::string, double> values;
        for (const auto& entry : result.LegacyValues())
            values.emplace(Text(entry.first), entry.second);
        return values;
    }
} // namespace

void init_bindings_risk(py::module_& m) {
    WithCopies(py::class_<RiskRequest_>(m, "RiskRequest_"))
        .def(py::init([](const py::object& inputs, const py::object& outputs, const py::object& reportFactors, const py::object& budget) {
                 return RiskRequest_{Ids(inputs, "inputs"), Ids(outputs, "outputs"), Factors(reportFactors), Budget(budget)};
             }),
             py::kw_only(), py::arg("inputs") = py::none(), py::arg("outputs") = py::none(), py::arg("report_factors") = py::none(),
             py::arg("numeric_payload_budget_bytes") = py::none())
        .def_property_readonly("inputs", [](const RiskRequest_& value) { return TextVector(value.inputs_); })
        .def_property_readonly("outputs", [](const RiskRequest_& value) { return TextVector(value.outputs_); })
        .def_property_readonly("report_factors",
                               [](const RiskRequest_& value) {
                                   return value.reportFactors_ ? std::optional<std::vector<double>>(CopyVector(*value.reportFactors_)) : std::nullopt;
                               })
        .def_property_readonly("numeric_payload_budget_bytes", [](const RiskRequest_& value) { return value.numericPayloadBudgetBytes_; });

    py::class_<RiskCoordinate_>(m, "RiskCoordinate_")
        .def_property_readonly("id", [](const RiskCoordinate_& value) { return Text(value.id_); })
        .def_property_readonly("label", [](const RiskCoordinate_& value) { return Text(value.label_); })
        .def_property_readonly("family", [](const RiskCoordinate_& value) { return Text(value.family_); })
        .def_property_readonly("ordinal", [](const RiskCoordinate_& value) { return value.ordinal_; })
        .def_property_readonly("value", [](const RiskCoordinate_& value) { return value.value_; })
        .def_property_readonly("native_unit", [](const RiskCoordinate_& value) { return Text(value.nativeUnit_); })
        .def_property_readonly(
            "physical_unit",
            [](const RiskCoordinate_& value) { return value.physicalUnit_ ? std::optional<std::string>(Text(*value.physicalUnit_)) : std::nullopt; })
        .def_property_readonly("report_scale", [](const RiskCoordinate_& value) { return value.reportScale_; });

    py::class_<RiskObservationSnapshot_>(m, "RiskObservationSnapshot_")
        .def_property_readonly("index", [](const RiskObservationSnapshot_& value) { return Text(value.index_); })
        .def_property_readonly("fixing_time", [](const RiskObservationSnapshot_& value) { return value.fixingTime_; })
        .def_property_readonly("historical", [](const RiskObservationSnapshot_& value) { return value.historical_; })
        .def_property_readonly("value", [](const RiskObservationSnapshot_& value) { return value.value_; });

    py::class_<RiskExecutionSnapshot_>(m, "RiskExecutionSnapshot_")
        .def_property_readonly("paths_per_replicate", [](const RiskExecutionSnapshot_& value) { return value.pathsPerReplicate_; })
        .def_property_readonly("pricing_replicates", [](const RiskExecutionSnapshot_& value) { return value.pricingReplicates_; })
        .def_property_readonly("all_expired", [](const RiskExecutionSnapshot_& value) { return value.allExpired_; })
        .def_property_readonly("simulation", [](const RiskExecutionSnapshot_& value) { return value.simulation_; })
        .def_property_readonly("product_settings", [](const RiskExecutionSnapshot_& value) { return value.productSettings_; })
        .def_property_readonly("product_dates", [](const RiskExecutionSnapshot_& value) { return CopyVector(value.productDates_); })
        .def_property_readonly("product_events", [](const RiskExecutionSnapshot_& value) { return TextVector(value.productEvents_); })
        .def_property_readonly("today_fixing_policy", [](const RiskExecutionSnapshot_& value) { return Text(value.todayFixingPolicy_); })
        .def_property_readonly("fixing_source", [](const RiskExecutionSnapshot_& value) { return Text(value.fixingSource_); })
        .def_property_readonly("model_snapshot_json", [](const RiskExecutionSnapshot_& value) { return Text(value.modelSnapshotJson_); })
        .def_property_readonly("observations", [](const RiskExecutionSnapshot_& value) { return CopyVector(value.observations_); });

    py::class_<RiskResultProvenance_>(m, "RiskResultProvenance_")
        .def_property_readonly("method", [](const RiskResultProvenance_& value) { return Text(value.method_); })
        .def_property_readonly("engine", [](const RiskResultProvenance_& value) { return Text(value.engine_); })
        .def_property_readonly("normalization", [](const RiskResultProvenance_& value) { return Text(value.normalization_); })
        .def_property_readonly("calibration", [](const RiskResultProvenance_& value) { return Text(value.calibration_); })
        .def_property_readonly("model_type", [](const RiskResultProvenance_& value) { return Text(value.modelType_); })
        .def_property_readonly("evaluation_date", [](const RiskResultProvenance_& value) { return value.evaluationDate_; })
        .def_property_readonly("execution", [](const RiskResultProvenance_& value) { return value.execution_; });

    WithCopies(py::class_<RiskResult_>(m, "RiskResult_"))
        .def_property_readonly("output_ids", [](const RiskResult_& value) { return TextVector(value.OutputIds()); })
        .def_property_readonly("values", [](const RiskResult_& value) { return CopyVector(value.Values()); })
        .def_property_readonly("jacobian", [](const RiskResult_& value) { return Matrix_<>(value.Jacobian()); })
        .def_property_readonly("reported_jacobian", &RiskResult_::ReportedJacobian)
        .def_property_readonly("input_axis", [](const RiskResult_& value) { return CopyVector(value.InputAxis()); })
        .def_property_readonly("complete_input_axis", [](const RiskResult_& value) { return CopyVector(value.CompleteInputAxis()); })
        .def_property_readonly("provenance", [](const RiskResult_& value) { return value.Provenance(); })
        .def_property_readonly("legacy_values", &LegacyValues);

    m.def(
        "MonteCarlo_ValueWithRisk",
        [](const std::shared_ptr<ScriptProductData_>& product, const std::shared_ptr<ModelData_>& model, const py::object& numPath,
           const py::object& request, const py::object& valuation, const py::object& simulation) {
            const int count = PathCount(numPath, "MonteCarlo_ValueWithRisk");
            const Handle_<ScriptProductData_> nativeProduct(product);
            const Handle_<ModelData_> nativeModel(model);
            const auto requested = SettingsInput<RiskRequest_>(request, "MonteCarlo_ValueWithRisk; request", "RiskRequest_");
            const auto settings =
                SettingsInput<ScriptValuationSettings_>(valuation, "MonteCarlo_ValueWithRisk; valuation", "ScriptValuationSettings_");
            const auto execution =
                simulation.is_none() ? DefaultRiskMonteCarloSettings()
                                     : SettingsInput<MonteCarloSettings_>(simulation, "MonteCarlo_ValueWithRisk; simulation", "MonteCarloSettings_");
            py::gil_scoped_release release;
            return ValueByMonteCarloWithRisk(nativeProduct, nativeModel, count, requested, settings, execution);
        },
        py::arg("product"), py::arg("modelData"), py::arg("num_path"), py::kw_only(), py::arg("request") = py::none(),
        py::arg("valuation") = py::none(), py::arg("simulation") = py::none());
}

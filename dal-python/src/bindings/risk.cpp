//
// Created by Codex on 2026/10/5.
//

#include "bindings.h"

#include <pybind11/stl.h>

#include <dal-public/src/value.hpp>

#include "riskrequest.hpp"

using namespace Dal;
using namespace Dal::Python;
using namespace Dal::Script;

namespace {
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
                 constexpr const char* IDENTIFIER = "InvalidRiskRequest";
                 return RiskRequest_{RequestIds(inputs, {"RiskRequest_; inputs", IDENTIFIER}),
                                     RequestIds(outputs, {"RiskRequest_; outputs", IDENTIFIER}),
                                     RequestFactors(reportFactors, {"RiskRequest_; report_factors", IDENTIFIER}),
                                     RequestBudget(budget, {"RiskRequest_; numeric_payload_budget_bytes", IDENTIFIER})};
             }),
             py::kw_only(), py::arg("inputs") = py::none(), py::arg("outputs") = py::none(), py::arg("report_factors") = py::none(),
             py::arg("numeric_payload_budget_bytes") = py::none())
        .def_property_readonly("inputs", [](const RiskRequest_& value) { return RiskTextVector(value.inputs_); })
        .def_property_readonly("outputs", [](const RiskRequest_& value) { return RiskTextVector(value.outputs_); })
        .def_property_readonly("report_factors",
                               [](const RiskRequest_& value) {
                                   return value.reportFactors_ ? std::optional<std::vector<double>>(CopyRiskVector(*value.reportFactors_))
                                                               : std::nullopt;
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
        .def_property_readonly("product_dates", [](const RiskExecutionSnapshot_& value) { return CopyRiskVector(value.productDates_); })
        .def_property_readonly("product_events", [](const RiskExecutionSnapshot_& value) { return RiskTextVector(value.productEvents_); })
        .def_property_readonly("today_fixing_policy", [](const RiskExecutionSnapshot_& value) { return Text(value.todayFixingPolicy_); })
        .def_property_readonly("fixing_source", [](const RiskExecutionSnapshot_& value) { return Text(value.fixingSource_); })
        .def_property_readonly("model_snapshot_json", [](const RiskExecutionSnapshot_& value) { return Text(value.modelSnapshotJson_); })
        .def_property_readonly("observations", [](const RiskExecutionSnapshot_& value) { return CopyRiskVector(value.observations_); });

    py::class_<RiskResultProvenance_>(m, "RiskResultProvenance_")
        .def_property_readonly("method", [](const RiskResultProvenance_& value) { return Text(value.method_); })
        .def_property_readonly("engine", [](const RiskResultProvenance_& value) { return Text(value.engine_); })
        .def_property_readonly("normalization", [](const RiskResultProvenance_& value) { return Text(value.normalization_); })
        .def_property_readonly("calibration", [](const RiskResultProvenance_& value) { return Text(value.calibration_); })
        .def_property_readonly("model_type", [](const RiskResultProvenance_& value) { return Text(value.modelType_); })
        .def_property_readonly("evaluation_date", [](const RiskResultProvenance_& value) { return value.evaluationDate_; })
        .def_property_readonly("execution", [](const RiskResultProvenance_& value) { return value.execution_; });

    WithCopies(py::class_<RiskResult_>(m, "RiskResult_"))
        .def_property_readonly("output_ids", [](const RiskResult_& value) { return RiskTextVector(value.OutputIds()); })
        .def_property_readonly("values", [](const RiskResult_& value) { return CopyRiskVector(value.Values()); })
        .def_property_readonly("jacobian", [](const RiskResult_& value) { return Matrix_<>(value.Jacobian()); })
        .def_property_readonly("reported_jacobian", &RiskResult_::ReportedJacobian)
        .def_property_readonly("input_axis", [](const RiskResult_& value) { return CopyRiskVector(value.InputAxis()); })
        .def_property_readonly("complete_input_axis", [](const RiskResult_& value) { return CopyRiskVector(value.CompleteInputAxis()); })
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

//
// Created by Codex on 2026/10/06.
//

#include "bindings.h"

#include <pybind11/stl.h>

#include <dal-public/src/value.hpp>

#include "riskrequest.hpp"

using namespace Dal;
using namespace Dal::Python;
using namespace Dal::Script;

void init_bindings_jacobianrisk(py::module_& m) {
    WithCopies(py::class_<JacobianRiskRequest_>(m, "JacobianRiskRequest_"))
        .def(py::init([](const py::object& inputs, const py::object& outputs, const py::object& reportFactors, const py::object& width,
                         const py::object& resultBudget, const py::object& recordingBudget, const py::object& scratchBudget) {
                 constexpr const char* IDENTIFIER = "InvalidJacobianRiskRequest";
                 const auto maximum = RequestUnsigned(width, {"JacobianRiskRequest_; max_block_width", IDENTIFIER},
                                                      "positive size_t integer at most AAD::ADJ_SIZE, excluding bool");
                 REQUIRE2(maximum > 0 && maximum <= AAD::ADJ_SIZE,
                          "InvalidJacobianRiskRequest: max_block_width must be in [1, " + String_(std::to_string(AAD::ADJ_SIZE)) + "]", ScriptError_);
                 return JacobianRiskRequest_{{RequestIds(inputs, {"JacobianRiskRequest_; inputs", IDENTIFIER}),
                                              RequestIds(outputs, {"JacobianRiskRequest_; outputs", IDENTIFIER}),
                                              RequestFactors(reportFactors, {"JacobianRiskRequest_; report_factors", IDENTIFIER}),
                                              RequestBudget(resultBudget, {"JacobianRiskRequest_; numeric_payload_budget_bytes", IDENTIFIER})},
                                             maximum,
                                             RequestBudget(recordingBudget, {"JacobianRiskRequest_; recording_capacity_budget_bytes", IDENTIFIER}),
                                             RequestBudget(scratchBudget, {"JacobianRiskRequest_; scratch_capacity_budget_bytes", IDENTIFIER})};
             }),
             py::kw_only(), py::arg("inputs") = py::none(), py::arg("outputs") = py::none(), py::arg("report_factors") = py::none(),
             py::arg("max_block_width") = 1, py::arg("numeric_payload_budget_bytes") = py::none(),
             py::arg("recording_capacity_budget_bytes") = py::none(), py::arg("scratch_capacity_budget_bytes") = py::none())
        .def_property_readonly("inputs", [](const JacobianRiskRequest_& value) { return RiskTextVector(value.selection_.inputs_); })
        .def_property_readonly("outputs", [](const JacobianRiskRequest_& value) { return RiskTextVector(value.selection_.outputs_); })
        .def_property_readonly("report_factors",
                               [](const JacobianRiskRequest_& value) {
                                   return value.selection_.reportFactors_
                                              ? std::optional<std::vector<double>>(CopyRiskVector(*value.selection_.reportFactors_))
                                              : std::nullopt;
                               })
        .def_property_readonly("max_block_width", [](const JacobianRiskRequest_& value) { return value.maxBlockWidth_; })
        .def_property_readonly("numeric_payload_budget_bytes",
                               [](const JacobianRiskRequest_& value) { return value.selection_.numericPayloadBudgetBytes_; })
        .def_property_readonly("recording_capacity_budget_bytes",
                               [](const JacobianRiskRequest_& value) { return value.recordingCapacityBudgetBytes_; })
        .def_property_readonly("scratch_capacity_budget_bytes", [](const JacobianRiskRequest_& value) { return value.scratchCapacityBudgetBytes_; });

    WithCopies(py::class_<JacobianRiskExecution_>(m, "JacobianRiskExecution_"))
        .def_property_readonly("actual_widths", [](const JacobianRiskExecution_& value) { return CopyRiskVector(value.actualWidths_); })
        .def_property_readonly("replay_attempts", [](const JacobianRiskExecution_& value) { return value.replayAttempts_; })
        .def_property_readonly("executed_paths", [](const JacobianRiskExecution_& value) { return value.executedPaths_; })
        .def_property_readonly("peak_recording_bytes", [](const JacobianRiskExecution_& value) { return value.peakRecordingBytes_; })
        .def_property_readonly("peak_scratch_bytes", [](const JacobianRiskExecution_& value) { return value.peakScratchBytes_; });

    WithCopies(py::class_<JacobianRiskResult_>(m, "JacobianRiskResult_"))
        .def_property_readonly("values", [](const JacobianRiskResult_& value) { return CopyRiskVector(value.Values()); })
        .def_property_readonly("jacobian", [](const JacobianRiskResult_& value) { return Matrix_<>(value.Jacobian()); })
        .def_property_readonly("reported_jacobian", &JacobianRiskResult_::ReportedJacobian)
        .def_property_readonly("output_axis", [](const JacobianRiskResult_& value) { return CopyRiskVector(value.OutputAxis()); })
        .def_property_readonly("complete_output_axis", [](const JacobianRiskResult_& value) { return CopyRiskVector(value.CompleteOutputAxis()); })
        .def_property_readonly("input_axis", [](const JacobianRiskResult_& value) { return CopyRiskVector(value.InputAxis()); })
        .def_property_readonly("complete_input_axis", [](const JacobianRiskResult_& value) { return CopyRiskVector(value.CompleteInputAxis()); })
        .def_property_readonly("provenance", [](const JacobianRiskResult_& value) { return value.Provenance(); })
        .def_property_readonly("execution", [](const JacobianRiskResult_& value) { return value.Execution(); });

    m.def(
        "MonteCarlo_ValueWithJacobianRisk",
        [](const std::shared_ptr<ScriptProductData_>& product, const std::shared_ptr<ModelData_>& model, const py::object& numPath,
           const py::object& request, const py::object& valuation, const py::object& simulation) {
            return EvaluatePythonRisk<JacobianRiskRequest_, &ValueByMonteCarloWithJacobianRisk>(
                product, model, numPath, request, valuation, simulation, {"MonteCarlo_ValueWithJacobianRisk", "JacobianRiskRequest_"});
        },
        py::arg("product"), py::arg("modelData"), py::arg("num_path"), py::kw_only(), py::arg("request") = py::none(),
        py::arg("valuation") = py::none(), py::arg("simulation") = py::none());
}

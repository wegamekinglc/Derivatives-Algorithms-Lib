//
// Created by Codex on 2026-10-06.
//

#include "bindings.h"

#include <pybind11/stl.h>

#include <dal-public/src/value.hpp>

#include "riskrequest.hpp"

using namespace Dal;
using namespace Dal::Python;
using namespace Dal::Script;

void init_bindings_weightedrisk(py::module_& m) {
    WithCopies(py::class_<WeightedRiskRequest_>(m, "WeightedRiskRequest_"))
        .def(py::init([](const py::object& inputs, const py::object& outputs, const py::object& weights, const py::object& reportFactors,
                         const py::object& budget) {
                 constexpr const char* IDENTIFIER = "InvalidWeightedRiskRequest";
                 return WeightedRiskRequest_{{RequestIds(inputs, {"WeightedRiskRequest_; inputs", IDENTIFIER}),
                                              RequestIds(outputs, {"WeightedRiskRequest_; outputs", IDENTIFIER}),
                                              RequestFactors(reportFactors, {"WeightedRiskRequest_; report_factors", IDENTIFIER}),
                                              RequestBudget(budget, {"WeightedRiskRequest_; numeric_payload_budget_bytes", IDENTIFIER})},
                                             RequestNumbers(weights, {"WeightedRiskRequest_; weights", IDENTIFIER}, "weights")};
             }),
             py::kw_only(), py::arg("inputs") = py::none(), py::arg("outputs") = py::none(), py::arg("weights") = py::none(),
             py::arg("report_factors") = py::none(), py::arg("numeric_payload_budget_bytes") = py::none())
        .def_property_readonly("inputs", [](const WeightedRiskRequest_& value) { return RiskTextVector(value.selection_.inputs_); })
        .def_property_readonly("outputs", [](const WeightedRiskRequest_& value) { return RiskTextVector(value.selection_.outputs_); })
        .def_property_readonly("weights",
                               [](const WeightedRiskRequest_& value) {
                                   return value.weights_ ? std::optional<std::vector<double>>(CopyRiskVector(*value.weights_)) : std::nullopt;
                               })
        .def_property_readonly("report_factors",
                               [](const WeightedRiskRequest_& value) {
                                   return value.selection_.reportFactors_
                                              ? std::optional<std::vector<double>>(CopyRiskVector(*value.selection_.reportFactors_))
                                              : std::nullopt;
                               })
        .def_property_readonly("numeric_payload_budget_bytes",
                               [](const WeightedRiskRequest_& value) { return value.selection_.numericPayloadBudgetBytes_; });

    py::class_<RiskOutputCoordinate_>(m, "RiskOutputCoordinate_")
        .def_property_readonly("id", [](const RiskOutputCoordinate_& value) { return Text(value.id_); })
        .def_property_readonly("label", [](const RiskOutputCoordinate_& value) { return Text(value.label_); })
        .def_property_readonly("slot", [](const RiskOutputCoordinate_& value) { return value.slot_; });

    WithCopies(py::class_<WeightedRiskResult_>(m, "WeightedRiskResult_"))
        .def_property_readonly("weighted_value", &WeightedRiskResult_::WeightedValue)
        .def_property_readonly("component_means", [](const WeightedRiskResult_& value) { return CopyRiskVector(value.ComponentMeans()); })
        .def_property_readonly("weights", [](const WeightedRiskResult_& value) { return CopyRiskVector(value.Weights()); })
        .def_property_readonly("output_axis", [](const WeightedRiskResult_& value) { return CopyRiskVector(value.OutputAxis()); })
        .def_property_readonly("jacobian", [](const WeightedRiskResult_& value) { return Matrix_<>(value.Jacobian()); })
        .def_property_readonly("reported_jacobian", &WeightedRiskResult_::ReportedJacobian)
        .def_property_readonly("input_axis", [](const WeightedRiskResult_& value) { return CopyRiskVector(value.InputAxis()); })
        .def_property_readonly("complete_input_axis", [](const WeightedRiskResult_& value) { return CopyRiskVector(value.CompleteInputAxis()); })
        .def_property_readonly("provenance", [](const WeightedRiskResult_& value) { return value.Provenance(); });

    m.def(
        "Product_Get_RiskOutputs",
        [](const std::shared_ptr<ScriptProductData_>& product) {
            REQUIRE2(product, "InvalidWeightedRiskRequest: product must not be null; field=product", ScriptError_);
            auto indexed = product->Product();
            indexed.IndexVariables();
            return CopyRiskVector(ScriptRiskOutputAxis(indexed));
        },
        py::arg("product"));

    m.def(
        "MonteCarlo_ValueWithWeightedRisk",
        [](const std::shared_ptr<ScriptProductData_>& product, const std::shared_ptr<ModelData_>& model, const py::object& numPath,
           const py::object& request, const py::object& valuation, const py::object& simulation) {
            return EvaluatePythonRisk<WeightedRiskRequest_, &ValueByMonteCarloWithWeightedRisk>(
                product, model, numPath, request, valuation, simulation, {"MonteCarlo_ValueWithWeightedRisk", "WeightedRiskRequest_"});
        },
        py::arg("product"), py::arg("modelData"), py::arg("num_path"), py::kw_only(), py::arg("request") = py::none(),
        py::arg("valuation") = py::none(), py::arg("simulation") = py::none());
}

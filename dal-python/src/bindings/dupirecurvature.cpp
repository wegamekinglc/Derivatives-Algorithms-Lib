//
// Created by Codex on 2026/10/10.
//

#include "bindings.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>

#include <pybind11/stl.h>

#include <dal-public/src/dupirecurvature.hpp>

#include "riskrequest.hpp"

using namespace Dal;
using namespace Dal::Python;

namespace {
    constexpr const char* IDENTIFIER = "InvalidCurvatureRequest";

    Vector_<> Steps(const py::handle& value) {
        constexpr const char* FIELD = "BumpOverAADRequest_; steps";
        static_cast<void>(RequestSequence(value, {FIELD, IDENTIFIER}));
        auto result = *RequestNumbers(value, {FIELD, IDENTIFIER}, "steps");
        REQUIRE(std::all_of(result.begin(), result.end(), [](double step) { return std::isfinite(step) && step > 0.0; }),
                "InvalidCurvatureRequest: BumpOverAADRequest_; steps must be finite and positive");
        return result;
    }

    AAD::BumpOverAADRequest_
    NewBumps(const py::object& directions, const py::object& steps, const py::object& numericBudget, const py::object& recordingBudget) {
        auto matrix = RequiredRiskInput<Matrix_<>>(directions, "BumpOverAADRequest_; directions", "DoubleMatrix_", IDENTIFIER);
        auto sizes = Steps(steps);
        REQUIRE(sizes.size() == static_cast<size_t>(matrix.Rows()),
                "InvalidCurvatureRequest: BumpOverAADRequest_; steps requires one value per direction row");
        REQUIRE(std::all_of(matrix.begin(), matrix.end(), [](double value) { return std::isfinite(value); }),
                "InvalidCurvatureRequest: BumpOverAADRequest_; directions must be finite");
        return {std::move(matrix), std::move(sizes), RequestBudget(numericBudget, {"BumpOverAADRequest_; numeric_payload_budget_bytes", IDENTIFIER}),
                RequestBudget(recordingBudget, {"BumpOverAADRequest_; recording_capacity_budget_bytes", IDENTIFIER})};
    }

    DupireScriptCurvatureRequest_ NewRequest(const py::object& risk, const py::object& bumps) {
        return {RequiredRiskInput<DupireScriptRiskRequest_>(risk, "DupireScriptCurvatureRequest_; risk", "DupireScriptRiskRequest_", IDENTIFIER),
                RequiredRiskInput<AAD::BumpOverAADRequest_>(bumps, "DupireScriptCurvatureRequest_; bumps", "BumpOverAADRequest_", IDENTIFIER)};
    }

    template <class T_> Handle_<T_> PlanHandle(const py::handle& value, const char* field, const char* type) {
        if (!py::isinstance<T_>(value))
            throw py::type_error(InputContext(value, field, type, IDENTIFIER));
        return Handle_<T_>(py::cast<std::shared_ptr<T_>>(value));
    }

    DupireScriptCurvaturePlan_ NewPlan(
        const py::object& product, const py::object& model, const py::object& calibration, const py::object& component, const py::object& request) {
        const auto nativeProduct = PlanHandle<ScriptProductData_>(product, "DupireScriptCurvaturePlan_New; product", "ScriptProductData_");
        const auto nativeModel = PlanHandle<ModelData_>(model, "DupireScriptCurvaturePlan_New; modelData", "ModelData_");
        const auto source = RequiredRiskInput<DupireCalibrationSnapshot_>(calibration, "DupireScriptCurvaturePlan_New; calibration",
                                                                          "DupireCalibrationSnapshot_", IDENTIFIER);
        const auto name = SettingStringInput(component, "DupireScriptCurvaturePlan_New; component", IDENTIFIER);
        const auto configuration = RequiredRiskInput<DupireScriptCurvatureRequest_>(request, "DupireScriptCurvaturePlan_New; request",
                                                                                    "DupireScriptCurvatureRequest_", IDENTIFIER);
        py::gil_scoped_release release;
        return PlanDupireScriptCurvature(nativeProduct, nativeModel, source, name, configuration);
    }

    DupireScriptCurvatureResult_ NewResult(const py::object& plan) {
        const auto nativePlan =
            RequiredRiskInput<DupireScriptCurvaturePlan_>(plan, "DupireScriptCurvatureResult_New; plan", "DupireScriptCurvaturePlan_", IDENTIFIER);
        py::gil_scoped_release release;
        return ValueByMonteCarloWithDupireCurvature(nativePlan);
    }
} // namespace

void init_bindings_dupirecurvature(py::module_& m) {
    WithCopies(py::class_<AAD::BumpOverAADRequest_>(m, "BumpOverAADRequest_"))
        .def(py::init(&NewBumps), py::kw_only(), py::arg("directions"), py::arg("steps"), py::arg("numeric_payload_budget_bytes") = py::none(),
             py::arg("recording_capacity_budget_bytes") = py::none())
        .def_property_readonly("directions", [](const AAD::BumpOverAADRequest_& value) { return Matrix_<>(value.directions_); })
        .def_property_readonly("steps", [](const AAD::BumpOverAADRequest_& value) { return CopyRiskVector(value.steps_); })
        .def_property_readonly("numeric_payload_budget_bytes", [](const AAD::BumpOverAADRequest_& value) { return value.numericPayloadBudgetBytes_; })
        .def_property_readonly("recording_capacity_budget_bytes",
                               [](const AAD::BumpOverAADRequest_& value) { return value.recordingCapacityBudgetBytes_; });

    WithCopies(py::class_<DupireScriptCurvatureRequest_>(m, "DupireScriptCurvatureRequest_"))
        .def(py::init(&NewRequest), py::kw_only(), py::arg("risk"), py::arg("bumps"))
        .def_property_readonly("risk", [](const DupireScriptCurvatureRequest_& value) { return value.risk_; })
        .def_property_readonly("bumps", [](const DupireScriptCurvatureRequest_& value) { return value.bumps_; });

    WithCopies(py::class_<DupireScriptCurvaturePlan_>(m, "DupireScriptCurvaturePlan_"))
        .def_property_readonly("base_plan", [](const DupireScriptCurvaturePlan_& value) { return value.BasePlan(); })
        .def_property_readonly("point", [](const DupireScriptCurvaturePlan_& value) { return CopyRiskVector(value.Point()); })
        .def_property_readonly("directions", [](const DupireScriptCurvaturePlan_& value) { return Matrix_<>(value.Directions()); })
        .def_property_readonly("steps", [](const DupireScriptCurvaturePlan_& value) { return CopyRiskVector(value.Steps()); })
        .def_property_readonly("numeric_payload_bytes", &DupireScriptCurvaturePlan_::NumericPayloadBytes);
    m.def("DupireScriptCurvaturePlan_New", &NewPlan, py::arg("product"), py::arg("modelData"), py::arg("calibration"), py::arg("component"),
          py::arg("request"));

    WithCopies(py::class_<DupireScriptCurvatureExecution_>(m, "DupireScriptCurvatureExecution_"))
        .def_property_readonly("method", [](const DupireScriptCurvatureExecution_& value) { return Text(value.method_); })
        .def_property_readonly("quote_gradient_evaluations",
                               [](const DupireScriptCurvatureExecution_& value) { return value.quoteGradientEvaluations_; })
        .def_property_readonly("paths_per_evaluation", [](const DupireScriptCurvatureExecution_& value) { return value.pathsPerEvaluation_; })
        .def_property_readonly("numeric_payload_bytes", [](const DupireScriptCurvatureExecution_& value) { return value.numericPayloadBytes_; });

    WithCopies(py::class_<DupireScriptCurvatureResult_>(m, "DupireScriptCurvatureResult_"))
        .def_property_readonly("base", [](const DupireScriptCurvatureResult_& value) { return value.Base(); })
        .def_property_readonly("point", [](const DupireScriptCurvatureResult_& value) { return CopyRiskVector(value.Point()); })
        .def_property_readonly("gradient", [](const DupireScriptCurvatureResult_& value) { return CopyRiskVector(value.Gradient()); })
        .def_property_readonly("input_axis", [](const DupireScriptCurvatureResult_& value) { return CopyRiskVector(value.InputAxis()); })
        .def_property_readonly("directions", [](const DupireScriptCurvatureResult_& value) { return Matrix_<>(value.Directions()); })
        .def_property_readonly("steps", [](const DupireScriptCurvatureResult_& value) { return CopyRiskVector(value.Steps()); })
        .def_property_readonly("hessian_products", [](const DupireScriptCurvatureResult_& value) { return Matrix_<>(value.HessianProducts()); })
        .def_property_readonly("execution", [](const DupireScriptCurvatureResult_& value) { return value.Execution(); });
    m.def("DupireScriptCurvatureResult_New", &NewResult, py::arg("plan"));
}

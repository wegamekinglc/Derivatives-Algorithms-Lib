#include "bindings.h"

#include <cmath>
#include <limits>
#include <string>

#include <pybind11/stl.h>

#include <dal-public/src/dupirequotesecondorder.hpp>

#include "riskrequest.hpp"

using namespace Dal;
using namespace Dal::Python;

namespace {
    constexpr const char* IDENTIFIER = "InvalidDupireQuoteSecondOrder";

    double NumberInput(const py::handle& value, const char* field) {
        const auto error = InputContext(value, field, "finite int or float, excluding bool and enums", IDENTIFIER);
        if (PyBool_Check(value.ptr()) || IsEnum(value) || (!PyLong_Check(value.ptr()) && !PyFloat_Check(value.ptr())))
            throw py::type_error(error);
        const double result = PyFloat_AsDouble(value.ptr());
        if (PyErr_Occurred())
            throw py::error_already_set();
        if (!std::isfinite(result))
            throw py::value_error(error);
        return result;
    }

    int GridCount(const py::handle& value, const char* field) {
        const auto count = RequestUnsigned(value, {field, IDENTIFIER}, "nonnegative int, excluding bool and enums");
        if (count > static_cast<size_t>((std::numeric_limits<int>::max)()))
            throw py::value_error(InputContext(value, field, "nonnegative int in native range", IDENTIFIER));
        return static_cast<int>(count);
    }

    DupireEuropeanCallPdeGrid_ GridInput(const py::handle& value) {
        constexpr const char* FIELD = "DupireQuoteSecondOrderPlan_New; grid";
        const auto grid = RequestSequence(value, {FIELD, IDENTIFIER});
        if (grid.size() != 3)
            throw py::value_error("DupireQuoteSecondOrderPlan_New; grid requires (log_spot_half_width, space_nodes, time_steps)");
        return {NumberInput(grid[0], "DupireQuoteSecondOrderPlan_New; grid.log_spot_half_width"),
                GridCount(grid[1], "DupireQuoteSecondOrderPlan_New; grid.space_nodes"),
                GridCount(grid[2], "DupireQuoteSecondOrderPlan_New; grid.time_steps")};
    }

    Vector_<DupireQuoteCoordinate_> AxisInput(const py::handle& value) {
        constexpr const char* FIELD = "DupireQuoteSecondOrderResult_New; input_axis";
        Vector_<DupireQuoteCoordinate_> axis;
        for (const auto& entry : RequestSequence(value, {FIELD, IDENTIFIER})) {
            const auto coordinate = RequestSequence(entry, {FIELD, IDENTIFIER});
            if (coordinate.size() != 3)
                throw py::value_error("DupireQuoteSecondOrderResult_New; input_axis entries require (quote_id, strike, maturity)");
            axis.push_back(
                {SettingStringInput(coordinate[0], FIELD, IDENTIFIER), NumberInput(coordinate[1], FIELD), NumberInput(coordinate[2], FIELD)});
        }
        return axis;
    }

    py::list CopyAxis(const Vector_<DupireQuoteCoordinate_>& axis) {
        py::list result;
        for (const auto& quote : axis)
            result.append(py::make_tuple(Text(quote.id_), quote.strike_, quote.maturity_));
        return result;
    }

    DupireQuoteSecondOrderPlan_ NewPlan(const py::object& calibration,
                                        const py::object& strike,
                                        const py::object& maturity,
                                        const py::object& underlyingId,
                                        const py::object& currency,
                                        const py::object& grid) {
        const auto snapshot = RequiredRiskInput<DupireCalibrationSnapshot_>(calibration, "DupireQuoteSecondOrderPlan_New; calibration",
                                                                            "DupireCalibrationSnapshot_", IDENTIFIER);
        const DupireEuropeanCall_ call{NumberInput(strike, "DupireQuoteSecondOrderPlan_New; strike"),
                                       NumberInput(maturity, "DupireQuoteSecondOrderPlan_New; maturity"),
                                       SettingStringInput(underlyingId, "DupireQuoteSecondOrderPlan_New; underlying_id", IDENTIFIER),
                                       Ccy_(SettingStringInput(currency, "DupireQuoteSecondOrderPlan_New; currency", IDENTIFIER))};
        const auto pde = GridInput(grid);
        py::gil_scoped_release release;
        return PrepareDupireEuropeanCallQuoteRisk(snapshot, call, pde);
    }

    DupireQuoteSecondOrderResult_ NewResult(const py::object& plan,
                                            const py::object& inputAxis,
                                            const py::object& directions,
                                            const py::object& payloadBudget,
                                            const py::object& recordingBudget) {
        const auto nativePlan =
            RequiredRiskInput<DupireQuoteSecondOrderPlan_>(plan, "DupireQuoteSecondOrderResult_New; plan", "DupireQuoteSecondOrderPlan_", IDENTIFIER);
        const auto axis = AxisInput(inputAxis);
        const auto seeds = RequiredRiskInput<Matrix_<>>(directions, "DupireQuoteSecondOrderResult_New; directions", "DoubleMatrix_", IDENTIFIER);
        const AAD::SecondOrderOptions_ options{
            RequestBudget(payloadBudget, {"DupireQuoteSecondOrderResult_New; numeric_payload_budget_bytes", IDENTIFIER}),
            RequestBudget(recordingBudget, {"DupireQuoteSecondOrderResult_New; recording_capacity_budget_bytes", IDENTIFIER})};
        py::gil_scoped_release release;
        return EvaluateDupireQuoteSecondOrder(nativePlan, axis, seeds, options);
    }
} // namespace

void init_bindings_dupirequotesecondorder(py::module_& m) {
    py::class_<DupireQuoteSecondOrderPlan_>(m, "DupireQuoteSecondOrderPlan_")
        .def_property_readonly("input_axis", [](const DupireQuoteSecondOrderPlan_& plan) { return CopyAxis(plan.InputAxis()); });
    py::class_<DupireQuoteSecondOrderResult_>(m, "DupireQuoteSecondOrderResult_")
        .def_property_readonly("value", [](const DupireQuoteSecondOrderResult_& result) { return result.Risk().Value(); })
        .def_property_readonly("point", [](const DupireQuoteSecondOrderResult_& result) { return CopyRiskVector(result.Risk().Point()); })
        .def_property_readonly("input_axis", [](const DupireQuoteSecondOrderResult_& result) { return CopyAxis(result.InputAxis()); })
        .def_property_readonly("gradient", [](const DupireQuoteSecondOrderResult_& result) { return CopyRiskVector(result.Risk().Gradient()); })
        .def_property_readonly("directions", [](const DupireQuoteSecondOrderResult_& result) { return Matrix_<>(result.Risk().Directions()); })
        .def_property_readonly("directional_derivatives",
                               [](const DupireQuoteSecondOrderResult_& result) { return CopyRiskVector(result.Risk().DirectionalDerivatives()); })
        .def_property_readonly("hessian_products",
                               [](const DupireQuoteSecondOrderResult_& result) { return Matrix_<>(result.Risk().HessianProducts()); })
        .def_property_readonly("quote_unit", [](const DupireQuoteSecondOrderResult_& result) { return Text(result.QuoteUnit()); })
        .def_property_readonly("method", [](const DupireQuoteSecondOrderResult_& result) {
            const auto& risk = result.Risk();
            const auto* pointwise = risk.Pointwise();
            return Text(pointwise ? pointwise->Execution().method_ : risk.Shared()->Execution().method_);
        });
    m.def("DupireQuoteSecondOrderPlan_New", &NewPlan, py::arg("calibration"), py::kw_only(), py::arg("strike"), py::arg("maturity"),
          py::arg("underlying_id"), py::arg("currency"), py::arg("grid"));
    m.def("DupireQuoteSecondOrderResult_New", &NewResult, py::arg("plan"), py::kw_only(), py::arg("input_axis"), py::arg("directions"),
          py::arg("numeric_payload_budget_bytes") = py::none(), py::arg("recording_capacity_budget_bytes") = py::none());
}

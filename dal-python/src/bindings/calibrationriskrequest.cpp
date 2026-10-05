//
// Created by Codex on 2026/10/5.
//

#include "bindings.h"

#include <pybind11/stl.h>

#include <dal-public/src/calibrationriskrequest.hpp>

#include "riskrequest.hpp"

using namespace Dal;
using namespace Dal::Python;

namespace {
    constexpr const char* IDENTIFIER = "InvalidCalibrationRiskRequest";

    std::optional<std::string> OptionalText(const std::optional<String_>& value) {
        return value ? std::optional<std::string>(Text(*value)) : std::nullopt;
    }
} // namespace

void init_bindings_calibrationriskrequest(py::module_& m) {
    WithCopies(py::class_<CalibrationRiskRequest_>(m, "CalibrationRiskRequest_"))
        .def(py::init([](const py::object& inputs, const py::object& reportFactors, const py::object& budget) {
                 return CalibrationRiskRequest_{RequestIds(inputs, {"CalibrationRiskRequest_; inputs", IDENTIFIER}),
                                                RequestFactors(reportFactors, {"CalibrationRiskRequest_; report_factors", IDENTIFIER}),
                                                RequestBudget(budget, {"CalibrationRiskRequest_; numeric_payload_budget_bytes", IDENTIFIER})};
             }),
             py::kw_only(), py::arg("inputs") = py::none(), py::arg("report_factors") = py::none(),
             py::arg("numeric_payload_budget_bytes") = py::none())
        .def_property_readonly("inputs", [](const CalibrationRiskRequest_& value) { return RiskTextVector(value.inputs_); })
        .def_property_readonly("report_factors",
                               [](const CalibrationRiskRequest_& value) {
                                   return value.reportFactors_ ? std::optional<std::vector<double>>(CopyRiskVector(*value.reportFactors_))
                                                               : std::nullopt;
                               })
        .def_property_readonly("numeric_payload_budget_bytes", [](const CalibrationRiskRequest_& value) { return value.numericPayloadBudgetBytes_; });

    WithCopies(py::class_<CalibrationQuoteCoordinate_>(m, "CalibrationQuoteCoordinate_"))
        .def_property_readonly("id", [](const CalibrationQuoteCoordinate_& value) { return Text(value.id_); })
        .def_property_readonly("label", [](const CalibrationQuoteCoordinate_& value) { return Text(value.label_); })
        .def_property_readonly("ordinal", [](const CalibrationQuoteCoordinate_& value) { return value.ordinal_; })
        .def_property_readonly("row", [](const CalibrationQuoteCoordinate_& value) { return value.row_; })
        .def_property_readonly("column", [](const CalibrationQuoteCoordinate_& value) { return value.column_; })
        .def_property_readonly("native_unit", [](const CalibrationQuoteCoordinate_& value) { return Text(value.nativeUnit_); })
        .def_property_readonly("report_scale", [](const CalibrationQuoteCoordinate_& value) { return value.reportScale_; })
        .def_property_readonly("value", [](const CalibrationQuoteCoordinate_& value) { return value.value_; })
        .def_property_readonly("strike", [](const CalibrationQuoteCoordinate_& value) { return value.strike_; })
        .def_property_readonly("maturity", [](const CalibrationQuoteCoordinate_& value) { return value.maturity_; })
        .def_property_readonly("block_key", [](const CalibrationQuoteCoordinate_& value) { return OptionalText(value.blockKey_); })
        .def_property_readonly("block_ordinal", [](const CalibrationQuoteCoordinate_& value) { return value.blockOrdinal_; });

    WithCopies(py::class_<CalibrationRiskPlan_>(m, "CalibrationRiskPlan_"))
        .def_property_readonly("calibration", [](const CalibrationRiskPlan_& value) { return value.Calibration(); })
        .def_property_readonly("complete_input_axis", [](const CalibrationRiskPlan_& value) { return CopyRiskVector(value.CompleteInputAxis()); })
        .def_property_readonly("input_axis", [](const CalibrationRiskPlan_& value) { return CopyRiskVector(value.InputAxis()); })
        .def_property_readonly("selected_ordinals", [](const CalibrationRiskPlan_& value) { return CopyRiskVector(value.SelectedOrdinals()); })
        .def_property_readonly("numeric_payload_bytes", &CalibrationRiskPlan_::NumericPayloadBytes);
    m.def(
        "CalibrationRiskPlan_New",
        [](const py::object& calibration, const py::object& request) {
            const auto source =
                RequiredRiskInput<CalibrationPullback_>(calibration, "CalibrationRiskPlan_New; calibration", "CalibrationPullback_", IDENTIFIER);
            const auto configuration = SettingsInput<CalibrationRiskRequest_>(request, "CalibrationRiskPlan_New; request", "CalibrationRiskRequest_");
            py::gil_scoped_release release;
            return PlanCalibrationRiskRequest(source, configuration);
        },
        py::arg("calibration"), py::kw_only(), py::arg("request") = py::none());

    WithCopies(py::class_<CalibrationRiskResult_>(m, "CalibrationRiskResult_"))
        .def_property_readonly("plan", [](const CalibrationRiskResult_& value) { return value.Plan(); })
        .def_property_readonly("quote_risk", [](const CalibrationRiskResult_& value) { return value.QuoteRisk(); })
        .def_property_readonly("jacobian", &CalibrationRiskResult_::Jacobian)
        .def_property_readonly("calibration_jacobian", &CalibrationRiskResult_::CalibrationJacobian)
        .def_property_readonly("direct_jacobian", &CalibrationRiskResult_::DirectJacobian)
        .def_property_readonly("reported_jacobian", &CalibrationRiskResult_::ReportedJacobian);
    m.def(
        "CalibrationRiskResult_New",
        [](const py::object& plan, const py::object& parameters, const py::object& direct) {
            const auto planned = RequiredRiskInput<CalibrationRiskPlan_>(plan, "CalibrationRiskResult_New; plan", "CalibrationRiskPlan_", IDENTIFIER);
            const auto seeds = RequiredRiskInput<CalibrationParameterAdjoints_>(parameters, "CalibrationRiskResult_New; parameter_adjoints",
                                                                                "CalibrationParameterAdjoints_", IDENTIFIER);
            const auto contribution = direct.is_none()
                                          ? std::nullopt
                                          : std::optional<CalibrationDirectQuoteAdjoints_>(RequiredRiskInput<CalibrationDirectQuoteAdjoints_>(
                                                direct, "CalibrationRiskResult_New; direct", "CalibrationDirectQuoteAdjoints_ or None", IDENTIFIER));
            py::gil_scoped_release release;
            return PullbackCalibrationWithRisk(planned, seeds, contribution);
        },
        py::arg("plan"), py::arg("parameter_adjoints"), py::kw_only(), py::arg("direct") = py::none());
}

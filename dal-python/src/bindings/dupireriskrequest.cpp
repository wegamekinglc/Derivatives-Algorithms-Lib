//
// Created by Codex on 2026/10/5.
//

#include "bindings.h"

#include <pybind11/stl.h>

#include <dal-public/src/dupireriskrequest.hpp>

#include "riskrequest.hpp"

using namespace Dal;
using namespace Dal::Python;

namespace {
    constexpr const char* IDENTIFIER = "InvalidDupireRiskRequest";

    int RequestPaths(const py::handle& value) {
        const auto context =
            InputContext(value, "DupireScriptRiskRequest_; num_paths", "integer in 1..INT_MAX, excluding bool and enums", "InvalidPathCount");
        return static_cast<int>(IntegerInput(value, context, 1, (std::numeric_limits<int>::max)(), "; number of Monte Carlo paths must be positive"));
    }

    Vector_<DupireQuoteBinding_> DirectBindings(const py::handle& value) {
        Vector_<DupireQuoteBinding_> bindings;
        if (value.is_none())
            return bindings;
        constexpr const char* FIELD = "DupireScriptRiskRequest_; direct_bindings";
        for (const auto& item : RequestSequence(value, {FIELD, IDENTIFIER}))
            bindings.push_back(RequiredRiskInput<DupireQuoteBinding_>(item, FIELD, "DupireQuoteBinding_", IDENTIFIER));
        return bindings;
    }

    template <class T_> Handle_<T_> NativeHandle(const py::handle& value, const char* field, const char* type) {
        if (!py::isinstance<T_>(value))
            throw py::type_error(InputContext(value, field, type, IDENTIFIER));
        return Handle_<T_>(py::cast<std::shared_ptr<T_>>(value));
    }

    DupireScriptRiskRequest_ NewRequest(const py::object& paths,
                                        const py::object& quotes,
                                        const py::object& bindings,
                                        const py::object& direct,
                                        const py::object& valuation,
                                        const py::object& simulation) {
        return {RequestPaths(paths),
                SettingsInput<CalibrationRiskRequest_>(quotes, "DupireScriptRiskRequest_; quotes", "CalibrationRiskRequest_"),
                DirectBindings(bindings),
                direct.is_none() ? std::nullopt
                                 : std::optional<CalibrationDirectQuoteAdjoints_>(RequiredRiskInput<CalibrationDirectQuoteAdjoints_>(
                                       direct, "DupireScriptRiskRequest_; direct", "CalibrationDirectQuoteAdjoints_ or None", IDENTIFIER)),
                SettingsInput<ScriptValuationSettings_>(valuation, "DupireScriptRiskRequest_; valuation", "ScriptValuationSettings_"),
                simulation.is_none() ? DefaultRiskMonteCarloSettings()
                                     : SettingsInput<MonteCarloSettings_>(simulation, "DupireScriptRiskRequest_; simulation", "MonteCarloSettings_")};
    }
} // namespace

void init_bindings_dupireriskrequest(py::module_& m) {
    WithCopies(py::class_<DupireQuoteBinding_>(m, "DupireQuoteBinding_"))
        .def(py::init([](const py::object& ordinal, const py::object& quote) {
                 return DupireQuoteBinding_{RequestUnsigned(ordinal, {"DupireQuoteBinding_; constant_ordinal", IDENTIFIER},
                                                            "nonnegative size_t integer, excluding bool and enums"),
                                            SettingStringInput(quote, "DupireQuoteBinding_; quote_id", IDENTIFIER)};
             }),
             py::kw_only(), py::arg("constant_ordinal"), py::arg("quote_id"))
        .def_property_readonly("constant_ordinal", [](const DupireQuoteBinding_& value) { return value.constantOrdinal_; })
        .def_property_readonly("quote_id", [](const DupireQuoteBinding_& value) { return Text(value.quoteId_); });

    WithCopies(py::class_<DupireScriptRiskRequest_>(m, "DupireScriptRiskRequest_"))
        .def(py::init(&NewRequest), py::kw_only(), py::arg("num_paths"), py::arg("quotes") = py::none(), py::arg("direct_bindings") = py::none(),
             py::arg("direct") = py::none(), py::arg("valuation") = py::none(), py::arg("simulation") = py::none())
        .def_property_readonly("num_paths", [](const DupireScriptRiskRequest_& value) { return value.numPaths_; })
        .def_property_readonly("quotes", [](const DupireScriptRiskRequest_& value) { return value.quotes_; })
        .def_property_readonly("direct_bindings", [](const DupireScriptRiskRequest_& value) { return CopyRiskVector(value.directBindings_); })
        .def_property_readonly("direct", [](const DupireScriptRiskRequest_& value) { return value.direct_; })
        .def_property_readonly("valuation", [](const DupireScriptRiskRequest_& value) { return value.valuation_; })
        .def_property_readonly("simulation", [](const DupireScriptRiskRequest_& value) { return value.simulation_; });

    WithCopies(py::class_<DupireScriptRiskPlan_>(m, "DupireScriptRiskPlan_"))
        .def_property_readonly("component", [](const DupireScriptRiskPlan_& value) { return Text(value.Component()); })
        .def_property_readonly("quote_plan", [](const DupireScriptRiskPlan_& value) { return value.QuotePlan(); })
        .def_property_readonly("complete_input_axis", [](const DupireScriptRiskPlan_& value) { return CopyRiskVector(value.CompleteInputAxis()); })
        .def_property_readonly("required_input_axis", [](const DupireScriptRiskPlan_& value) { return CopyRiskVector(value.RequiredInputAxis()); })
        .def_property_readonly("direct_bindings", [](const DupireScriptRiskPlan_& value) { return CopyRiskVector(value.DirectBindings()); })
        .def_property_readonly("num_paths", &DupireScriptRiskPlan_::NumPaths)
        .def_property_readonly("valuation_settings", [](const DupireScriptRiskPlan_& value) { return value.ValuationSettings(); })
        .def_property_readonly("simulation_settings", [](const DupireScriptRiskPlan_& value) { return value.SimulationSettings(); })
        .def_property_readonly("numeric_payload_bytes", &DupireScriptRiskPlan_::NumericPayloadBytes);
    m.def(
        "DupireScriptRiskPlan_New",
        [](const py::object& product, const py::object& model, const py::object& calibration, const py::object& component,
           const py::object& request) {
            const auto nativeProduct = NativeHandle<ScriptProductData_>(product, "DupireScriptRiskPlan_New; product", "ScriptProductData_");
            const auto nativeModel = NativeHandle<ModelData_>(model, "DupireScriptRiskPlan_New; modelData", "ModelData_");
            const auto source = RequiredRiskInput<DupireCalibrationSnapshot_>(calibration, "DupireScriptRiskPlan_New; calibration",
                                                                              "DupireCalibrationSnapshot_", IDENTIFIER);
            const auto name = SettingStringInput(component, "DupireScriptRiskPlan_New; component", IDENTIFIER);
            const auto configuration =
                RequiredRiskInput<DupireScriptRiskRequest_>(request, "DupireScriptRiskPlan_New; request", "DupireScriptRiskRequest_", IDENTIFIER);
            py::gil_scoped_release release;
            return PlanDupireScriptRisk(nativeProduct, nativeModel, source, name, configuration);
        },
        py::arg("product"), py::arg("modelData"), py::arg("calibration"), py::arg("component"), py::arg("request"));

    WithCopies(py::class_<DupireScriptRiskResult_>(m, "DupireScriptRiskResult_"))
        .def_property_readonly("valuation", [](const DupireScriptRiskResult_& value) { return value.Valuation(); })
        .def_property_readonly("quote_risk", [](const DupireScriptRiskResult_& value) { return value.QuoteRisk(); })
        .def_property_readonly("component", [](const DupireScriptRiskResult_& value) { return Text(value.Component()); })
        .def_property_readonly("method", [](const DupireScriptRiskResult_& value) { return Text(value.Method()); })
        .def_property_readonly("numeric_payload_bytes", &DupireScriptRiskResult_::NumericPayloadBytes);
    m.def(
        "DupireScriptRiskResult_New",
        [](const py::object& plan) {
            const auto nativePlan =
                RequiredRiskInput<DupireScriptRiskPlan_>(plan, "DupireScriptRiskResult_New; plan", "DupireScriptRiskPlan_", IDENTIFIER);
            py::gil_scoped_release release;
            return ValueByMonteCarloWithDupireRisk(nativePlan);
        },
        py::arg("plan"));
}

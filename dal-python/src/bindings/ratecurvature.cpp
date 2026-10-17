//
// Created by Codex on 2026/10/10.
//

#include "bindings.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <utility>

#include <pybind11/stl.h>

#include <dal-public/src/ratecurvature.hpp>

#include "riskrequest.hpp"

using namespace Dal;
using namespace Dal::Python;

namespace {
    constexpr const char* IDENTIFIER = "InvalidRateCurvatureRequest";

    py::sequence Sequence(const py::handle& value, const char* field, const char* expected) {
        if (!py::isinstance<py::list>(value) && !py::isinstance<py::tuple>(value))
            throw py::type_error(InputContext(value, field, expected, IDENTIFIER));
        return py::reinterpret_borrow<py::sequence>(value);
    }

    Vector_<> Numbers(const py::handle& value, const char* field) {
        static_cast<void>(Sequence(value, field, "list or tuple of finite real numbers"));
        auto result = *RequestNumbers(value, {field, IDENTIFIER}, field);
        REQUIRE(std::all_of(result.begin(), result.end(), [](double number) { return std::isfinite(number); }),
                String_(std::string(IDENTIFIER) + ": " + field + "; values must be finite"));
        return result;
    }

    template <class T_> void CalibrationFactory(py::module_& module) {
        module.def(
            "RateCalibration_New",
            [](const T_& input) {
                const auto spec = input;
                py::gil_scoped_release release;
                return NewRateCalibration(spec);
            },
            py::arg("spec"));
    }

    RateCalibrationSnapshot_ Recalibrate(const py::object& calibration, const py::object& quotes) {
        const auto source = RequiredRiskInput<RateCalibrationSnapshot_>(calibration, "RateCalibration_Recalibrate; calibration",
                                                                        "RateCalibrationSnapshot_", IDENTIFIER);
        const auto point = Numbers(quotes, "RateCalibration_Recalibrate; quotes");
        py::gil_scoped_release release;
        return RecalibrateRateWithRisk(source, point);
    }

    RateTradeQuoteCurvatureSettings_ NewSettings(const py::object& weights, const py::object& fixings) {
        auto values = weights.is_none() ? Vector_<>() : Numbers(weights, "RateTradeQuoteCurvatureSettings_; weights");
        Handle_<MarketFixingSnapshot_> history;
        if (!fixings.is_none()) {
            if (!py::isinstance<MarketFixingSnapshot_>(fixings))
                throw py::type_error(InputContext(fixings, "RateTradeQuoteCurvatureSettings_; fixings", "MarketFixingSnapshot_ or None", IDENTIFIER));
            history = Handle_<MarketFixingSnapshot_>(py::cast<std::shared_ptr<MarketFixingSnapshot_>>(fixings));
        }
        return {std::move(values), std::move(history)};
    }

    RateTradeQuoteCurvatureResult_
    NewTradeCurvature(const py::object& trades, const py::object& calibration, const py::object& bumps, const py::object& settings) {
        Vector_<RateTradeDefinition_> rows;
        for (const auto& trade : Sequence(trades, "RateTradeQuoteCurvature; trades", "list or tuple of RateTradeDefinition_"))
            rows.push_back(RequiredRiskInput<RateTradeDefinition_>(trade, "RateTradeQuoteCurvature; trades[" + std::to_string(rows.size()) + "]",
                                                                   "RateTradeDefinition_", IDENTIFIER));
        const auto source =
            RequiredRiskInput<RateCalibrationSnapshot_>(calibration, "RateTradeQuoteCurvature; calibration", "RateCalibrationSnapshot_", IDENTIFIER);
        const auto request = RequiredRiskInput<AAD::BumpOverAADRequest_>(bumps, "RateTradeQuoteCurvature; bumps", "BumpOverAADRequest_", IDENTIFIER);
        const auto configuration = settings.is_none()
                                       ? RateTradeQuoteCurvatureSettings_()
                                       : RequiredRiskInput<RateTradeQuoteCurvatureSettings_>(settings, "RateTradeQuoteCurvature; settings",
                                                                                             "RateTradeQuoteCurvatureSettings_", IDENTIFIER);
        py::gil_scoped_release release;
        return EvaluateRateTradeQuoteCurvature(rows, source, request, configuration);
    }
} // namespace

void init_bindings_ratecurvature(py::module_& m) {
    WithCopies(py::class_<RateCalibrationSnapshot_>(m, "RateCalibrationSnapshot_"))
        .def_property_readonly("point", [](const RateCalibrationSnapshot_& value) { return CopyRiskVector(value.Point()); })
        .def_property_readonly("parameters", [](const RateCalibrationSnapshot_& value) { return CopyRiskVector(value.Parameters()); })
        .def_property_readonly("provenance", [](const RateCalibrationSnapshot_& value) { return value.Provenance(); });
    CalibrationFactory<CurveCalibrationSpec_>(m);
    CalibrationFactory<JointMultiCurveCalibrationSpec_>(m);
    CalibrationFactory<CrossCurrencyCalibrationSpec_>(m);
    CalibrationFactory<JointXccyCalibrationSpec_>(m);
    m.def("RateCalibration_Recalibrate", &Recalibrate, py::arg("calibration"), py::arg("quotes"));

    WithCopies(py::class_<RateTradeQuoteCurvatureSettings_>(m, "RateTradeQuoteCurvatureSettings_"))
        .def(py::init(&NewSettings), py::kw_only(), py::arg("weights") = py::none(), py::arg("fixings") = py::none())
        .def_property_readonly("weights", [](const RateTradeQuoteCurvatureSettings_& value) { return CopyRiskVector(value.weights_); })
        .def_property_readonly(
            "fixings", [](const RateTradeQuoteCurvatureSettings_& value) { return std::const_pointer_cast<MarketFixingSnapshot_>(value.fixings_); });

    WithCopies(py::class_<RateQuoteCurvatureExecution_>(m, "RateQuoteCurvatureExecution_"))
        .def_property_readonly("method", [](const RateQuoteCurvatureExecution_& value) { return Text(value.method_); })
        .def_readonly("quote_gradient_evaluations", &RateQuoteCurvatureExecution_::quoteGradientEvaluations_)
        .def_readonly("calibrations", &RateQuoteCurvatureExecution_::calibrations_)
        .def_readonly("objective_reverse_sweeps", &RateQuoteCurvatureExecution_::objectiveReverseSweeps_)
        .def_readonly("numeric_payload_bytes", &RateQuoteCurvatureExecution_::numericPayloadBytes_)
        .def_readonly("peak_tape_bytes", &RateQuoteCurvatureExecution_::peakTapeBytes_)
        .def_readonly("cleanup_reserve_bytes", &RateQuoteCurvatureExecution_::cleanupReserveBytes_);

    WithCopies(py::class_<RateQuoteCurvatureResult_>(m, "RateQuoteCurvatureResult_"))
        .def_property_readonly("value", &RateQuoteCurvatureResult_::Value)
        .def_property_readonly("gradient", [](const RateQuoteCurvatureResult_& value) { return CopyRiskVector(value.Gradient()); })
        .def_property_readonly("point", [](const RateQuoteCurvatureResult_& value) { return CopyRiskVector(value.Point()); })
        .def_property_readonly("directions", [](const RateQuoteCurvatureResult_& value) { return Matrix_<>(value.Directions()); })
        .def_property_readonly("steps", [](const RateQuoteCurvatureResult_& value) { return CopyRiskVector(value.Steps()); })
        .def_property_readonly("hessian_products", [](const RateQuoteCurvatureResult_& value) { return Matrix_<>(value.HessianProducts()); })
        .def_property_readonly("base_calibration", [](const RateQuoteCurvatureResult_& value) { return value.BaseCalibration(); })
        .def_property_readonly("execution", [](const RateQuoteCurvatureResult_& value) { return value.Execution(); });

    WithCopies(py::class_<RateTradeQuoteCurvatureResult_>(m, "RateTradeQuoteCurvatureResult_"))
        .def_property_readonly("currency", [](const RateTradeQuoteCurvatureResult_& value) { return std::string(value.Currency().String()); })
        .def_property_readonly("curvature", [](const RateTradeQuoteCurvatureResult_& value) { return value.Curvature(); });
    m.def("RateTradeQuoteCurvature", &NewTradeCurvature, py::arg("trades"), py::arg("calibration"), py::arg("bumps"), py::kw_only(),
          py::arg("settings") = py::none());
}

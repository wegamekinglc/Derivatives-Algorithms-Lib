//
// Created by Codex on 2026/10/10.
//

#include "bindings.h"

#include <chrono>
#include <cmath>
#include <condition_variable>
#include <limits>
#include <mutex>

#include <pybind11/stl.h>

#include <dal-public/src/europeanpderisk.hpp>

#include "riskrequest.hpp"

using namespace Dal;
using namespace Dal::Python;

namespace {
    constexpr const char* IDENTIFIER = "InvalidEuropeanPdeRiskRequest";
    thread_local bool testBarrierArmed = false;
    std::mutex testBarrierMutex;
    std::condition_variable testBarrierCondition;
    bool testBarrierEntered = false;
    bool testBarrierReleased = false;

    void WaitForGilTestRelease() {
        if (!testBarrierArmed)
            return;
        testBarrierArmed = false;
        std::unique_lock<std::mutex> lock(testBarrierMutex);
        testBarrierEntered = true;
        REQUIRE(testBarrierCondition.wait_for(lock, std::chrono::seconds(5), [] { return testBarrierReleased; }),
                "EuropeanPdeRisk: Python GIL test barrier timed out");
    }

    double RealInput(const py::handle& value, const char* field) {
        const auto context = InputContext(value, std::string("EuropeanPdeRisk; ") + field, "finite int or float, excluding bool", IDENTIFIER);
        if (PyBool_Check(value.ptr()) || IsEnum(value) || (!PyLong_Check(value.ptr()) && !PyFloat_Check(value.ptr())))
            throw py::type_error(context);
        const double result = PyFloat_AsDouble(value.ptr());
        if (PyErr_Occurred()) {
            PyErr_Clear();
            THROW(String_(context));
        }
        REQUIRE(std::isfinite(result), String_(context));
        return result;
    }

    int IntInput(const py::handle& value, const char* field) {
        const auto context = InputContext(value, std::string("EuropeanPdeSettings; ") + field, "representable int, excluding bool", IDENTIFIER);
        return static_cast<int>(
            IntegerInput(value, context, std::numeric_limits<int>::min(), std::numeric_limits<int>::max(), "; integer exceeds int range"));
    }

    EuropeanPdeSettings_ NewSettings(const py::object& nodes,
                                     const py::object& intervals,
                                     const py::object& upper,
                                     const py::object& spotIndex,
                                     const py::object& expiry,
                                     const py::object& dividend,
                                     const py::object& forwardLimit,
                                     const py::object& transposeLimit) {
        EuropeanPdeSettings_ settings;
        settings.gridPoints_ = IntInput(nodes, "grid_points");
        settings.ordinarySteps_ = IntInput(intervals, "ordinary_steps");
        settings.upper_ = RealInput(upper, "upper");
        settings.spotIndex_ = spotIndex.is_none() ? std::nullopt : std::optional<int>(IntInput(spotIndex, "spot_index"));
        settings.expiry_ = RealInput(expiry, "expiry");
        settings.dividendYield_ = RealInput(dividend, "dividend_yield");
        settings.accuracy_ = {RealInput(forwardLimit, "forward_backward_error_limit"), RealInput(transposeLimit, "transpose_backward_error_limit")};
        static_cast<void>(AAD::ResolveEuropeanThetaSettings(settings));
        return settings;
    }

    EuropeanPdeRiskResult_ NewRisk(const py::object& rate,
                                   const py::object& volatility,
                                   const py::object& strike,
                                   const py::object& settings,
                                   const py::object& payloadBudget,
                                   const py::object& tapeBudget) {
        EuropeanPdeRiskRequest_ request;
        request.point_ = {RealInput(rate, "rate"), RealInput(volatility, "volatility"), RealInput(strike, "strike")};
        request.settings_ = SettingsInput<EuropeanPdeSettings_>(settings, "EuropeanPdeRiskResult_New; settings", "EuropeanPdeSettings_");
        request.numericPayloadBudgetBytes_ = RequestBudget(payloadBudget, {"EuropeanPdeRisk; numeric_payload_budget_bytes", IDENTIFIER});
        request.recordingCapacityBudgetBytes_ = RequestBudget(tapeBudget, {"EuropeanPdeRisk; recording_capacity_budget_bytes", IDENTIFIER});
        py::gil_scoped_release release;
        WaitForGilTestRelease();
        return EvaluateEuropeanPdeRisk(request);
    }
} // namespace

void init_bindings_europeanpderisk(py::module_& m) {
    WithCopies(py::class_<EuropeanPdeSettings_>(m, "EuropeanPdeSettings_"))
        .def(py::init(&NewSettings), py::kw_only(), py::arg("grid_points") = 61, py::arg("ordinary_steps") = 120, py::arg("upper") = 400.0,
             py::arg("spot_index") = py::none(), py::arg("expiry") = 1.0, py::arg("dividend_yield") = 0.02,
             py::arg("forward_backward_error_limit") = 1e-12, py::arg("transpose_backward_error_limit") = 1e-12)
        .def_readonly("grid_points", &EuropeanPdeSettings_::gridPoints_)
        .def_readonly("ordinary_steps", &EuropeanPdeSettings_::ordinarySteps_)
        .def_readonly("upper", &EuropeanPdeSettings_::upper_)
        .def_readonly("spot_index", &EuropeanPdeSettings_::spotIndex_)
        .def_readonly("expiry", &EuropeanPdeSettings_::expiry_)
        .def_readonly("dividend_yield", &EuropeanPdeSettings_::dividendYield_)
        .def_property_readonly("forward_backward_error_limit",
                               [](const EuropeanPdeSettings_& value) { return value.accuracy_.forwardBackwardErrorLimit_; })
        .def_property_readonly("transpose_backward_error_limit",
                               [](const EuropeanPdeSettings_& value) { return value.accuracy_.transposeBackwardErrorLimit_; });

    WithCopies(py::class_<EuropeanPdeRiskExecution_>(m, "EuropeanPdeRiskExecution_"))
        .def_readonly("actual_steps", &EuropeanPdeRiskExecution_::actualSteps_)
        .def_readonly("numeric_payload_bytes", &EuropeanPdeRiskExecution_::numericPayloadBytes_)
        .def_readonly("peak_tape_bytes", &EuropeanPdeRiskExecution_::peakTapeBytes_)
        .def_readonly("cleanup_reserve_bytes", &EuropeanPdeRiskExecution_::cleanupReserveBytes_)
        .def_readonly("reverse_scratch_peak_bytes", &EuropeanPdeRiskExecution_::reverseScratchPeakBytes_);

    WithCopies(py::class_<EuropeanPdeRiskResult_>(m, "EuropeanPdeRiskResult_"))
        .def_property_readonly("point", [](const EuropeanPdeRiskResult_& value) { return value.request_.point_; })
        .def_property_readonly("settings", [](const EuropeanPdeRiskResult_& value) { return value.request_.settings_; })
        .def_property_readonly("prices", [](const EuropeanPdeRiskResult_& value) { return value.prices_; })
        .def_property_readonly("jacobian", [](const EuropeanPdeRiskResult_& value) { return Matrix_<>(value.jacobian_); })
        .def_property_readonly("grid", [](const EuropeanPdeRiskResult_& value) { return CopyRiskVector(value.grid_); })
        .def_readonly("spot", &EuropeanPdeRiskResult_::spot_)
        .def_property_readonly("method", [](const EuropeanPdeRiskResult_& value) { return Text(value.method_); })
        .def_property_readonly("payoff_labels", [](const EuropeanPdeRiskResult_& value) { return RiskTextVector(value.payoffLabels_); })
        .def_property_readonly("parameter_labels", [](const EuropeanPdeRiskResult_& value) { return RiskTextVector(value.parameterLabels_); })
        .def_property_readonly("parameter_units", [](const EuropeanPdeRiskResult_& value) { return RiskTextVector(value.parameterUnits_); })
        .def_property_readonly("transpose_error_labels",
                               [](const EuropeanPdeRiskResult_& value) { return RiskTextVector(value.transposeErrorLabels_); })
        .def_property_readonly("forward_backward_errors", [](const EuropeanPdeRiskResult_& value) { return Matrix_<>(value.forwardBackwardErrors_); })
        .def_property_readonly("transpose_backward_errors",
                               [](const EuropeanPdeRiskResult_& value) { return Matrix_<>(value.transposeBackwardErrors_); })
        .def_property_readonly("execution", [](const EuropeanPdeRiskResult_& value) { return value.execution_; })
        .def_property_readonly("numeric_payload_budget_bytes",
                               [](const EuropeanPdeRiskResult_& value) { return value.request_.numericPayloadBudgetBytes_; })
        .def_property_readonly("recording_capacity_budget_bytes",
                               [](const EuropeanPdeRiskResult_& value) { return value.request_.recordingCapacityBudgetBytes_; });
    m.def("EuropeanPdeRiskResult_New", &NewRisk, py::arg("rate"), py::arg("volatility"), py::arg("strike"), py::kw_only(),
          py::arg("settings") = py::none(), py::arg("numeric_payload_budget_bytes") = py::none(),
          py::arg("recording_capacity_budget_bytes") = py::none());

    m.def("_EuropeanPdeRiskGilBarrier_ArmForTesting", [] {
        std::lock_guard<std::mutex> lock(testBarrierMutex);
        testBarrierEntered = testBarrierReleased = false;
        testBarrierArmed = true;
    });
    m.def("_EuropeanPdeRiskGilBarrier_ReleaseForTesting", [] {
        std::lock_guard<std::mutex> lock(testBarrierMutex);
        if (!testBarrierEntered)
            return false;
        testBarrierReleased = true;
        testBarrierCondition.notify_all();
        return true;
    });
}

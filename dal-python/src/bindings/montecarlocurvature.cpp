//
// Created by Codex on 2026/10/10.
//

#include "bindings.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>

#include <pybind11/stl.h>

#include <dal-public/src/montecarlocurvature.hpp>

#include "riskrequest.hpp"

using namespace Dal;
using namespace Dal::Python;
using Script::SegmentedMonteCarloSettings_;

namespace {
    constexpr const char* IDENTIFIER = "InvalidSegmentedMonteCarloRequest";

    Vector_<> Numbers(const py::handle& value, const char* field) {
        static_cast<void>(RequestSequence(value, {field, IDENTIFIER}));
        auto result = *RequestNumbers(value, {field, IDENTIFIER}, field);
        REQUIRE(std::all_of(result.begin(), result.end(), [](double number) { return std::isfinite(number); }),
                String_(std::string(IDENTIFIER) + ": " + field + "; values must be finite"));
        return result;
    }

    std::optional<std::uint64_t> ScrambleKey(const py::handle& value) {
        if (value.is_none())
            return std::nullopt;
        const auto context = InputContext(value, "SegmentedMonteCarloSettings_; scramble_key", "uint64 integer or None, excluding bool", IDENTIFIER);
        if (PyBool_Check(value.ptr()) || IsEnum(value) || !PyIndex_Check(value.ptr()))
            throw py::type_error(context);
        const auto integer = py::reinterpret_steal<py::object>(PyNumber_Index(value.ptr()));
        if (!integer)
            throw py::error_already_set();
        const auto result = PyLong_AsUnsignedLongLong(integer.ptr());
        if (PyErr_Occurred()) {
            PyErr_Clear();
            THROW2(String_(context), ScriptError_);
        }
        return static_cast<std::uint64_t>(result);
    }

    SegmentedMonteCarloSettings_ NewSettings(const py::object& rsg,
                                             const py::object& bridge,
                                             const py::object& firstPath,
                                             const py::object& key,
                                             const py::object& precision,
                                             const py::object& segmentSteps,
                                             const py::object& checkpoint,
                                             const py::object& recording) {
        SegmentedMonteCarloSettings_ settings;
        settings.rsg_ = SettingStringInput(rsg, "SegmentedMonteCarloSettings_; rsg", IDENTIFIER);
        if (!PyBool_Check(bridge.ptr()))
            throw py::type_error(InputContext(bridge, "SegmentedMonteCarloSettings_; use_bb", "bool", IDENTIFIER));
        settings.useBb_ = bridge.ptr() == Py_True;
        settings.firstPath_ = RequestUnsigned(firstPath, {"SegmentedMonteCarloSettings_; first_path", IDENTIFIER});
        settings.scrambleKey_ = ScrambleKey(key);
        settings.normalPrecision_ = SettingStringInput(precision, "SegmentedMonteCarloSettings_; normal_precision", IDENTIFIER);
        settings.path_.segmentSteps_ = RequestUnsigned(segmentSteps, {"SegmentedMonteCarloSettings_; segment_steps", IDENTIFIER});
        REQUIRE2(settings.path_.segmentSteps_ > 0, "InvalidSegmentedMonteCarloRequest: segment_steps must be positive", ScriptError_);
        settings.path_.checkpointCapacityBudgetBytes_ =
            RequestBudget(checkpoint, {"SegmentedMonteCarloSettings_; checkpoint_capacity_budget_bytes", IDENTIFIER});
        settings.path_.recordingCapacityBudgetBytes_ =
            RequestBudget(recording, {"SegmentedMonteCarloSettings_; recording_capacity_budget_bytes", IDENTIFIER});
        return settings;
    }

    BlackScholesMonteCarloPlan_ NewPlan(const py::object& product, const py::object& valuation, const py::object& smoothing) {
        if (!py::isinstance<Script::ScriptProductData_>(product))
            throw py::type_error(InputContext(product, "BlackScholesMonteCarloPlan_New; product", "ScriptProductData_", IDENTIFIER));
        const Handle_<Script::ScriptProductData_> contract(py::cast<std::shared_ptr<Script::ScriptProductData_>>(product));
        const auto settings =
            SettingsInput<Script::ScriptValuationSettings_>(valuation, "BlackScholesMonteCarloPlan_New; valuation", "ScriptValuationSettings_");
        const double width = Numbers(py::make_tuple(smoothing), "BlackScholesMonteCarloPlan_New; smoothing")[0];
        py::gil_scoped_release release;
        return PlanBlackScholesMonteCarlo(contract, settings, width);
    }

    struct Inputs_ {
        BlackScholesMonteCarloPlan_ plan_;
        Vector_<> point_;
        size_t paths_;
        SegmentedMonteCarloSettings_ settings_;
    };

    Inputs_ Inputs(const py::object& plan, const py::object& point, const py::object& paths, const py::object& settings, const char* function) {
        const std::string field(function);
        return {RequiredRiskInput<BlackScholesMonteCarloPlan_>(plan, field + "; plan", "BlackScholesMonteCarloPlan_", IDENTIFIER),
                Numbers(point, (field + "; point").c_str()), static_cast<size_t>(PathCount(paths, function)),
                settings.is_none()
                    ? SegmentedMonteCarloSettings_()
                    : RequiredRiskInput<SegmentedMonteCarloSettings_>(settings, field + "; settings", "SegmentedMonteCarloSettings_", IDENTIFIER)};
    }

    BlackScholesMonteCarloResult_ NewRisk(const py::object& plan, const py::object& point, const py::object& paths, const py::object& settings) {
        const auto inputs = Inputs(plan, point, paths, settings, "BlackScholesMonteCarlo_Get_Risk");
        py::gil_scoped_release release;
        return ValueByBlackScholesSegmentedMonteCarlo(inputs.plan_, inputs.point_, inputs.paths_, inputs.settings_);
    }

    BlackScholesMonteCarloCurvatureResult_
    NewCurvature(const py::object& plan, const py::object& point, const py::object& paths, const py::object& bumps, const py::object& settings) {
        const auto inputs = Inputs(plan, point, paths, settings, "BlackScholesMonteCarlo_Get_Curvature");
        const auto request =
            RequiredRiskInput<AAD::BumpOverAADRequest_>(bumps, "BlackScholesMonteCarlo_Get_Curvature; bumps", "BumpOverAADRequest_", IDENTIFIER);
        py::gil_scoped_release release;
        return ValueByBlackScholesMonteCarloWithCurvature(inputs.plan_, inputs.point_, inputs.paths_, request, inputs.settings_);
    }

    template <class T_, class F_> void MeanProperties(py::class_<T_> cls, F_ mean) {
        cls.def_property_readonly("value", [mean](const T_& value) { return mean(value).MeanValue(); })
            .def_property_readonly("gradient", [mean](const T_& value) { return CopyRiskVector(mean(value).MeanGradient()); })
            .def_property_readonly("parameter_labels", [mean](const T_& value) { return RiskTextVector(mean(value).ParameterLabels()); })
            .def_property_readonly("execution", [mean](const T_& value) { return mean(value).Execution(); });
    }
} // namespace

void init_bindings_montecarlocurvature(py::module_& m) {
    WithCopies(py::class_<SegmentedMonteCarloSettings_>(m, "SegmentedMonteCarloSettings_"))
        .def(py::init(&NewSettings), py::kw_only(), py::arg("rsg") = "sobol", py::arg("use_bb") = false, py::arg("first_path") = 0,
             py::arg("scramble_key") = py::none(), py::arg("normal_precision") = "Default", py::arg("segment_steps") = 64,
             py::arg("checkpoint_capacity_budget_bytes") = py::none(), py::arg("recording_capacity_budget_bytes") = py::none())
        .def_property_readonly("rsg", [](const SegmentedMonteCarloSettings_& value) { return Text(value.rsg_); })
        .def_readonly("use_bb", &SegmentedMonteCarloSettings_::useBb_)
        .def_readonly("first_path", &SegmentedMonteCarloSettings_::firstPath_)
        .def_readonly("scramble_key", &SegmentedMonteCarloSettings_::scrambleKey_)
        .def_property_readonly("normal_precision", [](const SegmentedMonteCarloSettings_& value) { return Text(value.normalPrecision_); })
        .def_property_readonly("segment_steps", [](const SegmentedMonteCarloSettings_& value) { return value.path_.segmentSteps_; })
        .def_property_readonly("checkpoint_capacity_budget_bytes",
                               [](const SegmentedMonteCarloSettings_& value) { return value.path_.checkpointCapacityBudgetBytes_; })
        .def_property_readonly("recording_capacity_budget_bytes",
                               [](const SegmentedMonteCarloSettings_& value) { return value.path_.recordingCapacityBudgetBytes_; });

    WithCopies(py::class_<BlackScholesMonteCarloPlan_>(m, "BlackScholesMonteCarloPlan_"))
        .def_property_readonly("parameter_labels", [](const BlackScholesMonteCarloPlan_& value) { return RiskTextVector(value.ParameterLabels()); })
        .def_property_readonly("script_constants", [](const BlackScholesMonteCarloPlan_& value) { return CopyRiskVector(value.ScriptConstants()); })
        .def_property_readonly("valuation", [](const BlackScholesMonteCarloPlan_& value) { return value.Valuation(); })
        .def_property_readonly("smoothing", [](const BlackScholesMonteCarloPlan_& value) { return value.Prepared().Simulation().smooth_; })
        .def_property_readonly("contract_dates", [](const BlackScholesMonteCarloPlan_& value) { return CopyRiskVector(value.Contract().Dates()); })
        .def_property_readonly("contract_events",
                               [](const BlackScholesMonteCarloPlan_& value) { return RiskTextVector(value.Contract().EventTexts()); })
        .def_property_readonly("contract_settings", [](const BlackScholesMonteCarloPlan_& value) { return value.Contract().Settings(); })
        .def_property_readonly("observations", [](const BlackScholesMonteCarloPlan_& value) { return CopyRiskVector(value.Observations()); });
    m.def("BlackScholesMonteCarloPlan_New", &NewPlan, py::arg("product"), py::kw_only(), py::arg("valuation") = py::none(),
          py::arg("smoothing") = Script::DEFAULT_SMOOTH);

    using Script::SegmentedMonteCarloExecution_;
    WithCopies(py::class_<SegmentedMonteCarloExecution_>(m, "SegmentedMonteCarloExecution_"))
        .def_readonly("path_count", &SegmentedMonteCarloExecution_::pathCount_)
        .def_readonly("first_path", &SegmentedMonteCarloExecution_::firstPath_)
        .def_readonly("batch_size", &SegmentedMonteCarloExecution_::batchSize_)
        .def_readonly("batches", &SegmentedMonteCarloExecution_::batches_)
        .def_readonly("lanes", &SegmentedMonteCarloExecution_::lanes_)
        .def_readonly("segment_steps", &SegmentedMonteCarloExecution_::segmentSteps_)
        .def_property_readonly("rsg", [](const SegmentedMonteCarloExecution_& value) { return Text(value.rsg_); })
        .def_readonly("use_bb", &SegmentedMonteCarloExecution_::useBb_)
        .def_readonly("scramble_key", &SegmentedMonteCarloExecution_::scrambleKey_)
        .def_property_readonly("normal_precision", [](const SegmentedMonteCarloExecution_& value) { return Text(value.normalPrecision_); })
        .def_readonly("max_path_tape_bytes", &SegmentedMonteCarloExecution_::maxPathTapeBytes_)
        .def_readonly("max_path_checkpoint_bytes", &SegmentedMonteCarloExecution_::maxPathCheckpointBytes_)
        .def_readonly("max_path_cleanup_reserve_bytes", &SegmentedMonteCarloExecution_::maxPathCleanupReserveBytes_);
    MeanProperties(WithCopies(py::class_<Script::SegmentedMonteCarloResult_>(m, "SegmentedMonteCarloResult_")),
                   [](const Script::SegmentedMonteCarloResult_& value) -> const auto& { return value; });
    auto mean = WithCopies(py::class_<BlackScholesMonteCarloResult_>(m, "BlackScholesMonteCarloResult_"));
    MeanProperties(mean, [](const BlackScholesMonteCarloResult_& value) -> const auto& { return value.Mean(); });
    mean.def_property_readonly("plan", [](const BlackScholesMonteCarloResult_& value) { return value.Plan(); })
        .def_property_readonly("point", [](const BlackScholesMonteCarloResult_& value) { return CopyRiskVector(value.Point()); })
        .def_property_readonly("settings", [](const BlackScholesMonteCarloResult_& value) { return value.Settings(); });
    m.def("BlackScholesMonteCarlo_Get_Risk", &NewRisk, py::arg("plan"), py::arg("point"), py::arg("num_path"), py::kw_only(),
          py::arg("settings") = py::none());

    using Script::MonteCarloCurvatureExecution_;
    WithCopies(py::class_<MonteCarloCurvatureExecution_>(m, "MonteCarloCurvatureExecution_"))
        .def_property_readonly("method", [](const MonteCarloCurvatureExecution_& value) { return Text(value.method_); })
        .def_readonly("gradient_evaluations", &MonteCarloCurvatureExecution_::gradientEvaluations_)
        .def_readonly("numeric_payload_bytes", &MonteCarloCurvatureExecution_::numericPayloadBytes_)
        .def_readonly("max_path_tape_bytes", &MonteCarloCurvatureExecution_::maxPathTapeBytes_)
        .def_readonly("max_path_checkpoint_bytes", &MonteCarloCurvatureExecution_::maxPathCheckpointBytes_)
        .def_readonly("max_path_cleanup_reserve_bytes", &MonteCarloCurvatureExecution_::maxPathCleanupReserveBytes_)
        .def_readonly("recording_capacity_budget_bytes", &MonteCarloCurvatureExecution_::recordingCapacityBudgetBytes_);
    WithCopies(py::class_<BlackScholesMonteCarloCurvatureResult_>(m, "BlackScholesMonteCarloCurvatureResult_"))
        .def_property_readonly("plan", [](const BlackScholesMonteCarloCurvatureResult_& value) { return value.Plan(); })
        .def_property_readonly("base", [](const BlackScholesMonteCarloCurvatureResult_& value) { return value.Curvature().Base(); })
        .def_property_readonly("point", [](const BlackScholesMonteCarloCurvatureResult_& value) { return CopyRiskVector(value.Curvature().Point()); })
        .def_property_readonly("directions",
                               [](const BlackScholesMonteCarloCurvatureResult_& value) { return Matrix_<>(value.Curvature().Directions()); })
        .def_property_readonly("steps", [](const BlackScholesMonteCarloCurvatureResult_& value) { return CopyRiskVector(value.Curvature().Steps()); })
        .def_property_readonly("hessian_products",
                               [](const BlackScholesMonteCarloCurvatureResult_& value) { return Matrix_<>(value.Curvature().HessianProducts()); })
        .def_property_readonly("settings", [](const BlackScholesMonteCarloCurvatureResult_& value) { return value.Curvature().Settings(); })
        .def_property_readonly("execution", [](const BlackScholesMonteCarloCurvatureResult_& value) { return value.Curvature().Execution(); });
    m.def("BlackScholesMonteCarlo_Get_Curvature", &NewCurvature, py::arg("plan"), py::arg("point"), py::arg("num_path"), py::arg("bumps"),
          py::kw_only(), py::arg("settings") = py::none());
}

//
// Created by Codex on 2026/10/10.
//

#include "bindings.h"

#include <array>
#include <string>
#include <vector>

#include <pybind11/stl.h>

#include <dal-public/src/lsmccurvature.hpp>

#include "riskrequest.hpp"

using namespace Dal;
using namespace Dal::Python;

namespace {
    constexpr const char* IDENTIFIER = "InvalidLsmcCurvatureRequest";

    BlackScholesLsmcPlan_ NewPlan(const py::object& product, const py::object& valuation, const py::object& simulation) {
        if (!py::isinstance<Script::ScriptProductData_>(product))
            throw py::type_error(InputContext(product, "BlackScholesLsmcPlan_New; product", "ScriptProductData_", IDENTIFIER));
        const Handle_<Script::ScriptProductData_> contract(py::cast<std::shared_ptr<Script::ScriptProductData_>>(product));
        const auto settings =
            SettingsInput<Script::ScriptValuationSettings_>(valuation, "BlackScholesLsmcPlan_New; valuation", "ScriptValuationSettings_");
        const auto execution = simulation.is_none() ? DefaultRiskMonteCarloSettings()
                                                    : SettingsInput<Script::MonteCarloSettings_>(simulation, "BlackScholesLsmcPlan_New; simulation",
                                                                                                 "MonteCarloSettings_");
        py::gil_scoped_release release;
        return PlanBlackScholesLsmc(contract, execution, settings);
    }

    BlackScholesLsmcCurvatureResult_ NewCurvature(const py::object& plan, const py::object& point, const py::object& paths, const py::object& bumps) {
        const auto fixedPlan =
            RequiredRiskInput<BlackScholesLsmcPlan_>(plan, "BlackScholesLsmc_Get_Curvature; plan", "BlackScholesLsmcPlan_", IDENTIFIER);
        const RiskRequestContext_ context{"BlackScholesLsmc_Get_Curvature; point", IDENTIFIER};
        static_cast<void>(RequestSequence(point, context));
        const auto fixedPoint = *RequestNumbers(point, context, context.field_);
        const auto count = static_cast<size_t>(PathCount(paths, "BlackScholesLsmc_Get_Curvature"));
        const auto request =
            RequiredRiskInput<AAD::BumpOverAADRequest_>(bumps, "BlackScholesLsmc_Get_Curvature; bumps", "BumpOverAADRequest_", IDENTIFIER);
        py::gil_scoped_release release;
        return ValueByBlackScholesLsmcWithCurvature(fixedPlan, fixedPoint, count, request);
    }

    std::vector<std::array<int, 3>> Powers(const Script::ExerciseRegression_& value) {
        std::vector<std::array<int, 3>> result;
        result.reserve(value.powers_.size());
        for (const auto& row : value.powers_)
            result.push_back({row[0], row[1], row[2]});
        return result;
    }
} // namespace

void init_bindings_lsmccurvature(py::module_& m) {
    WithCopies(py::class_<BlackScholesLsmcPlan_>(m, "BlackScholesLsmcPlan_"))
        .def_property_readonly("parameter_labels", [](const BlackScholesLsmcPlan_& value) { return RiskTextVector(value.ParameterLabels()); })
        .def_property_readonly("script_constants", [](const BlackScholesLsmcPlan_& value) { return CopyRiskVector(value.ScriptConstants()); })
        .def_property_readonly("valuation", [](const BlackScholesLsmcPlan_& value) { return value.Valuation(); })
        .def_property_readonly("simulation", [](const BlackScholesLsmcPlan_& value) { return value.Simulation(); })
        .def_property_readonly("contract_dates", [](const BlackScholesLsmcPlan_& value) { return CopyRiskVector(value.Contract().Dates()); })
        .def_property_readonly("contract_events", [](const BlackScholesLsmcPlan_& value) { return RiskTextVector(value.Contract().EventTexts()); })
        .def_property_readonly("contract_settings", [](const BlackScholesLsmcPlan_& value) { return value.Contract().Settings(); })
        .def_property_readonly("event_dates", [](const BlackScholesLsmcPlan_& value) { return CopyRiskVector(value.Prepared().EventDates()); })
        .def_property_readonly("time_line", [](const BlackScholesLsmcPlan_& value) { return CopyRiskVector(value.Prepared().TimeLine()); })
        .def_property_readonly("observations", [](const BlackScholesLsmcPlan_& value) { return CopyRiskVector(value.Observations()); });
    m.def("BlackScholesLsmcPlan_New", &NewPlan, py::arg("product"), py::kw_only(), py::arg("valuation") = py::none(),
          py::arg("simulation") = py::none());

    using Script::ExerciseRegression_;
    WithCopies(py::class_<ExerciseRegression_>(m, "LsmcExercisePolicy_"))
        .def_property_readonly("coefficients", [](const ExerciseRegression_& value) { return CopyRiskVector(value.coefficients_); })
        .def_property_readonly("normalization_means", [](const ExerciseRegression_& value) { return CopyRiskVector(value.means_); })
        .def_property_readonly("normalization_sigmas", [](const ExerciseRegression_& value) { return CopyRiskVector(value.sigmas_); })
        .def_property_readonly("basis_powers", &Powers)
        .def_readonly("basis_degree", &ExerciseRegression_::basisDegree_)
        .def_readonly("degenerate", &ExerciseRegression_::degenerate_)
        .def_property_readonly("degenerate_reason", [](const ExerciseRegression_& value) { return Text(value.degenerateReason_); })
        .def_readonly("mean", &ExerciseRegression_::mean_)
        .def_readonly("sigma", &ExerciseRegression_::sigma_)
        .def_readonly("condition_path_count", &ExerciseRegression_::numCondTrue_)
        .def_readonly("effective_rank", &ExerciseRegression_::effectiveRank_)
        .def_property_readonly("solver", [](const ExerciseRegression_& value) { return Text(value.solver_); })
        .def_property_readonly("fallback_reason", [](const ExerciseRegression_& value) { return Text(value.fallbackReason_); })
        .def_readonly("validation_mse", &ExerciseRegression_::validationMse_);

    using Script::LsmcCurvatureExecution_;
    WithCopies(py::class_<LsmcCurvatureExecution_>(m, "LsmcCurvatureExecution_"))
        .def_property_readonly("method", [](const LsmcCurvatureExecution_& value) { return Text(value.method_); })
        .def_readonly("gradient_evaluations", &LsmcCurvatureExecution_::gradientEvaluations_)
        .def_readonly("paths_per_replicate", &LsmcCurvatureExecution_::pathsPerReplicate_)
        .def_readonly("training_paths", &LsmcCurvatureExecution_::trainingPaths_)
        .def_readonly("validation_paths", &LsmcCurvatureExecution_::validationPaths_)
        .def_readonly("pricing_replicates", &LsmcCurvatureExecution_::pricingReplicates_)
        .def_readonly("numeric_payload_bytes", &LsmcCurvatureExecution_::numericPayloadBytes_)
        .def_readonly("max_batch_tape_bytes", &LsmcCurvatureExecution_::maxBatchTapeBytes_)
        .def_readonly("max_batch_cleanup_reserve_bytes", &LsmcCurvatureExecution_::maxBatchCleanupReserveBytes_)
        .def_readonly("recording_capacity_budget_bytes", &LsmcCurvatureExecution_::recordingCapacityBudgetBytes_);

    WithCopies(py::class_<BlackScholesLsmcCurvatureResult_>(m, "BlackScholesLsmcCurvatureResult_"))
        .def_property_readonly("plan", [](const BlackScholesLsmcCurvatureResult_& value) { return value.Plan(); })
        .def_property_readonly("value", [](const BlackScholesLsmcCurvatureResult_& value) { return value.Curvature().Value(); })
        .def_property_readonly("gradient", [](const BlackScholesLsmcCurvatureResult_& value) { return CopyRiskVector(value.Curvature().Gradient()); })
        .def_property_readonly("parameter_labels",
                               [](const BlackScholesLsmcCurvatureResult_& value) { return RiskTextVector(value.Plan().ParameterLabels()); })
        .def_property_readonly("point", [](const BlackScholesLsmcCurvatureResult_& value) { return CopyRiskVector(value.Curvature().Point()); })
        .def_property_readonly("directions", [](const BlackScholesLsmcCurvatureResult_& value) { return Matrix_<>(value.Curvature().Directions()); })
        .def_property_readonly("steps", [](const BlackScholesLsmcCurvatureResult_& value) { return CopyRiskVector(value.Curvature().Steps()); })
        .def_property_readonly("hessian_products",
                               [](const BlackScholesLsmcCurvatureResult_& value) { return Matrix_<>(value.Curvature().HessianProducts()); })
        .def_property_readonly("base_policy",
                               [](const BlackScholesLsmcCurvatureResult_& value) { return CopyRiskVector(value.Curvature().BasePolicy()); })
        .def_property_readonly("simulation", [](const BlackScholesLsmcCurvatureResult_& value) { return value.Plan().Simulation(); })
        .def_property_readonly("execution", [](const BlackScholesLsmcCurvatureResult_& value) { return value.Curvature().Execution(); });
    m.def("BlackScholesLsmc_Get_Curvature", &NewCurvature, py::arg("plan"), py::arg("point"), py::arg("num_path"), py::arg("bumps"));
}

#include "bindings.h"

#include <cmath>
#include <limits>
#include <pybind11/stl.h>

#include <dal-public/src/earlyexercisepderisk.hpp>

#include "riskrequest.hpp"

using namespace Dal;
using namespace Dal::Python;

namespace {
    constexpr const char* IDENTIFIER = "InvalidEarlyExercisePdeRiskRequest";

    template <class F_> auto Input(F_ read) {
        try {
            return read();
        } catch (const Exception_& error) {
            throw py::value_error(error.what());
        }
    }
    py::dict Arguments(const py::args& args, const py::kwargs& kwargs, py::dict defaults, const char* required = nullptr) {
        if (args.size() > (required ? 1u : 0u))
            throw py::type_error(std::string(IDENTIFIER) + "; " + (required ? required : "settings") +
                                 "; unexpected positional arguments; settings are keyword-only");
        bool supplied = false;
        if (required && !args.empty()) {
            defaults[required] = args[0];
            supplied = true;
        }
        for (const auto& item : kwargs) {
            const auto key = py::cast<std::string>(item.first);
            if (!defaults.contains(item.first) || (supplied && key == required))
                throw py::type_error(std::string(IDENTIFIER) + "; " + key + "; unknown or duplicate keyword");
            defaults[item.first] = item.second;
        }
        if (required && defaults[required].is_none())
            throw py::type_error(std::string(IDENTIFIER) + "; " + required + "; required typed argument");
        return defaults;
    }
    double Real(const py::handle& value, const std::string& field) {
        const auto context = InputContext(value, field, "finite int or float excluding bool and enums", IDENTIFIER);
        if (PyBool_Check(value.ptr()) || IsEnum(value) || (!PyLong_Check(value.ptr()) && !PyFloat_Check(value.ptr())))
            throw py::type_error(context);
        const double result = PyFloat_AsDouble(value.ptr());
        if (PyErr_Occurred()) {
            PyErr_Clear();
            throw py::value_error(context);
        }
        if (!std::isfinite(result))
            throw py::value_error(context);
        return result;
    }
    bool Boolean(const py::handle& value, const char* field) {
        if (!PyBool_Check(value.ptr()))
            throw py::type_error(InputContext(value, field, "bool", IDENTIFIER));
        return py::cast<bool>(value);
    }
    size_t Count(const py::handle& value, const char* field) {
        if (!PyLong_Check(value.ptr()) || PyBool_Check(value.ptr()) || IsEnum(value))
            throw py::type_error(InputContext(value, field, "nonnegative size_t int excluding bool and enums", IDENTIFIER));
        return RequestUnsigned(value, {field, IDENTIFIER});
    }
    Vector_<double> Numbers(const py::handle& value, const char* field) {
        Vector_<double> result;
        const auto sequence = RequestSequence(value, {field, IDENTIFIER});
        result.reserve(sequence.size());
        for (size_t i = 0; i < sequence.size(); ++i)
            result.push_back(Real(sequence[i], std::string(field) + "[" + std::to_string(i) + "]"));
        return result;
    }
    template <class T_, class = void> struct IsDalEnum_ : std::false_type {};
    template <class T_> struct IsDalEnum_<T_, std::void_t<decltype(std::declval<T_>().String())>> : std::true_type {};
    template <class T_> py::object Detached(const T_& value);
    template <class T_> py::object Detached(const std::optional<T_>& value);
    template <class T_> py::object Detached(const Vector_<T_>& value);
    template <class T_> py::object Detached(const T_& value) {
        if constexpr (std::is_same_v<T_, String_>)
            return py::str(Text(value));
        else if constexpr (IsDalEnum_<T_>::value)
            return py::str(Text(value.String()));
        else
            return py::cast(value, py::return_value_policy::copy);
    }
    template <class T_> py::object Detached(const std::optional<T_>& value) { return value ? Detached(*value) : py::none(); }
    template <class T_> py::object Detached(const Vector_<T_>& value) {
        py::list result;
        for (const auto& item : value)
            result.append(Detached(item));
        return result;
    }
    template <class T_, class V_> void ReadOnly(py::class_<T_>* type, const char* name, V_ T_::* member) {
        type->def_property_readonly(name, [member](const T_& value) { return Detached(value.*member); });
    }
    EarlyExercisePdeRiskSettings_ NewSettings(const py::args& args, const py::kwargs& kwargs) {
        py::dict defaults;
        defaults["delta"] = py::cast(true);
        defaults["gamma"] = py::cast(true);
        defaults["vega"] = py::cast(true);
        defaults["carry_risk"] = py::cast(true);
        defaults["spot_bump"] = py::none();
        defaults["volatility_bump"] = py::cast(1e-3);
        defaults["rate_bumps"] = py::tuple();
        defaults["dividend_yield_bumps"] = py::tuple();
        defaults["delta_stability_tolerance"] = py::cast(2e-3);
        defaults["gamma_stability_tolerance"] = py::cast(2e-2);
        defaults["vega_stability_tolerance"] = py::cast(3e-3);
        defaults["rate_stability_tolerance"] = py::cast(3e-3);
        defaults["dividend_yield_stability_tolerance"] = py::cast(3e-3);
        defaults["max_scenarios"] = py::cast(129);
        defaults["max_work_units"] = py::cast(100000000);
        defaults["numeric_payload_budget_bytes"] = py::cast(67108864);
        defaults["workspace_budget_bytes"] = py::cast(268435456);
        const auto values = Arguments(args, kwargs, defaults);
        return Input([&] {
            EarlyExercisePdeRiskSettings_ s;
            s.delta_ = Boolean(values["delta"], "risk.delta");
            s.gamma_ = Boolean(values["gamma"], "risk.gamma");
            s.vega_ = Boolean(values["vega"], "risk.vega");
            s.carryRisk_ = Boolean(values["carry_risk"], "risk.carry_risk");
            s.spotBump_ = values["spot_bump"].is_none() ? std::nullopt : std::optional<double>(Real(values["spot_bump"], "risk.spot_bump"));
            s.volatilityBump_ = Real(values["volatility_bump"], "risk.volatility_bump");
            s.rateBumps_ = Numbers(values["rate_bumps"], "risk.rate_bumps");
            s.dividendYieldBumps_ = Numbers(values["dividend_yield_bumps"], "risk.dividend_yield_bumps");
            s.deltaStabilityTolerance_ = Real(values["delta_stability_tolerance"], "risk.delta_stability_tolerance");
            s.gammaStabilityTolerance_ = Real(values["gamma_stability_tolerance"], "risk.gamma_stability_tolerance");
            s.vegaStabilityTolerance_ = Real(values["vega_stability_tolerance"], "risk.vega_stability_tolerance");
            s.rateStabilityTolerance_ = Real(values["rate_stability_tolerance"], "risk.rate_stability_tolerance");
            s.dividendYieldStabilityTolerance_ = Real(values["dividend_yield_stability_tolerance"], "risk.dividend_yield_stability_tolerance");
            s.maxScenarios_ = Count(values["max_scenarios"], "risk.max_scenarios");
            s.maxWorkUnits_ = Count(values["max_work_units"], "risk.max_work_units");
            s.numericPayloadBudgetBytes_ = Count(values["numeric_payload_budget_bytes"], "risk.numeric_payload_budget_bytes");
            s.workspaceBudgetBytes_ = Count(values["workspace_budget_bytes"], "risk.workspace_budget_bytes");
            ValidateEarlyExercisePdeRiskSettings(s);
            return s;
        });
    }
} // namespace

void init_bindings_earlyexercisepderisk(py::module_& m) {
    py::enum_<EarlyExercisePdeRiskStatus_::Value_>(m, "EarlyExercisePdeRiskStatus_")
        .value("COMPLETE", EarlyExercisePdeRiskStatus_::Value_::COMPLETE)
        .value("PARTIAL", EarlyExercisePdeRiskStatus_::Value_::PARTIAL)
        .value("BASE_FAILED", EarlyExercisePdeRiskStatus_::Value_::BASE_FAILED)
        .value("WORK_LIMIT", EarlyExercisePdeRiskStatus_::Value_::WORK_LIMIT);
    py::enum_<EarlyExercisePdeRiskAvailability_::Value_>(m, "EarlyExercisePdeRiskAvailability_")
        .value("AVAILABLE", EarlyExercisePdeRiskAvailability_::Value_::AVAILABLE)
        .value("AVAILABLE_EXERCISE_REGION", EarlyExercisePdeRiskAvailability_::Value_::AVAILABLE_EXERCISE_REGION)
        .value("UNAVAILABLE_BASE_FAILED", EarlyExercisePdeRiskAvailability_::Value_::UNAVAILABLE_BASE_FAILED)
        .value("UNAVAILABLE_WORK_LIMIT", EarlyExercisePdeRiskAvailability_::Value_::UNAVAILABLE_WORK_LIMIT)
        .value("UNAVAILABLE_SCENARIO_FAILED", EarlyExercisePdeRiskAvailability_::Value_::UNAVAILABLE_SCENARIO_FAILED)
        .value("UNAVAILABLE_DEGENERATE_DIFFUSION", EarlyExercisePdeRiskAvailability_::Value_::UNAVAILABLE_DEGENERATE_DIFFUSION)
        .value("UNAVAILABLE_DOMAIN_BOUNDARY", EarlyExercisePdeRiskAvailability_::Value_::UNAVAILABLE_DOMAIN_BOUNDARY)
        .value("UNAVAILABLE_NEAR_EXERCISE_BOUNDARY", EarlyExercisePdeRiskAvailability_::Value_::UNAVAILABLE_NEAR_EXERCISE_BOUNDARY)
        .value("UNAVAILABLE_EXERCISE_SWITCH", EarlyExercisePdeRiskAvailability_::Value_::UNAVAILABLE_EXERCISE_SWITCH)
        .value("UNAVAILABLE_NONFINITE", EarlyExercisePdeRiskAvailability_::Value_::UNAVAILABLE_NONFINITE)
        .value("UNAVAILABLE_STEP_UNSTABLE", EarlyExercisePdeRiskAvailability_::Value_::UNAVAILABLE_STEP_UNSTABLE);
    py::enum_<EarlyExercisePdeRiskScenarioStatus_::Value_>(m, "EarlyExercisePdeRiskScenarioStatus_")
        .value("PLANNED", EarlyExercisePdeRiskScenarioStatus_::Value_::PLANNED)
        .value("SUCCESS", EarlyExercisePdeRiskScenarioStatus_::Value_::SUCCESS)
        .value("NUMERICAL_FAILURE", EarlyExercisePdeRiskScenarioStatus_::Value_::NUMERICAL_FAILURE)
        .value("NOT_RUN_WORK_LIMIT", EarlyExercisePdeRiskScenarioStatus_::Value_::NOT_RUN_WORK_LIMIT)
        .value("NOT_RUN_BASE_FAILED", EarlyExercisePdeRiskScenarioStatus_::Value_::NOT_RUN_BASE_FAILED);
    py::enum_<EarlyExercisePdeRiskWorkLimitSource_::Value_>(m, "EarlyExercisePdeRiskWorkLimitSource_")
        .value("NONE", EarlyExercisePdeRiskWorkLimitSource_::Value_::NONE)
        .value("SCENARIO", EarlyExercisePdeRiskWorkLimitSource_::Value_::SCENARIO)
        .value("GLOBAL", EarlyExercisePdeRiskWorkLimitSource_::Value_::GLOBAL);
    py::enum_<EarlyExercisePdeRiskMembership_::Value_>(m, "EarlyExercisePdeRiskMembership_")
        .value("NOT_EXERCISABLE", EarlyExercisePdeRiskMembership_::Value_::NOT_EXERCISABLE)
        .value("CONTINUATION", EarlyExercisePdeRiskMembership_::Value_::CONTINUATION)
        .value("EXERCISE", EarlyExercisePdeRiskMembership_::Value_::EXERCISE)
        .value("BOUNDARY", EarlyExercisePdeRiskMembership_::Value_::BOUNDARY);
    py::enum_<EarlyExercisePdeRiskBumpOrigin_::Value_>(m, "EarlyExercisePdeRiskBumpOrigin_")
        .value("DEFAULT", EarlyExercisePdeRiskBumpOrigin_::Value_::DEFAULT)
        .value("EXPLICIT", EarlyExercisePdeRiskBumpOrigin_::Value_::EXPLICIT);
    auto settings = WithCopies(py::class_<EarlyExercisePdeRiskSettings_>(m, "EarlyExercisePdeRiskSettings_"));
    settings.def(py::init(&NewSettings));
    ReadOnly(&settings, "delta", &EarlyExercisePdeRiskSettings_::delta_);
    ReadOnly(&settings, "gamma", &EarlyExercisePdeRiskSettings_::gamma_);
    ReadOnly(&settings, "vega", &EarlyExercisePdeRiskSettings_::vega_);
    ReadOnly(&settings, "carry_risk", &EarlyExercisePdeRiskSettings_::carryRisk_);
    ReadOnly(&settings, "spot_bump", &EarlyExercisePdeRiskSettings_::spotBump_);
    ReadOnly(&settings, "volatility_bump", &EarlyExercisePdeRiskSettings_::volatilityBump_);
    ReadOnly(&settings, "rate_bumps", &EarlyExercisePdeRiskSettings_::rateBumps_);
    ReadOnly(&settings, "dividend_yield_bumps", &EarlyExercisePdeRiskSettings_::dividendYieldBumps_);
    ReadOnly(&settings, "delta_stability_tolerance", &EarlyExercisePdeRiskSettings_::deltaStabilityTolerance_);
    ReadOnly(&settings, "gamma_stability_tolerance", &EarlyExercisePdeRiskSettings_::gammaStabilityTolerance_);
    ReadOnly(&settings, "vega_stability_tolerance", &EarlyExercisePdeRiskSettings_::vegaStabilityTolerance_);
    ReadOnly(&settings, "rate_stability_tolerance", &EarlyExercisePdeRiskSettings_::rateStabilityTolerance_);
    ReadOnly(&settings, "dividend_yield_stability_tolerance", &EarlyExercisePdeRiskSettings_::dividendYieldStabilityTolerance_);
    ReadOnly(&settings, "max_scenarios", &EarlyExercisePdeRiskSettings_::maxScenarios_);
    ReadOnly(&settings, "max_work_units", &EarlyExercisePdeRiskSettings_::maxWorkUnits_);
    ReadOnly(&settings, "numeric_payload_budget_bytes", &EarlyExercisePdeRiskSettings_::numericPayloadBudgetBytes_);
    ReadOnly(&settings, "workspace_budget_bytes", &EarlyExercisePdeRiskSettings_::workspaceBudgetBytes_);
    auto request = WithCopies(py::class_<EarlyExercisePdeRiskRequest_>(m, "EarlyExercisePdeRiskRequest_"));
    request.def(py::init([](const py::args& args, const py::kwargs& kwargs) {
        py::dict defaults;
        defaults["valuation"] = py::none();
        defaults["risk"] = py::none();
        const auto values = Arguments(args, kwargs, defaults, "valuation");
        return Input([&] {
            EarlyExercisePdeRiskRequest_ value;
            value.valuation_ = RequiredRiskInput<EarlyExercisePdeRequest_>(values["valuation"], "valuation", "EarlyExercisePdeRequest_", IDENTIFIER);
            if (!values["risk"].is_none())
                value.risk_ = RequiredRiskInput<EarlyExercisePdeRiskSettings_>(values["risk"], "risk", "EarlyExercisePdeRiskSettings_", IDENTIFIER);
            ValidateEarlyExercisePdeRiskRequest(value);
            return value;
        });
    }));
    ReadOnly(&request, "valuation", &EarlyExercisePdeRiskRequest_::valuation_);
    ReadOnly(&request, "risk", &EarlyExercisePdeRiskRequest_::risk_);
    auto stencilentry = WithCopies(py::class_<EarlyExercisePdeRiskStencilEntry_>(m, "EarlyExercisePdeRiskStencilEntry_"));
    ReadOnly(&stencilentry, "scenario_index", &EarlyExercisePdeRiskStencilEntry_::scenarioIndex_);
    ReadOnly(&stencilentry, "weight", &EarlyExercisePdeRiskStencilEntry_::weight_);
    auto record = WithCopies(py::class_<EarlyExercisePdeRiskRecord_>(m, "EarlyExercisePdeRiskRecord_"));
    ReadOnly(&record, "risk_index", &EarlyExercisePdeRiskRecord_::riskIndex_);
    ReadOnly(&record, "risk_id", &EarlyExercisePdeRiskRecord_::riskId_);
    ReadOnly(&record, "parameter_id", &EarlyExercisePdeRiskRecord_::parameterId_);
    ReadOnly(&record, "derivative_order", &EarlyExercisePdeRiskRecord_::derivativeOrder_);
    ReadOnly(&record, "segment_index", &EarlyExercisePdeRiskRecord_::segmentIndex_);
    ReadOnly(&record, "start_date", &EarlyExercisePdeRiskRecord_::startDate_);
    ReadOnly(&record, "end_date", &EarlyExercisePdeRiskRecord_::endDate_);
    ReadOnly(&record, "parameter_value", &EarlyExercisePdeRiskRecord_::parameterValue_);
    ReadOnly(&record, "unit", &EarlyExercisePdeRiskRecord_::unit_);
    ReadOnly(&record, "value", &EarlyExercisePdeRiskRecord_::value_);
    ReadOnly(&record, "status", &EarlyExercisePdeRiskRecord_::status_);
    ReadOnly(&record, "reason", &EarlyExercisePdeRiskRecord_::reason_);
    ReadOnly(&record, "method", &EarlyExercisePdeRiskRecord_::method_);
    ReadOnly(&record, "interpretation", &EarlyExercisePdeRiskRecord_::interpretation_);
    ReadOnly(&record, "fine_bump", &EarlyExercisePdeRiskRecord_::fineBump_);
    ReadOnly(&record, "coarse_bump", &EarlyExercisePdeRiskRecord_::coarseBump_);
    ReadOnly(&record, "bump_origin", &EarlyExercisePdeRiskRecord_::bumpOrigin_);
    ReadOnly(&record, "fine_estimate", &EarlyExercisePdeRiskRecord_::fineEstimate_);
    ReadOnly(&record, "coarse_estimate", &EarlyExercisePdeRiskRecord_::coarseEstimate_);
    ReadOnly(&record, "step_difference", &EarlyExercisePdeRiskRecord_::stepDifference_);
    ReadOnly(&record, "normalized_step_difference", &EarlyExercisePdeRiskRecord_::normalizedStepDifference_);
    ReadOnly(&record, "stability_tolerance", &EarlyExercisePdeRiskRecord_::stabilityTolerance_);
    ReadOnly(&record, "fine_stencil", &EarlyExercisePdeRiskRecord_::fineStencil_);
    ReadOnly(&record, "coarse_stencil", &EarlyExercisePdeRiskRecord_::coarseStencil_);
    auto planstep = WithCopies(py::class_<EarlyExercisePdeRiskPlanStep_>(m, "EarlyExercisePdeRiskPlanStep_"));
    ReadOnly(&planstep, "source_time_index", &EarlyExercisePdeRiskPlanStep_::sourceTimeIndex_);
    ReadOnly(&planstep, "time_index", &EarlyExercisePdeRiskPlanStep_::timeIndex_);
    ReadOnly(&planstep, "dt", &EarlyExercisePdeRiskPlanStep_::dt_);
    ReadOnly(&planstep, "theta", &EarlyExercisePdeRiskPlanStep_::theta_);
    ReadOnly(&planstep, "carry_index", &EarlyExercisePdeRiskPlanStep_::carryIndex_);
    ReadOnly(&planstep, "damped", &EarlyExercisePdeRiskPlanStep_::damped_);
    ReadOnly(&planstep, "projection", &EarlyExercisePdeRiskPlanStep_::projection_);
    auto plan = WithCopies(py::class_<EarlyExercisePdeRiskPlan_>(m, "EarlyExercisePdeRiskPlan_"));
    ReadOnly(&plan, "schema_version", &EarlyExercisePdeRiskPlan_::schemaVersion_);
    ReadOnly(&plan, "plan_id", &EarlyExercisePdeRiskPlan_::planId_);
    ReadOnly(&plan, "evaluation_date", &EarlyExercisePdeRiskPlan_::evaluationDate_);
    ReadOnly(&plan, "maturity_date", &EarlyExercisePdeRiskPlan_::maturityDate_);
    ReadOnly(&plan, "upper", &EarlyExercisePdeRiskPlan_::upper_);
    ReadOnly(&plan, "grid_points", &EarlyExercisePdeRiskPlan_::gridPoints_);
    ReadOnly(&plan, "grid_spacing", &EarlyExercisePdeRiskPlan_::gridSpacing_);
    ReadOnly(&plan, "grid", &EarlyExercisePdeRiskPlan_::grid_);
    ReadOnly(&plan, "time_grid", &EarlyExercisePdeRiskPlan_::timeGrid_);
    ReadOnly(&plan, "steps", &EarlyExercisePdeRiskPlan_::steps_);
    ReadOnly(&plan, "epsilon", &EarlyExercisePdeRiskPlan_::epsilon_);
    ReadOnly(&plan, "theta", &EarlyExercisePdeRiskPlan_::theta_);
    ReadOnly(&plan, "effective_rannacher_intervals", &EarlyExercisePdeRiskPlan_::effectiveRannacherIntervals_);
    ReadOnly(&plan, "penalty_factor", &EarlyExercisePdeRiskPlan_::penaltyFactor_);
    ReadOnly(&plan, "max_iterations_per_step", &EarlyExercisePdeRiskPlan_::maxIterationsPerStep_);
    ReadOnly(&plan, "linear_backward_error_limit", &EarlyExercisePdeRiskPlan_::linearBackwardErrorLimit_);
    ReadOnly(&plan, "interpolation_policy", &EarlyExercisePdeRiskPlan_::interpolationPolicy_);
    ReadOnly(&plan, "resolved_risk", &EarlyExercisePdeRiskPlan_::resolvedRisk_);
    ReadOnly(&plan, "spot_bump_origin", &EarlyExercisePdeRiskPlan_::spotBumpOrigin_);
    ReadOnly(&plan, "volatility_bump_origin", &EarlyExercisePdeRiskPlan_::volatilityBumpOrigin_);
    ReadOnly(&plan, "rate_bump_origins", &EarlyExercisePdeRiskPlan_::rateBumpOrigins_);
    ReadOnly(&plan, "dividend_yield_bump_origins", &EarlyExercisePdeRiskPlan_::dividendYieldBumpOrigins_);
    ReadOnly(&plan, "planned_scenarios", &EarlyExercisePdeRiskPlan_::plannedScenarios_);
    ReadOnly(&plan, "base_planned_steps", &EarlyExercisePdeRiskPlan_::basePlannedSteps_);
    ReadOnly(&plan, "planned_steps", &EarlyExercisePdeRiskPlan_::plannedSteps_);
    ReadOnly(&plan, "planned_numeric_payload_bytes", &EarlyExercisePdeRiskPlan_::plannedNumericPayloadBytes_);
    ReadOnly(&plan, "planned_workspace_bytes", &EarlyExercisePdeRiskPlan_::plannedWorkspaceBytes_);
    ReadOnly(&plan, "planned_peak_native_bytes", &EarlyExercisePdeRiskPlan_::plannedPeakNativeBytes_);
    auto scenario = WithCopies(py::class_<EarlyExercisePdeRiskScenario_>(m, "EarlyExercisePdeRiskScenario_"));
    ReadOnly(&scenario, "scenario_index", &EarlyExercisePdeRiskScenario_::scenarioIndex_);
    ReadOnly(&scenario, "scenario_id", &EarlyExercisePdeRiskScenario_::scenarioId_);
    ReadOnly(&scenario, "plan_id", &EarlyExercisePdeRiskScenario_::planId_);
    ReadOnly(&scenario, "parameter_id", &EarlyExercisePdeRiskScenario_::parameterId_);
    ReadOnly(&scenario, "segment_index", &EarlyExercisePdeRiskScenario_::segmentIndex_);
    ReadOnly(&scenario, "parameter_value", &EarlyExercisePdeRiskScenario_::parameterValue_);
    ReadOnly(&scenario, "offset", &EarlyExercisePdeRiskScenario_::offset_);
    ReadOnly(&scenario, "status", &EarlyExercisePdeRiskScenario_::status_);
    ReadOnly(&scenario, "solver_status", &EarlyExercisePdeRiskScenario_::solverStatus_);
    ReadOnly(&scenario, "work_limit_source", &EarlyExercisePdeRiskScenario_::workLimitSource_);
    ReadOnly(&scenario, "price_available", &EarlyExercisePdeRiskScenario_::priceAvailable_);
    ReadOnly(&scenario, "price", &EarlyExercisePdeRiskScenario_::price_);
    ReadOnly(&scenario, "epsilon", &EarlyExercisePdeRiskScenario_::epsilon_);
    ReadOnly(&scenario, "planned_steps", &EarlyExercisePdeRiskScenario_::plannedSteps_);
    ReadOnly(&scenario, "actual_steps", &EarlyExercisePdeRiskScenario_::actualSteps_);
    ReadOnly(&scenario, "completed_steps", &EarlyExercisePdeRiskScenario_::completedSteps_);
    ReadOnly(&scenario, "total_solves", &EarlyExercisePdeRiskScenario_::totalSolves_);
    ReadOnly(&scenario, "work_units", &EarlyExercisePdeRiskScenario_::workUnits_);
    ReadOnly(&scenario, "numeric_payload_bytes", &EarlyExercisePdeRiskScenario_::numericPayloadBytes_);
    ReadOnly(&scenario, "peak_workspace_bytes", &EarlyExercisePdeRiskScenario_::peakWorkspaceBytes_);
    ReadOnly(&scenario, "max_iteration_change", &EarlyExercisePdeRiskScenario_::maxIterationChange_);
    ReadOnly(&scenario, "max_linear_backward_error", &EarlyExercisePdeRiskScenario_::maxLinearBackwardError_);
    ReadOnly(&scenario, "max_obstacle_violation", &EarlyExercisePdeRiskScenario_::maxObstacleViolation_);
    ReadOnly(&scenario, "max_dual_violation", &EarlyExercisePdeRiskScenario_::maxDualViolation_);
    ReadOnly(&scenario, "max_complementarity_residual", &EarlyExercisePdeRiskScenario_::maxComplementarityResidual_);
    ReadOnly(&scenario, "failure", &EarlyExercisePdeRiskScenario_::failure_);
    ReadOnly(&scenario, "evaluation_membership", &EarlyExercisePdeRiskScenario_::evaluationMembership_);
    ReadOnly(&scenario, "evaluation_boundary_intersects_spot_stencil", &EarlyExercisePdeRiskScenario_::evaluationBoundaryIntersectsSpotStencil_);
    ReadOnly(&scenario, "future_contact_changed", &EarlyExercisePdeRiskScenario_::futureContactChanged_);
    ReadOnly(&scenario, "upwind_changed", &EarlyExercisePdeRiskScenario_::upwindChanged_);
    auto execution = WithCopies(py::class_<EarlyExercisePdeRiskExecution_>(m, "EarlyExercisePdeRiskExecution_"));
    ReadOnly(&execution, "planned_scenarios", &EarlyExercisePdeRiskExecution_::plannedScenarios_);
    ReadOnly(&execution, "attempted_scenarios", &EarlyExercisePdeRiskExecution_::attemptedScenarios_);
    ReadOnly(&execution, "successful_scenarios", &EarlyExercisePdeRiskExecution_::successfulScenarios_);
    ReadOnly(&execution, "failed_scenarios", &EarlyExercisePdeRiskExecution_::failedScenarios_);
    ReadOnly(&execution, "not_run_scenarios", &EarlyExercisePdeRiskExecution_::notRunScenarios_);
    ReadOnly(&execution, "total_solves", &EarlyExercisePdeRiskExecution_::totalSolves_);
    ReadOnly(&execution, "work_units", &EarlyExercisePdeRiskExecution_::workUnits_);
    ReadOnly(&execution, "planned_numeric_payload_bytes", &EarlyExercisePdeRiskExecution_::plannedNumericPayloadBytes_);
    ReadOnly(&execution, "planned_workspace_bytes", &EarlyExercisePdeRiskExecution_::plannedWorkspaceBytes_);
    ReadOnly(&execution, "planned_peak_native_bytes", &EarlyExercisePdeRiskExecution_::plannedPeakNativeBytes_);
    ReadOnly(&execution, "numeric_payload_bytes", &EarlyExercisePdeRiskExecution_::numericPayloadBytes_);
    ReadOnly(&execution, "peak_workspace_bytes", &EarlyExercisePdeRiskExecution_::peakWorkspaceBytes_);
    ReadOnly(&execution, "peak_native_bytes", &EarlyExercisePdeRiskExecution_::peakNativeBytes_);
    ReadOnly(&execution, "work_limit_scenario_index", &EarlyExercisePdeRiskExecution_::workLimitScenarioIndex_);
    ReadOnly(&execution, "work_limit_source", &EarlyExercisePdeRiskExecution_::workLimitSource_);
    WithCopies(py::class_<EarlyExercisePdeRiskResult_>(m, "EarlyExercisePdeRiskResult_"))
        .def_property_readonly("schema_version", [](const EarlyExercisePdeRiskResult_& value) { return Detached(value.SchemaVersion()); })
        .def_property_readonly("status", [](const EarlyExercisePdeRiskResult_& value) { return Detached(value.Status()); })
        .def_property_readonly("is_complete", [](const EarlyExercisePdeRiskResult_& value) { return Detached(value.IsComplete()); })
        .def_property_readonly("price_available", [](const EarlyExercisePdeRiskResult_& value) { return Detached(value.PriceAvailable()); })
        .def_property_readonly("price", [](const EarlyExercisePdeRiskResult_& value) { return Detached(value.Price()); })
        .def_property_readonly("request", [](const EarlyExercisePdeRiskResult_& value) { return Detached(value.Request()); })
        .def_property_readonly("base_result", [](const EarlyExercisePdeRiskResult_& value) { return Detached(value.BaseResult()); })
        .def_property_readonly("plan", [](const EarlyExercisePdeRiskResult_& value) { return Detached(value.Plan()); })
        .def_property_readonly("risks", [](const EarlyExercisePdeRiskResult_& value) { return Detached(value.Risks()); })
        .def_property_readonly("scenarios", [](const EarlyExercisePdeRiskResult_& value) { return Detached(value.Scenarios()); })
        .def_property_readonly("execution", [](const EarlyExercisePdeRiskResult_& value) { return Detached(value.Execution()); })
        .def_property_readonly("method", [](const EarlyExercisePdeRiskResult_& value) { return Detached(value.Method()); })
        .def_property_readonly("risk_basis", [](const EarlyExercisePdeRiskResult_& value) { return Detached(value.RiskBasis()); })
        .def_property_readonly("aad_used", [](const EarlyExercisePdeRiskResult_& value) { return Detached(value.AadUsed()); })
        .def("risk_value", [](const EarlyExercisePdeRiskResult_& value, const py::args& args, const py::kwargs& kwargs) {
            py::dict defaults;
            defaults["risk_id"] = py::none();
            const auto values = Arguments(args, kwargs, defaults, "risk_id");
            return value.RiskValue(Input([&] { return SettingStringInput(values["risk_id"], "risk_id", IDENTIFIER); }));
        });
    m.def("EarlyExercisePdeRiskResult_New", [](const py::args& args, const py::kwargs& kwargs) {
        py::dict defaults;
        defaults["request"] = py::none();
        const auto values = Arguments(args, kwargs, defaults, "request");
        const auto request =
            RequiredRiskInput<EarlyExercisePdeRiskRequest_>(values["request"], "request", "EarlyExercisePdeRiskRequest_", IDENTIFIER);
        py::gil_scoped_release release;
        return EvaluateEarlyExercisePdeRisk(request);
    });
}

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <dal-public/src/earlyexercisepderisk.hpp>
#include <dal/math/aad/native.hpp>
#include <dal/math/aad/recording.hpp>

#include "../../../dal-cpp/benchmarks/pde_risk_perf/copy.hpp"

namespace py = pybind11;
using namespace Dal;

namespace {
    template <class T_, class = void> struct Enum_ : std::false_type {};
    template <class T_> struct Enum_<T_, std::void_t<decltype(std::declval<T_>().String())>> : std::true_type {};
    template <class T_> py::object Scalar(const T_& value) {
        if constexpr (std::is_same_v<T_, String_>)
            return py::str(std::string(value.data(), value.size()));
        else if constexpr (Enum_<T_>::value)
            return py::str(value.String());
        else
            return py::cast(value, py::return_value_policy::copy);
    }
    template <class T_> py::object Scalar(const std::optional<T_>& value) { return value ? Scalar(*value) : py::none(); }
    template <class T_> py::dict Snapshot(const T_& value);
    template <class T_> py::list Rows(const Vector_<T_>& values) {
        py::list rows;
        for (const auto& value : values)
            rows.append(Snapshot(value));
        return rows;
    }
    template <> py::dict Snapshot(const EarlyExercisePdeRiskStencilEntry_& value) {
        py::dict result;
        result["scenario_index"] = Scalar(value.scenarioIndex_);
        result["weight"] = Scalar(value.weight_);
        return result;
    }
    template <> py::dict Snapshot(const EarlyExercisePdeRiskRecord_& value) {
        py::dict result;
        result["risk_index"] = Scalar(value.riskIndex_);
        result["risk_id"] = Scalar(value.riskId_);
        result["parameter_id"] = Scalar(value.parameterId_);
        result["derivative_order"] = Scalar(value.derivativeOrder_);
        result["segment_index"] = Scalar(value.segmentIndex_);
        result["start_date"] = Scalar(value.startDate_);
        result["end_date"] = Scalar(value.endDate_);
        result["parameter_value"] = Scalar(value.parameterValue_);
        result["unit"] = Scalar(value.unit_);
        result["value"] = Scalar(value.value_);
        result["status"] = Scalar(value.status_);
        result["reason"] = Scalar(value.reason_);
        result["method"] = Scalar(value.method_);
        result["interpretation"] = Scalar(value.interpretation_);
        result["fine_bump"] = Scalar(value.fineBump_);
        result["coarse_bump"] = Scalar(value.coarseBump_);
        result["bump_origin"] = Scalar(value.bumpOrigin_);
        result["fine_estimate"] = Scalar(value.fineEstimate_);
        result["coarse_estimate"] = Scalar(value.coarseEstimate_);
        result["step_difference"] = Scalar(value.stepDifference_);
        result["normalized_step_difference"] = Scalar(value.normalizedStepDifference_);
        result["stability_tolerance"] = Scalar(value.stabilityTolerance_);
        result["fine_stencil"] = Rows(value.fineStencil_);
        result["coarse_stencil"] = Rows(value.coarseStencil_);
        return result;
    }
    template <> py::dict Snapshot(const EarlyExercisePdeRiskPlanStep_& value) {
        py::dict result;
        result["source_time_index"] = Scalar(value.sourceTimeIndex_);
        result["time_index"] = Scalar(value.timeIndex_);
        result["dt"] = Scalar(value.dt_);
        result["theta"] = Scalar(value.theta_);
        result["carry_index"] = Scalar(value.carryIndex_);
        result["damped"] = Scalar(value.damped_);
        result["projection"] = Scalar(value.projection_);
        return result;
    }
    template <> py::dict Snapshot(const EarlyExercisePdeRiskScenario_& value) {
        py::dict result;
        result["scenario_index"] = Scalar(value.scenarioIndex_);
        result["scenario_id"] = Scalar(value.scenarioId_);
        result["plan_id"] = Scalar(value.planId_);
        result["parameter_id"] = Scalar(value.parameterId_);
        result["segment_index"] = Scalar(value.segmentIndex_);
        result["parameter_value"] = Scalar(value.parameterValue_);
        result["offset"] = Scalar(value.offset_);
        result["status"] = Scalar(value.status_);
        result["solver_status"] = Scalar(value.solverStatus_);
        result["work_limit_source"] = Scalar(value.workLimitSource_);
        result["price_available"] = Scalar(value.priceAvailable_);
        result["price"] = Scalar(value.price_);
        result["epsilon"] = Scalar(value.epsilon_);
        result["planned_steps"] = Scalar(value.plannedSteps_);
        result["actual_steps"] = Scalar(value.actualSteps_);
        result["completed_steps"] = Scalar(value.completedSteps_);
        result["total_solves"] = Scalar(value.totalSolves_);
        result["work_units"] = Scalar(value.workUnits_);
        result["numeric_payload_bytes"] = Scalar(value.numericPayloadBytes_);
        result["peak_workspace_bytes"] = Scalar(value.peakWorkspaceBytes_);
        result["max_iteration_change"] = Scalar(value.maxIterationChange_);
        result["max_linear_backward_error"] = Scalar(value.maxLinearBackwardError_);
        result["max_obstacle_violation"] = Scalar(value.maxObstacleViolation_);
        result["max_dual_violation"] = Scalar(value.maxDualViolation_);
        result["max_complementarity_residual"] = Scalar(value.maxComplementarityResidual_);
        if (value.failure_) {
            const auto& f = *value.failure_;
            py::dict failure;
            failure["status"] = Scalar(f.status_);
            failure["time_index"] = Scalar(f.timeIndex_);
            failure["iteration"] = Scalar(f.iteration_);
            failure["calendar_date"] = Scalar(f.calendarDate_);
            failure["node"] = Scalar(f.node_);
            failure["residual"] = Scalar(f.residual_);
            failure["threshold"] = Scalar(f.threshold_);
            failure["work_units"] = Scalar(f.workUnits_);
            failure["reason"] = Scalar(f.reason_);
            result["failure"] = failure;
        } else
            result["failure"] = py::none();
        result["evaluation_membership"] = Scalar(value.evaluationMembership_);
        result["evaluation_boundary_intersects_spot_stencil"] = Scalar(value.evaluationBoundaryIntersectsSpotStencil_);
        result["future_contact_changed"] = Scalar(value.futureContactChanged_);
        result["upwind_changed"] = Scalar(value.upwindChanged_);
        return result;
    }
    template <> py::dict Snapshot(const EarlyExercisePdeRiskExecution_& value) {
        py::dict result;
        result["planned_scenarios"] = Scalar(value.plannedScenarios_);
        result["attempted_scenarios"] = Scalar(value.attemptedScenarios_);
        result["successful_scenarios"] = Scalar(value.successfulScenarios_);
        result["failed_scenarios"] = Scalar(value.failedScenarios_);
        result["not_run_scenarios"] = Scalar(value.notRunScenarios_);
        result["total_solves"] = Scalar(value.totalSolves_);
        result["work_units"] = Scalar(value.workUnits_);
        result["planned_numeric_payload_bytes"] = Scalar(value.plannedNumericPayloadBytes_);
        result["planned_workspace_bytes"] = Scalar(value.plannedWorkspaceBytes_);
        result["planned_peak_native_bytes"] = Scalar(value.plannedPeakNativeBytes_);
        result["numeric_payload_bytes"] = Scalar(value.numericPayloadBytes_);
        result["peak_workspace_bytes"] = Scalar(value.peakWorkspaceBytes_);
        result["peak_native_bytes"] = Scalar(value.peakNativeBytes_);
        result["work_limit_scenario_index"] = Scalar(value.workLimitScenarioIndex_);
        result["work_limit_source"] = Scalar(value.workLimitSource_);
        return result;
    }
    py::dict Settings(const EarlyExercisePdeRiskSettings_& value) {
        py::dict result;
        result["delta"] = Scalar(value.delta_);
        result["gamma"] = Scalar(value.gamma_);
        result["vega"] = Scalar(value.vega_);
        result["carry_risk"] = Scalar(value.carryRisk_);
        result["spot_bump"] = Scalar(value.spotBump_);
        result["volatility_bump"] = Scalar(value.volatilityBump_);
        result["rate_bumps"] = py::cast(std::vector<double>(value.rateBumps_.begin(), value.rateBumps_.end()));
        result["dividend_yield_bumps"] = py::cast(std::vector<double>(value.dividendYieldBumps_.begin(), value.dividendYieldBumps_.end()));
        result["delta_stability_tolerance"] = Scalar(value.deltaStabilityTolerance_);
        result["gamma_stability_tolerance"] = Scalar(value.gammaStabilityTolerance_);
        result["vega_stability_tolerance"] = Scalar(value.vegaStabilityTolerance_);
        result["rate_stability_tolerance"] = Scalar(value.rateStabilityTolerance_);
        result["dividend_yield_stability_tolerance"] = Scalar(value.dividendYieldStabilityTolerance_);
        result["max_scenarios"] = Scalar(value.maxScenarios_);
        result["max_work_units"] = Scalar(value.maxWorkUnits_);
        result["numeric_payload_budget_bytes"] = Scalar(value.numericPayloadBudgetBytes_);
        result["workspace_budget_bytes"] = Scalar(value.workspaceBudgetBytes_);
        return result;
    }
    py::dict Plan(const EarlyExercisePdeRiskPlan_& value) {
        py::dict result;
        result["schema_version"] = Scalar(value.schemaVersion_);
        result["plan_id"] = Scalar(value.planId_);
        result["evaluation_date"] = Scalar(value.evaluationDate_);
        result["maturity_date"] = Scalar(value.maturityDate_);
        result["upper"] = Scalar(value.upper_);
        result["grid_points"] = Scalar(value.gridPoints_);
        result["grid_spacing"] = Scalar(value.gridSpacing_);
        py::list grid;
        for (const auto& item : value.grid_)
            grid.append(Scalar(item));
        result["grid"] = grid;
        py::list times;
        for (const auto& t : value.timeGrid_) {
            py::dict node;
            node["time_years"] = Scalar(t.timeYears_);
            node["calendar_date"] = Scalar(t.calendarDate_);
            node["is_exercise_date"] = Scalar(t.isExerciseDate_);
            node["is_carry_knot"] = Scalar(t.isCarryKnot_);
            node["is_maturity"] = Scalar(t.isMaturity_);
            times.append(node);
        }
        result["time_grid"] = times;
        result["steps"] = Rows(value.steps_);
        result["epsilon"] = Scalar(value.epsilon_);
        result["theta"] = Scalar(value.theta_);
        result["effective_rannacher_intervals"] = Scalar(value.effectiveRannacherIntervals_);
        result["penalty_factor"] = Scalar(value.penaltyFactor_);
        result["max_iterations_per_step"] = Scalar(value.maxIterationsPerStep_);
        result["linear_backward_error_limit"] = Scalar(value.linearBackwardErrorLimit_);
        result["interpolation_policy"] = Scalar(value.interpolationPolicy_);
        result["resolved_risk"] = Settings(value.resolvedRisk_);
        result["spot_bump_origin"] = Scalar(value.spotBumpOrigin_);
        result["volatility_bump_origin"] = Scalar(value.volatilityBumpOrigin_);
        py::list rate_bump_origins;
        for (const auto& item : value.rateBumpOrigins_)
            rate_bump_origins.append(Scalar(item));
        result["rate_bump_origins"] = rate_bump_origins;
        py::list dividend_yield_bump_origins;
        for (const auto& item : value.dividendYieldBumpOrigins_)
            dividend_yield_bump_origins.append(Scalar(item));
        result["dividend_yield_bump_origins"] = dividend_yield_bump_origins;
        result["planned_scenarios"] = Scalar(value.plannedScenarios_);
        result["base_planned_steps"] = Scalar(value.basePlannedSteps_);
        result["planned_steps"] = Scalar(value.plannedSteps_);
        result["planned_numeric_payload_bytes"] = Scalar(value.plannedNumericPayloadBytes_);
        result["planned_workspace_bytes"] = Scalar(value.plannedWorkspaceBytes_);
        result["planned_peak_native_bytes"] = Scalar(value.plannedPeakNativeBytes_);
        return result;
    }
} // namespace

PYBIND11_MODULE(_dal_pde_risk_test, m) {
    m.def("_benchmark_copy", [](const EarlyExercisePdeRiskRequest_& request, size_t reads) {
        REQUIRE(reads >= 1 && reads <= 1000, "copy probe reads outside [1,1000]");
        const auto probe = PdeRiskBench::CopyProbe(request, reads);
        py::dict output;
        output["compute_ns"] = probe.computeNs_;
        output["copy_ns"] = probe.copyNs_;
        output["records"] = probe.records_;
        output["output_bytes"] = probe.outputBytes_;
        output["peak_detached_capacity"] = probe.peakDetachedCapacity_;
        output["work"] = probe.work_;
        output["solves"] = probe.solves_;
        output["payload"] = probe.payload_;
        return output;
    });
    m.def("_test_early_exercise_risk_core", [](const EarlyExercisePdeRiskRequest_& request) {
        const auto result = PDE::EvaluateEarlyExercisePdeRisk(request);
        py::dict output;
        output["status"] = Scalar(result.Status());
        output["price"] = result.PriceAvailable() ? Scalar(result.Price()) : py::none();
        output["plan"] = Plan(result.Plan());
        output["risks"] = Rows(result.Risks());
        output["scenarios"] = Rows(result.Scenarios());
        output["execution"] = Snapshot(result.Execution());
        return output;
    });
    m.def("_test_early_exercise_risk_tape", [](const py::function& call) {
        using namespace AAD;
        Clear(*Tape());
        auto mode = SetNumResultsForAAD(true, 2);
        RecordingScope_ scope;
        Number_ x;
        scope.RegisterInput(x, 2.);
        scope.StartRecording();
        Number_ square = x * x;
        const auto nodes = Tape()->nodes_.OccupiedSlots();
        call();
        REQUIRE(Tape()->nodes_.OccupiedSlots() == nodes, "binding changed recording tape");
        Number_ cube = square * x;
        scope.FinishRecording();
        scope.ClearAdjoints();
        NativeOperations_::SetSeed(cube, 3., 1);
        const auto finished = Tape()->nodes_.OccupiedSlots(), events = Tape()->ReverseEventCount();
        const auto ders = Tape()->ders_.OccupiedSlots(), args = Tape()->argPtrs_.OccupiedSlots();
        call();
        REQUIRE(Tape()->nodes_.OccupiedSlots() == finished && Tape()->ReverseEventCount() == events && Tape()->ders_.OccupiedSlots() == ders &&
                    Tape()->argPtrs_.OccupiedSlots() == args && Tape()->multi_ && Tape()->numAdj_ == 2 &&
                    NativeOperations_::ReadAdjoint(cube, 1) == 3.,
                "binding changed finished graph, events, channels or seeds");
        scope.Reverse();
        REQUIRE(NativeOperations_::ReadAdjoint(x, 1) == 36., "binding changed subsequent reverse");
        scope.Close();
        Clear(*Tape());
    });
}

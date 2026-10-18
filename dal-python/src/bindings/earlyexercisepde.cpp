#include "bindings.h"

#include <cmath>
#include <limits>
#include <pybind11/stl.h>

#include <dal-public/src/earlyexercisepde.hpp>

#include "riskrequest.hpp"

using namespace Dal;
using namespace Dal::Python;

namespace {
    constexpr const char* IDENTIFIER = "InvalidEarlyExercisePdeRequest";

    double Real(const py::handle& value, const char* field) {
        const auto context = InputContext(value, field, "finite int or float excluding bool and enums", IDENTIFIER);
        if (PyBool_Check(value.ptr()) || IsEnum(value) || (!PyLong_Check(value.ptr()) && !PyFloat_Check(value.ptr())))
            throw py::type_error(context);
        const double result = PyFloat_AsDouble(value.ptr());
        if (PyErr_Occurred()) {
            PyErr_Clear();
            throw py::value_error(context);
        }
        REQUIRE(std::isfinite(result), String_(context));
        return result;
    }
    int Integer(const py::handle& value, const char* field) {
        return static_cast<int>(IntegerInput(value, InputContext(value, field, "representable int excluding bool and enums", IDENTIFIER),
                                             std::numeric_limits<int>::min(), std::numeric_limits<int>::max(), "; exceeds int range"));
    }
    Date_ DateInput(const py::handle& value, const char* field) { return RequiredRiskInput<Date_>(value, field, "dal.Date_", IDENTIFIER); }
    template <class E_> E_ EnumInput(const py::handle& value, const char* field, const char* first, const char* second) {
        if (py::isinstance<typename E_::Value_>(value))
            return E_(py::cast<typename E_::Value_>(value));
        const auto text = SettingStringInput(value, field, IDENTIFIER);
        REQUIRE(text == first || text == second, String_("InvalidEarlyExercisePdeRequest; ") + field + "; expected canonical enum string");
        return E_(text);
    }
    template <class T_, class V_> void ReadOnly(py::class_<T_>* type, const char* name, V_ T_::* member) {
        type->def_property_readonly(name, [member](const T_& value) { return value.*member; });
    }
    EarlyExercisePdeSettings_ NewSettings(const py::object& gridPoints,
                                          const py::object& upper,
                                          const py::object& ordinarySteps,
                                          const py::object& theta,
                                          const py::object& rannacherIntervals,
                                          const py::object& penaltyFactor,
                                          const py::object& absoluteTolerance,
                                          const py::object& relativeTolerance,
                                          const py::object& linearBackwardErrorLimit,
                                          const py::object& maxIterationsPerStep,
                                          const py::object& maxActualSteps,
                                          const py::object& maxWorkUnits,
                                          const py::object& numericPayloadBudgetBytes,
                                          const py::object& workspaceBudgetBytes) {
        EarlyExercisePdeSettings_ s;
        s.gridPoints_ = Integer(gridPoints, "grid_points");
        s.upper_ = upper.is_none() ? std::nullopt : std::optional<double>(Real(upper, "upper"));
        s.ordinarySteps_ = Integer(ordinarySteps, "ordinary_steps");
        s.theta_ = Real(theta, "theta");
        s.rannacherIntervals_ = Integer(rannacherIntervals, "rannacher_intervals");
        s.penaltyFactor_ = Real(penaltyFactor, "penalty_factor");
        s.absoluteTolerance_ = Real(absoluteTolerance, "absolute_tolerance");
        s.relativeTolerance_ = Real(relativeTolerance, "relative_tolerance");
        s.linearBackwardErrorLimit_ = Real(linearBackwardErrorLimit, "linear_backward_error_limit");
        s.maxIterationsPerStep_ = Integer(maxIterationsPerStep, "max_iterations_per_step");
        s.maxActualSteps_ = Integer(maxActualSteps, "max_actual_steps");
        s.maxWorkUnits_ = RequestUnsigned(maxWorkUnits, {"max_work_units", IDENTIFIER});
        s.numericPayloadBudgetBytes_ = RequestUnsigned(numericPayloadBudgetBytes, {"numeric_payload_budget_bytes", IDENTIFIER});
        s.workspaceBudgetBytes_ = RequestUnsigned(workspaceBudgetBytes, {"workspace_budget_bytes", IDENTIFIER});
        ValidateEarlyExercisePdeSettings(s);
        return s;
    }
} // namespace

void init_bindings_earlyexercisepde(py::module_& m) {
    py::enum_<EarlyExerciseStyle_::Value_>(m, "EarlyExerciseStyle_")
        .value("BERMUDAN", EarlyExerciseStyle_::Value_::BERMUDAN)
        .value("AMERICAN", EarlyExerciseStyle_::Value_::AMERICAN);
    py::enum_<OptionType_::Value_>(m, "OptionType_").value("CALL", OptionType_::Value_::CALL).value("PUT", OptionType_::Value_::PUT);
    auto settings = WithCopies(py::class_<EarlyExercisePdeSettings_>(m, "EarlyExercisePdeSettings_"));
    settings.def(py::init(&NewSettings), py::kw_only(), py::arg("grid_points") = 401, py::arg("upper") = py::none(), py::arg("ordinary_steps") = 500,
                 py::arg("theta") = 1, py::arg("rannacher_intervals") = 2, py::arg("penalty_factor") = 100000000,
                 py::arg("absolute_tolerance") = 1e-8, py::arg("relative_tolerance") = 1e-10, py::arg("linear_backward_error_limit") = 1e-12,
                 py::arg("max_iterations_per_step") = 100, py::arg("max_actual_steps") = 100000,
                 py::arg("max_work_units") = py::int_(size_t(100000000)), py::arg("numeric_payload_budget_bytes") = py::int_(size_t(67108864)),
                 py::arg("workspace_budget_bytes") = py::int_(size_t(268435456)));
    ReadOnly(&settings, "grid_points", &EarlyExercisePdeSettings_::gridPoints_);
    ReadOnly(&settings, "upper", &EarlyExercisePdeSettings_::upper_);
    ReadOnly(&settings, "ordinary_steps", &EarlyExercisePdeSettings_::ordinarySteps_);
    ReadOnly(&settings, "theta", &EarlyExercisePdeSettings_::theta_);
    ReadOnly(&settings, "rannacher_intervals", &EarlyExercisePdeSettings_::rannacherIntervals_);
    ReadOnly(&settings, "penalty_factor", &EarlyExercisePdeSettings_::penaltyFactor_);
    ReadOnly(&settings, "absolute_tolerance", &EarlyExercisePdeSettings_::absoluteTolerance_);
    ReadOnly(&settings, "relative_tolerance", &EarlyExercisePdeSettings_::relativeTolerance_);
    ReadOnly(&settings, "linear_backward_error_limit", &EarlyExercisePdeSettings_::linearBackwardErrorLimit_);
    ReadOnly(&settings, "max_iterations_per_step", &EarlyExercisePdeSettings_::maxIterationsPerStep_);
    ReadOnly(&settings, "max_actual_steps", &EarlyExercisePdeSettings_::maxActualSteps_);
    ReadOnly(&settings, "max_work_units", &EarlyExercisePdeSettings_::maxWorkUnits_);
    ReadOnly(&settings, "numeric_payload_budget_bytes", &EarlyExercisePdeSettings_::numericPayloadBudgetBytes_);
    ReadOnly(&settings, "workspace_budget_bytes", &EarlyExercisePdeSettings_::workspaceBudgetBytes_);
    auto segment = WithCopies(py::class_<EarlyExercisePdeCarrySegment_>(m, "EarlyExercisePdeCarrySegment_"));
    segment.def(py::init([](const py::object& date, const py::object& rate, const py::object& yield) {
                    return EarlyExercisePdeCarrySegment_{DateInput(date, "carry.end_date"), Real(rate, "carry.rate"),
                                                         Real(yield, "carry.dividend_yield")};
                }),
                py::arg("end_date"), py::arg("rate"), py::arg("dividend_yield"));
    ReadOnly(&segment, "end_date", &EarlyExercisePdeCarrySegment_::endDate_);
    ReadOnly(&segment, "rate", &EarlyExercisePdeCarrySegment_::rate_);
    ReadOnly(&segment, "dividend_yield", &EarlyExercisePdeCarrySegment_::dividendYield_);
    auto market = WithCopies(py::class_<EarlyExercisePdeMarket_>(m, "EarlyExercisePdeMarket_"));
    market.def(py::init([](const py::object& spot, const py::object& vol, const py::object& segments) {
                   EarlyExercisePdeMarket_ result;
                   result.spot_ = Real(spot, "market.spot");
                   result.volatility_ = Real(vol, "market.volatility");
                   for (const auto& value : RequestSequence(segments, {"market.carry", IDENTIFIER}))
                       result.carry_.push_back(
                           RequiredRiskInput<EarlyExercisePdeCarrySegment_>(value, "market.carry", "EarlyExercisePdeCarrySegment_", IDENTIFIER));
                   return result;
               }),
               py::arg("spot"), py::arg("volatility"), py::arg("carry"));
    ReadOnly(&market, "spot", &EarlyExercisePdeMarket_::spot_);
    ReadOnly(&market, "volatility", &EarlyExercisePdeMarket_::volatility_);
    market.def_property_readonly("carry", [](const EarlyExercisePdeMarket_& x) { return CopyRiskVector(x.carry_); });
    auto contract = WithCopies(py::class_<EarlyExercisePdeContract_>(m, "EarlyExercisePdeContract_"));
    contract.def(py::init([](const py::object& today, const py::object& maturity, const py::object& strike, const py::object& option,
                             const py::object& style, const py::object& dates) {
                     EarlyExercisePdeContract_ result;
                     result.evaluationDate_ = DateInput(today, "contract.evaluation_date");
                     result.maturityDate_ = DateInput(maturity, "contract.maturity_date");
                     result.strike_ = Real(strike, "contract.strike");
                     result.optionType_ = EnumInput<OptionType_>(option, "contract.option_type", "CALL", "PUT");
                     result.exerciseStyle_ = EnumInput<EarlyExerciseStyle_>(style, "contract.exercise_style", "BERMUDAN", "AMERICAN");
                     for (const auto& value : RequestSequence(dates, {"contract.exercise_dates", IDENTIFIER}))
                         result.exerciseDates_.push_back(DateInput(value, "contract.exercise_dates"));
                     return result;
                 }),
                 py::arg("evaluation_date"), py::arg("maturity_date"), py::arg("strike"), py::arg("option_type"), py::arg("exercise_style"),
                 py::kw_only(), py::arg("exercise_dates") = py::tuple());
    ReadOnly(&contract, "evaluation_date", &EarlyExercisePdeContract_::evaluationDate_);
    ReadOnly(&contract, "maturity_date", &EarlyExercisePdeContract_::maturityDate_);
    ReadOnly(&contract, "strike", &EarlyExercisePdeContract_::strike_);
    contract.def_property_readonly("option_type", [](const EarlyExercisePdeContract_& x) { return std::string(x.optionType_.String()); });
    contract.def_property_readonly("exercise_style", [](const EarlyExercisePdeContract_& x) { return std::string(x.exerciseStyle_.String()); });
    contract.def_property_readonly("exercise_dates", [](const EarlyExercisePdeContract_& x) { return CopyRiskVector(x.exerciseDates_); });
    auto request = WithCopies(py::class_<EarlyExercisePdeRequest_>(m, "EarlyExercisePdeRequest_"));
    request.def(py::init([](const EarlyExercisePdeContract_& contract, const EarlyExercisePdeMarket_& market, const py::object& settings) {
                    EarlyExercisePdeRequest_ result{contract, market,
                                                    SettingsInput<EarlyExercisePdeSettings_>(settings, "settings", "EarlyExercisePdeSettings_")};
                    ValidateEarlyExercisePdeRequest(result);
                    return result;
                }),
                py::arg("contract"), py::arg("market"), py::kw_only(), py::arg("settings") = py::none());
    ReadOnly(&request, "contract", &EarlyExercisePdeRequest_::contract_);
    ReadOnly(&request, "market", &EarlyExercisePdeRequest_::market_);
    ReadOnly(&request, "settings", &EarlyExercisePdeRequest_::settings_);
    auto timenode = WithCopies(py::class_<EarlyExercisePdeTimeNode_>(m, "EarlyExercisePdeTimeNode_"));
    ReadOnly(&timenode, "time_years", &EarlyExercisePdeTimeNode_::timeYears_);
    ReadOnly(&timenode, "calendar_date", &EarlyExercisePdeTimeNode_::calendarDate_);
    ReadOnly(&timenode, "is_exercise_date", &EarlyExercisePdeTimeNode_::isExerciseDate_);
    ReadOnly(&timenode, "is_carry_knot", &EarlyExercisePdeTimeNode_::isCarryKnot_);
    ReadOnly(&timenode, "is_maturity", &EarlyExercisePdeTimeNode_::isMaturity_);
    auto stepdiagnostic = WithCopies(py::class_<EarlyExercisePdeStepDiagnostic_>(m, "EarlyExercisePdeStepDiagnostic_"));
    ReadOnly(&stepdiagnostic, "rollback_step", &EarlyExercisePdeStepDiagnostic_::rollbackStep_);
    ReadOnly(&stepdiagnostic, "source_time_index", &EarlyExercisePdeStepDiagnostic_::sourceTimeIndex_);
    ReadOnly(&stepdiagnostic, "time_index", &EarlyExercisePdeStepDiagnostic_::timeIndex_);
    ReadOnly(&stepdiagnostic, "solve_count", &EarlyExercisePdeStepDiagnostic_::solveCount_);
    ReadOnly(&stepdiagnostic, "dt", &EarlyExercisePdeStepDiagnostic_::dt_);
    ReadOnly(&stepdiagnostic, "theta", &EarlyExercisePdeStepDiagnostic_::theta_);
    ReadOnly(&stepdiagnostic, "iteration_change", &EarlyExercisePdeStepDiagnostic_::iterationChange_);
    ReadOnly(&stepdiagnostic, "damped", &EarlyExercisePdeStepDiagnostic_::damped_);
    ReadOnly(&stepdiagnostic, "projection", &EarlyExercisePdeStepDiagnostic_::projection_);
    ReadOnly(&stepdiagnostic, "completed", &EarlyExercisePdeStepDiagnostic_::completed_);
    ReadOnly(&stepdiagnostic, "linear_backward_error", &EarlyExercisePdeStepDiagnostic_::linearBackwardError_);
    ReadOnly(&stepdiagnostic, "projection_change", &EarlyExercisePdeStepDiagnostic_::projectionChange_);
    ReadOnly(&stepdiagnostic, "projection_residual", &EarlyExercisePdeStepDiagnostic_::projectionResidual_);
    ReadOnly(&stepdiagnostic, "obstacle_violation", &EarlyExercisePdeStepDiagnostic_::obstacleViolation_);
    ReadOnly(&stepdiagnostic, "dual_violation", &EarlyExercisePdeStepDiagnostic_::dualViolation_);
    ReadOnly(&stepdiagnostic, "complementarity_residual", &EarlyExercisePdeStepDiagnostic_::complementarityResidual_);
    ReadOnly(&stepdiagnostic, "normalized_obstacle_violation", &EarlyExercisePdeStepDiagnostic_::normalizedObstacleViolation_);
    ReadOnly(&stepdiagnostic, "normalized_dual_violation", &EarlyExercisePdeStepDiagnostic_::normalizedDualViolation_);
    ReadOnly(&stepdiagnostic, "normalized_complementarity_residual", &EarlyExercisePdeStepDiagnostic_::normalizedComplementarityResidual_);
    auto exerciselevel = WithCopies(py::class_<EarlyExercisePdeExerciseLevel_>(m, "EarlyExercisePdeExerciseLevel_"));
    ReadOnly(&exerciselevel, "time_index", &EarlyExercisePdeExerciseLevel_::timeIndex_);
    ReadOnly(&exerciselevel, "calendar_date", &EarlyExercisePdeExerciseLevel_::calendarDate_);
    ReadOnly(&exerciselevel, "region_offset", &EarlyExercisePdeExerciseLevel_::regionOffset_);
    ReadOnly(&exerciselevel, "region_count", &EarlyExercisePdeExerciseLevel_::regionCount_);
    ReadOnly(&exerciselevel, "epsilon", &EarlyExercisePdeExerciseLevel_::epsilon_);
    exerciselevel.def_property_readonly("kind", [](const EarlyExercisePdeExerciseLevel_& x) { return std::string(x.kind_.String()); });
    auto exerciseregion = WithCopies(py::class_<EarlyExercisePdeExerciseRegion_>(m, "EarlyExercisePdeExerciseRegion_"));
    ReadOnly(&exerciseregion, "time_index", &EarlyExercisePdeExerciseRegion_::timeIndex_);
    ReadOnly(&exerciseregion, "region_index", &EarlyExercisePdeExerciseRegion_::regionIndex_);
    ReadOnly(&exerciseregion, "first_node", &EarlyExercisePdeExerciseRegion_::firstNode_);
    ReadOnly(&exerciseregion, "last_node", &EarlyExercisePdeExerciseRegion_::lastNode_);
    ReadOnly(&exerciseregion, "left_clipped", &EarlyExercisePdeExerciseRegion_::leftClipped_);
    ReadOnly(&exerciseregion, "right_clipped", &EarlyExercisePdeExerciseRegion_::rightClipped_);
    auto execution = WithCopies(py::class_<EarlyExercisePdeExecution_>(m, "EarlyExercisePdeExecution_"));
    ReadOnly(&execution, "planned_steps", &EarlyExercisePdeExecution_::plannedSteps_);
    ReadOnly(&execution, "actual_steps", &EarlyExercisePdeExecution_::actualSteps_);
    ReadOnly(&execution, "completed_steps", &EarlyExercisePdeExecution_::completedSteps_);
    ReadOnly(&execution, "base_steps", &EarlyExercisePdeExecution_::baseSteps_);
    ReadOnly(&execution, "damping_added_steps", &EarlyExercisePdeExecution_::dampingAddedSteps_);
    ReadOnly(&execution, "stability_added_steps", &EarlyExercisePdeExecution_::stabilityAddedSteps_);
    ReadOnly(&execution, "effective_rannacher_intervals", &EarlyExercisePdeExecution_::effectiveRannacherIntervals_);
    ReadOnly(&execution, "max_iterations", &EarlyExercisePdeExecution_::maxIterations_);
    ReadOnly(&execution, "upwind_rows", &EarlyExercisePdeExecution_::upwindRows_);
    ReadOnly(&execution, "total_solves", &EarlyExercisePdeExecution_::totalSolves_);
    ReadOnly(&execution, "work_units", &EarlyExercisePdeExecution_::workUnits_);
    ReadOnly(&execution, "planned_region_capacity", &EarlyExercisePdeExecution_::plannedRegionCapacity_);
    ReadOnly(&execution, "region_capacity", &EarlyExercisePdeExecution_::regionCapacity_);
    ReadOnly(&execution, "planned_numeric_payload_bytes", &EarlyExercisePdeExecution_::plannedNumericPayloadBytes_);
    ReadOnly(&execution, "planned_workspace_bytes", &EarlyExercisePdeExecution_::plannedWorkspaceBytes_);
    ReadOnly(&execution, "numeric_payload_bytes", &EarlyExercisePdeExecution_::numericPayloadBytes_);
    ReadOnly(&execution, "peak_workspace_bytes", &EarlyExercisePdeExecution_::peakWorkspaceBytes_);
    ReadOnly(&execution, "epsilon", &EarlyExercisePdeExecution_::epsilon_);
    ReadOnly(&execution, "max_linear_backward_error", &EarlyExercisePdeExecution_::maxLinearBackwardError_);
    ReadOnly(&execution, "max_obstacle_violation", &EarlyExercisePdeExecution_::maxObstacleViolation_);
    ReadOnly(&execution, "max_dual_violation", &EarlyExercisePdeExecution_::maxDualViolation_);
    ReadOnly(&execution, "max_complementarity_residual", &EarlyExercisePdeExecution_::maxComplementarityResidual_);
    auto failure = WithCopies(py::class_<EarlyExercisePdeFailure_>(m, "EarlyExercisePdeFailure_"));
    ReadOnly(&failure, "time_index", &EarlyExercisePdeFailure_::timeIndex_);
    ReadOnly(&failure, "iteration", &EarlyExercisePdeFailure_::iteration_);
    ReadOnly(&failure, "calendar_date", &EarlyExercisePdeFailure_::calendarDate_);
    ReadOnly(&failure, "node", &EarlyExercisePdeFailure_::node_);
    ReadOnly(&failure, "residual", &EarlyExercisePdeFailure_::residual_);
    ReadOnly(&failure, "threshold", &EarlyExercisePdeFailure_::threshold_);
    ReadOnly(&failure, "work_units", &EarlyExercisePdeFailure_::workUnits_);
    failure.def_property_readonly("status", [](const EarlyExercisePdeFailure_& x) { return std::string(x.status_.String()); });
    failure.def_property_readonly("reason", [](const EarlyExercisePdeFailure_& x) { return Text(x.reason_); });
    WithCopies(py::class_<EarlyExercisePdeResult_>(m, "EarlyExercisePdeResult_"))
        .def_property_readonly("status", [](const EarlyExercisePdeResult_& x) { return std::string(x.Status().String()); })
        .def_property_readonly("is_success", &EarlyExercisePdeResult_::IsSuccess)
        .def_property_readonly("price", &EarlyExercisePdeResult_::Price)
        .def_property_readonly("method", [](const EarlyExercisePdeResult_& x) { return Text(x.Method()); })
        .def_property_readonly("request", [](const EarlyExercisePdeResult_& x) { return x.Request(); })
        .def_property_readonly("grid", [](const EarlyExercisePdeResult_& x) { return CopyRiskVector(x.Grid()); })
        .def_property_readonly("values", [](const EarlyExercisePdeResult_& x) { return CopyRiskVector(x.Values()); })
        .def_property_readonly("time_grid", [](const EarlyExercisePdeResult_& x) { return CopyRiskVector(x.TimeGrid()); })
        .def_property_readonly("step_diagnostics", [](const EarlyExercisePdeResult_& x) { return CopyRiskVector(x.StepDiagnostics()); })
        .def_property_readonly("exercise_levels", [](const EarlyExercisePdeResult_& x) { return CopyRiskVector(x.ExerciseLevels()); })
        .def_property_readonly("exercise_regions", [](const EarlyExercisePdeResult_& x) { return CopyRiskVector(x.ExerciseRegions()); })
        .def_property_readonly("execution", [](const EarlyExercisePdeResult_& x) { return x.Execution(); })
        .def_property_readonly("failure", [](const EarlyExercisePdeResult_& x) { return x.Failure(); });
    m.def(
        "EarlyExercisePdeResult_New",
        [](const EarlyExercisePdeRequest_& value) {
            const auto request = value;
            py::gil_scoped_release release;
            return EvaluateEarlyExercisePde(request);
        },
        py::arg("request"));
}

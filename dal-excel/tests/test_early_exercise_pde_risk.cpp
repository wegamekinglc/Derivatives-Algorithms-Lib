#include <gtest/gtest.h>

#include <array>
#include <limits>

#include <dal-excel/src/__earlyexercisepderisk.hpp>
#include <dal-excel/src/__earlyexercisepderiskinput.hpp>
#include <dal/math/aad/native.hpp>
#include <dal/math/aad/recording.hpp>
#include <dal/math/pde/earlyexercisepderiskinternal.hpp>
#include <dal/storage/json.hpp>

#ifdef _WIN32
#include <Windows.h>
#include <dal-excel/src/__script_test_api.hpp>
#endif

using namespace Dal;

#ifdef _WIN32
namespace {
    EarlyExercisePdeRequest_ Valuation();
    struct NativeRiskText_ {
        std::vector<wchar_t> text_;
        OPER_ cell_{};
        explicit NativeRiskText_(const std::wstring& value) : text_(1, wchar_t(value.size())) {
            text_.insert(text_.end(), value.begin(), value.end());
            cell_.xltype = xltypeStr;
            cell_.val.str = text_.data();
        }
    };
    template <class T_> NativeRiskText_ NativeRiskHandle(const Handle_<T_>& value) {
        const auto tag = Excel::ScriptTestStoreRepository(Handle_<Storable_>(value));
        return NativeRiskText_(std::wstring(tag.begin(), tag.end()));
    }
    template <class... A_> OPER_* NativeRiskCall(const char* name, A_... args) {
        const auto address = GetProcAddress(GetModuleHandleA("dal_excel.xll"), name);
        REQUIRE(address, "missing generated risk export");
        return reinterpret_cast<OPER_* (*)(A_...)>(address)(args...);
    }
    std::string NativeRiskOutput(OPER_* value) {
        const auto release = reinterpret_cast<void (*)(OPER_*)>(GetProcAddress(GetModuleHandleA("dal_excel.xll"), "xlAutoFree12"));
        REQUIRE(value && release, "missing native result/release");
        const std::unique_ptr<OPER_, decltype(release)> owned(value, release);
        REQUIRE((value->xltype & xltypeMulti) && value->val.array.rows == 1 && value->val.array.columns == 1, "expected scalar");
        const auto& scalar = value->val.array.lparray[0];
        REQUIRE(scalar.xltype == xltypeStr, "expected text output");
        std::string result;
        for (int i = 0; i < scalar.val.str[0]; ++i)
            result += static_cast<char>(scalar.val.str[i + 1]);
        return result;
    }
    void NativeRiskError(const std::string& message, const char* field) {
        ASSERT_EQ(message.substr(0, 7), "#Error:") << message;
        ASSERT_NE(message.find("InvalidEarlyExercisePdeRiskRequest"), std::string::npos) << message;
        ASSERT_NE(message.find(field), std::string::npos) << message;
    }
    Handle_<StorableEarlyExercisePdeRiskResult_> NativeRiskResult() {
        auto price = Valuation();
        price.settings_.gridPoints_ = 401;
        price.settings_.ordinarySteps_ = 500;
        return Handle_<StorableEarlyExercisePdeRiskResult_>(new StorableEarlyExercisePdeRiskResult_("", EvaluateEarlyExercisePdeRisk({price, {}})));
    }
} // namespace

TEST(ExcelEarlyExercisePdeRiskTest, TestGeneratedNativeEntriesRejectNonAsciiCanonicalText) {
    Excel::ScriptTestInitialize(1);
    NativeRiskText_ name(L"raw-risk"), key(L"\u0164elta"), id(L"\u0144elta");
    OPER_ blank{}, cells[2]{key.cell_, {}}, table{};
    blank.xltype = xltypeNil;
    cells[1].xltype = xltypeBool;
    cells[1].val.xbool = 0;
    table.xltype = xltypeMulti;
    table.val.array.lparray = cells;
    table.val.array.rows = 1;
    table.val.array.columns = 2;
    ASSERT_NO_FATAL_FAILURE(
        NativeRiskError(NativeRiskOutput(NativeRiskCall("xl_EarlyExercisePdeRiskSettings_New", &name.cell_, &table, &blank)), "row=1"));
    const auto handle = NativeRiskHandle(NativeRiskResult());
    ASSERT_NO_FATAL_FAILURE(
        NativeRiskError(NativeRiskOutput(NativeRiskCall("xl_EarlyExercisePdeRiskResult_Get_RiskValue", &handle.cell_, &id.cell_)), "risk_id"));
}

TEST(ExcelEarlyExercisePdeRiskTest, TestGeneratedNativeEntriesPreserveConversionErrorContext) {
    Excel::ScriptTestInitialize(1);
    const auto result = NativeRiskHandle(NativeRiskResult());
    NativeRiskText_ name(L"raw-risk"), id(L"Delta");
    OPER_ blank{}, numeric{}, boolean{}, error{};
    blank.xltype = xltypeNil;
    numeric.xltype = xltypeNum;
    numeric.val.num = 1.;
    boolean.xltype = xltypeBool;
    boolean.val.xbool = 1;
    error.xltype = xltypeErr;
    error.val.err = xlerrNA;
    for (const auto* invalid : {&numeric, &boolean, &error}) {
        ASSERT_NO_FATAL_FAILURE(
            NativeRiskError(NativeRiskOutput(NativeRiskCall("xl_EarlyExercisePdeRiskSettings_New", invalid, &blank, &blank)), "name"));
        ASSERT_NO_FATAL_FAILURE(
            NativeRiskError(NativeRiskOutput(NativeRiskCall("xl_EarlyExercisePdeRiskResult_Get_RiskValue", &result.cell_, invalid)), "risk_id"));
    }
    Handle_<StorableEarlyExercisePdeRiskSettings_> settings(new StorableEarlyExercisePdeRiskSettings_("", {}));
    const auto wrong = NativeRiskHandle(settings);
    const Handle_<StorableEarlyExercisePdeRequest_> valuation(new StorableEarlyExercisePdeRequest_("", Valuation()));
    const auto price = NativeRiskHandle(valuation);
    ASSERT_NO_FATAL_FAILURE(
        NativeRiskError(NativeRiskOutput(NativeRiskCall("xl_EarlyExercisePdeRiskResult_Get_RiskValue", &wrong.cell_, &id.cell_)), "result"));
    ASSERT_NO_FATAL_FAILURE(
        NativeRiskError(NativeRiskOutput(NativeRiskCall("xl_EarlyExercisePdeRiskResult_New", &name.cell_, &wrong.cell_)), "request"));
    ASSERT_NO_FATAL_FAILURE(
        NativeRiskError(NativeRiskOutput(NativeRiskCall("xl_EarlyExercisePdeRiskSettings_Get_Configuration", &result.cell_)), "settings"));
    ASSERT_NO_FATAL_FAILURE(
        NativeRiskError(NativeRiskOutput(NativeRiskCall("xl_EarlyExercisePdeRiskRequest_New", &name.cell_, &wrong.cell_, &blank)), "valuation"));
    ASSERT_NO_FATAL_FAILURE(
        NativeRiskError(NativeRiskOutput(NativeRiskCall("xl_EarlyExercisePdeRiskRequest_New", &name.cell_, &price.cell_, &result.cell_)), "risk"));
}
#endif

TEST(ExcelEarlyExercisePdeRiskTest, TestSettingsDefaultsAndAtomicFailure) {
    Handle_<StorableEarlyExercisePdeRiskSettings_> settings;
    EarlyExercisePdeRiskSettings_New("settings", {}, {}, &settings);
    ASSERT_TRUE(settings->val_.delta_);
    ASSERT_EQ(settings->val_.maxScenarios_, 129);
    const auto previous = settings;
    Matrix_<Cell_> rows(1, 2);
    rows(0, 0) = "delta";
    rows(0, 1) = 1.0;
    ASSERT_THROW(EarlyExercisePdeRiskSettings_New("invalid", rows, {}, &settings), Exception_);
    ASSERT_EQ(settings, previous);
    Matrix_<Cell_> table;
    EarlyExercisePdeRiskSettings_Get_Configuration(settings, &table);
    ASSERT_EQ(table.Rows(), 16);
    ASSERT_EQ(Cell::ToString(table(0, 0)), "Key");
}

namespace {
    Matrix_<Cell_> Setting(const char* key, const Cell_& value) {
        Matrix_<Cell_> rows(1, 2);
        rows(0, 0) = key;
        rows(0, 1) = value;
        return rows;
    }
    EarlyExercisePdeRequest_ Valuation() {
        EarlyExercisePdeRequest_ value;
        const Date_ today(2026, 10, 18), maturity(2027, 10, 18);
        value.contract_ = {today, maturity, 100.0, OptionType_("PUT"), EarlyExerciseStyle_("BERMUDAN"), {maturity}};
        value.market_ = {100.0, 0.2, {{maturity, 0.05, 0.03}}};
        value.settings_.gridPoints_ = 41;
        value.settings_.upper_ = 400.0;
        value.settings_.ordinarySteps_ = 2;
        value.settings_.rannacherIntervals_ = 0;
        return value;
    }
    Handle_<StorableEarlyExercisePdeRiskRequest_> Request(const EarlyExercisePdeRequest_& valuation, const EarlyExercisePdeRiskSettings_& risk = {}) {
        Handle_<StorableEarlyExercisePdeRequest_> price(new StorableEarlyExercisePdeRequest_("price", valuation));
        Handle_<StorableEarlyExercisePdeRiskSettings_> settings(new StorableEarlyExercisePdeRiskSettings_("risk", risk));
        Handle_<StorableEarlyExercisePdeRiskRequest_> request;
        EarlyExercisePdeRiskRequest_New("request", price, settings, &request);
        return request;
    }
    void Header(const Matrix_<Cell_>& table, std::initializer_list<const char*> labels) {
        ASSERT_EQ(table.Cols(), labels.size());
        int col = 0;
        for (const auto* label : labels)
            ASSERT_EQ(std::string(Cell::ToString(table(0, col++)).c_str()), label);
    }
    template <class F_> void Error(F_ action, const char* field) {
        try {
            action();
            FAIL() << "expected input rejection";
        } catch (const Exception_& error) {
            ASSERT_NE(std::string(error.what()).find("InvalidEarlyExercisePdeRiskRequest"), std::string::npos) << error.what();
            ASSERT_NE(std::string(error.what()).find(field), std::string::npos) << error.what();
        }
    }
    void Same(const Cell_& cell, double value) { ASSERT_DOUBLE_EQ(Cell::ToDouble(cell), value); }
    void Same(const Cell_& cell, size_t value) { Same(cell, double(value)); }
    void Same(const Cell_& cell, int value) { Same(cell, double(value)); }
    void Same(const Cell_& cell, bool value) {
        ASSERT_TRUE(std::holds_alternative<bool>(cell.val_));
        ASSERT_EQ(std::get<bool>(cell.val_), value);
    }
    void Same(const Cell_& cell, const String_& value) { ASSERT_EQ(std::string(Cell::ToString(cell).c_str()), std::string(value.c_str())); }
    void Same(const Cell_& cell, const char* value) { Same(cell, String_(value)); }
    void Same(const Cell_& cell, const Date_& value) { Same(cell, double(Date::ToExcel(value))); }
    template <class T_> auto Same(const Cell_& cell, const T_& value) -> decltype(value.String(), void()) { Same(cell, value.String()); }
    template <class T_> void Same(const Cell_& cell, const std::optional<T_>& value) {
        ASSERT_EQ(Cell::IsEmpty(cell), !value);
        if (value)
            Same(cell, *value);
    }
    template <class T_, class V_> void SameMember(const Cell_& cell, const std::optional<T_>& value, V_ T_::* member) {
        if (value)
            ASSERT_NO_FATAL_FAILURE(Same(cell, (*value).*member));
        else
            ASSERT_TRUE(Cell::IsEmpty(cell));
    }
    void RisksParity(const Handle_<StorableEarlyExercisePdeRiskResult_>& result, const EarlyExercisePdeRiskResult_& expected) {
        Matrix_<Cell_> table;
        EarlyExercisePdeRiskResult_Get_Risks(result, &table);
        ASSERT_NO_FATAL_FAILURE(Header(
            table,
            {"RiskIndex",         "RiskId",     "ParameterId", "DerivativeOrder", "SegmentIndex",   "StartDate",      "EndDate",
             "ParameterValue",    "Unit",       "Value",       "Status",          "Reason",         "Method",         "Interpretation",
             "FineBump",          "CoarseBump", "BumpOrigin",  "FineEstimate",    "CoarseEstimate", "StepDifference", "NormalizedStepDifference",
             "StabilityTolerance"}));
        ASSERT_EQ(table.Rows(), expected.Risks().size() + 1);
        for (size_t i = 0; i < expected.Risks().size(); ++i) {
            const auto& value = expected.Risks()[i];
            ASSERT_NO_FATAL_FAILURE(Same(table(static_cast<int>(i) + 1, 0), value.riskIndex_));
            ASSERT_NO_FATAL_FAILURE(Same(table(static_cast<int>(i) + 1, 1), value.riskId_));
            ASSERT_NO_FATAL_FAILURE(Same(table(static_cast<int>(i) + 1, 2), value.parameterId_));
            ASSERT_NO_FATAL_FAILURE(Same(table(static_cast<int>(i) + 1, 3), value.derivativeOrder_));
            ASSERT_NO_FATAL_FAILURE(Same(table(static_cast<int>(i) + 1, 4), value.segmentIndex_));
            ASSERT_NO_FATAL_FAILURE(Same(table(static_cast<int>(i) + 1, 5), value.startDate_));
            ASSERT_NO_FATAL_FAILURE(Same(table(static_cast<int>(i) + 1, 6), value.endDate_));
            ASSERT_NO_FATAL_FAILURE(Same(table(static_cast<int>(i) + 1, 7), value.parameterValue_));
            ASSERT_NO_FATAL_FAILURE(Same(table(static_cast<int>(i) + 1, 8), value.unit_));
            ASSERT_NO_FATAL_FAILURE(Same(table(static_cast<int>(i) + 1, 9), value.value_));
            ASSERT_NO_FATAL_FAILURE(Same(table(static_cast<int>(i) + 1, 10), value.status_));
            ASSERT_NO_FATAL_FAILURE(Same(table(static_cast<int>(i) + 1, 11), value.reason_));
            ASSERT_NO_FATAL_FAILURE(Same(table(static_cast<int>(i) + 1, 12), value.method_));
            ASSERT_NO_FATAL_FAILURE(Same(table(static_cast<int>(i) + 1, 13), value.interpretation_));
            ASSERT_NO_FATAL_FAILURE(Same(table(static_cast<int>(i) + 1, 14), value.fineBump_));
            ASSERT_NO_FATAL_FAILURE(Same(table(static_cast<int>(i) + 1, 15), value.coarseBump_));
            ASSERT_NO_FATAL_FAILURE(Same(table(static_cast<int>(i) + 1, 16), value.bumpOrigin_));
            ASSERT_NO_FATAL_FAILURE(Same(table(static_cast<int>(i) + 1, 17), value.fineEstimate_));
            ASSERT_NO_FATAL_FAILURE(Same(table(static_cast<int>(i) + 1, 18), value.coarseEstimate_));
            ASSERT_NO_FATAL_FAILURE(Same(table(static_cast<int>(i) + 1, 19), value.stepDifference_));
            ASSERT_NO_FATAL_FAILURE(Same(table(static_cast<int>(i) + 1, 20), value.normalizedStepDifference_));
            ASSERT_NO_FATAL_FAILURE(Same(table(static_cast<int>(i) + 1, 21), value.stabilityTolerance_));
        }
    }
    void ScenariosParity(const Handle_<StorableEarlyExercisePdeRiskResult_>& result, const EarlyExercisePdeRiskResult_& expected) {
        Matrix_<Cell_> table;
        EarlyExercisePdeRiskResult_Get_Scenarios(result, &table);
        ASSERT_NO_FATAL_FAILURE(Header(table, {"ScenarioIndex",
                                               "ScenarioId",
                                               "PlanId",
                                               "ParameterId",
                                               "SegmentIndex",
                                               "ParameterValue",
                                               "Offset",
                                               "Status",
                                               "SolverStatus",
                                               "WorkLimitSource",
                                               "PriceAvailable",
                                               "Price",
                                               "Epsilon",
                                               "PlannedSteps",
                                               "ActualSteps",
                                               "CompletedSteps",
                                               "TotalSolves",
                                               "WorkUnits",
                                               "NumericPayloadBytes",
                                               "PeakWorkspaceBytes",
                                               "MaxIterationChange",
                                               "MaxLinearBackwardError",
                                               "MaxObstacleViolation",
                                               "MaxDualViolation",
                                               "MaxComplementarityResidual",
                                               "FailureStatus",
                                               "FailureTimeIndex",
                                               "FailureIteration",
                                               "FailureCalendarDate",
                                               "FailureNode",
                                               "FailureResidual",
                                               "FailureThreshold",
                                               "FailureWorkUnits",
                                               "FailureReason",
                                               "EvaluationMembership",
                                               "EvaluationBoundaryIntersectsSpotStencil",
                                               "FutureContactChanged",
                                               "UpwindChanged"}));
        ASSERT_EQ(table.Rows(), expected.Scenarios().size() + 1);
        for (size_t i = 0; i < expected.Scenarios().size(); ++i) {
            const auto& value = expected.Scenarios()[i];
            ASSERT_NO_FATAL_FAILURE(Same(table(static_cast<int>(i) + 1, 0), value.scenarioIndex_));
            ASSERT_NO_FATAL_FAILURE(Same(table(static_cast<int>(i) + 1, 1), value.scenarioId_));
            ASSERT_NO_FATAL_FAILURE(Same(table(static_cast<int>(i) + 1, 2), value.planId_));
            ASSERT_NO_FATAL_FAILURE(Same(table(static_cast<int>(i) + 1, 3), value.parameterId_));
            ASSERT_NO_FATAL_FAILURE(Same(table(static_cast<int>(i) + 1, 4), value.segmentIndex_));
            ASSERT_NO_FATAL_FAILURE(Same(table(static_cast<int>(i) + 1, 5), value.parameterValue_));
            ASSERT_NO_FATAL_FAILURE(Same(table(static_cast<int>(i) + 1, 6), value.offset_));
            ASSERT_NO_FATAL_FAILURE(Same(table(static_cast<int>(i) + 1, 7), value.status_));
            ASSERT_NO_FATAL_FAILURE(Same(table(static_cast<int>(i) + 1, 8), value.solverStatus_));
            ASSERT_NO_FATAL_FAILURE(Same(table(static_cast<int>(i) + 1, 9), value.workLimitSource_));
            ASSERT_NO_FATAL_FAILURE(Same(table(static_cast<int>(i) + 1, 10), value.priceAvailable_));
            ASSERT_NO_FATAL_FAILURE(Same(table(static_cast<int>(i) + 1, 11), value.price_));
            ASSERT_NO_FATAL_FAILURE(Same(table(static_cast<int>(i) + 1, 12), value.epsilon_));
            ASSERT_NO_FATAL_FAILURE(Same(table(static_cast<int>(i) + 1, 13), value.plannedSteps_));
            ASSERT_NO_FATAL_FAILURE(Same(table(static_cast<int>(i) + 1, 14), value.actualSteps_));
            ASSERT_NO_FATAL_FAILURE(Same(table(static_cast<int>(i) + 1, 15), value.completedSteps_));
            ASSERT_NO_FATAL_FAILURE(Same(table(static_cast<int>(i) + 1, 16), value.totalSolves_));
            ASSERT_NO_FATAL_FAILURE(Same(table(static_cast<int>(i) + 1, 17), value.workUnits_));
            ASSERT_NO_FATAL_FAILURE(Same(table(static_cast<int>(i) + 1, 18), value.numericPayloadBytes_));
            ASSERT_NO_FATAL_FAILURE(Same(table(static_cast<int>(i) + 1, 19), value.peakWorkspaceBytes_));
            ASSERT_NO_FATAL_FAILURE(Same(table(static_cast<int>(i) + 1, 20), value.maxIterationChange_));
            ASSERT_NO_FATAL_FAILURE(Same(table(static_cast<int>(i) + 1, 21), value.maxLinearBackwardError_));
            ASSERT_NO_FATAL_FAILURE(Same(table(static_cast<int>(i) + 1, 22), value.maxObstacleViolation_));
            ASSERT_NO_FATAL_FAILURE(Same(table(static_cast<int>(i) + 1, 23), value.maxDualViolation_));
            ASSERT_NO_FATAL_FAILURE(Same(table(static_cast<int>(i) + 1, 24), value.maxComplementarityResidual_));
            ASSERT_NO_FATAL_FAILURE(SameMember(table(static_cast<int>(i) + 1, 25), value.failure_, &EarlyExercisePdeFailure_::status_));
            ASSERT_NO_FATAL_FAILURE(SameMember(table(static_cast<int>(i) + 1, 26), value.failure_, &EarlyExercisePdeFailure_::timeIndex_));
            ASSERT_NO_FATAL_FAILURE(SameMember(table(static_cast<int>(i) + 1, 27), value.failure_, &EarlyExercisePdeFailure_::iteration_));
            ASSERT_NO_FATAL_FAILURE(SameMember(table(static_cast<int>(i) + 1, 28), value.failure_, &EarlyExercisePdeFailure_::calendarDate_));
            ASSERT_NO_FATAL_FAILURE(SameMember(table(static_cast<int>(i) + 1, 29), value.failure_, &EarlyExercisePdeFailure_::node_));
            ASSERT_NO_FATAL_FAILURE(SameMember(table(static_cast<int>(i) + 1, 30), value.failure_, &EarlyExercisePdeFailure_::residual_));
            ASSERT_NO_FATAL_FAILURE(SameMember(table(static_cast<int>(i) + 1, 31), value.failure_, &EarlyExercisePdeFailure_::threshold_));
            ASSERT_NO_FATAL_FAILURE(SameMember(table(static_cast<int>(i) + 1, 32), value.failure_, &EarlyExercisePdeFailure_::workUnits_));
            ASSERT_NO_FATAL_FAILURE(SameMember(table(static_cast<int>(i) + 1, 33), value.failure_, &EarlyExercisePdeFailure_::reason_));
            ASSERT_NO_FATAL_FAILURE(Same(table(static_cast<int>(i) + 1, 34), value.evaluationMembership_));
            ASSERT_NO_FATAL_FAILURE(Same(table(static_cast<int>(i) + 1, 35), value.evaluationBoundaryIntersectsSpotStencil_));
            ASSERT_NO_FATAL_FAILURE(Same(table(static_cast<int>(i) + 1, 36), value.futureContactChanged_));
            ASSERT_NO_FATAL_FAILURE(Same(table(static_cast<int>(i) + 1, 37), value.upwindChanged_));
        }
    }
    void PlanStepsParity(const Handle_<StorableEarlyExercisePdeRiskResult_>& result, const EarlyExercisePdeRiskResult_& expected) {
        Matrix_<Cell_> table;
        EarlyExercisePdeRiskResult_Get_PlanSteps(result, &table);
        ASSERT_NO_FATAL_FAILURE(Header(table, {"SourceTimeIndex", "TimeIndex", "Dt", "Theta", "CarryIndex", "Damped", "Projection"}));
        ASSERT_EQ(table.Rows(), expected.Plan().steps_.size() + 1);
        for (size_t i = 0; i < expected.Plan().steps_.size(); ++i) {
            const auto& value = expected.Plan().steps_[i];
            ASSERT_NO_FATAL_FAILURE(Same(table(static_cast<int>(i) + 1, 0), value.sourceTimeIndex_));
            ASSERT_NO_FATAL_FAILURE(Same(table(static_cast<int>(i) + 1, 1), value.timeIndex_));
            ASSERT_NO_FATAL_FAILURE(Same(table(static_cast<int>(i) + 1, 2), value.dt_));
            ASSERT_NO_FATAL_FAILURE(Same(table(static_cast<int>(i) + 1, 3), value.theta_));
            ASSERT_NO_FATAL_FAILURE(Same(table(static_cast<int>(i) + 1, 4), value.carryIndex_));
            ASSERT_NO_FATAL_FAILURE(Same(table(static_cast<int>(i) + 1, 5), value.damped_));
            ASSERT_NO_FATAL_FAILURE(Same(table(static_cast<int>(i) + 1, 6), value.projection_));
        }
    }
    void PlanTimeGridParity(const Handle_<StorableEarlyExercisePdeRiskResult_>& result, const EarlyExercisePdeRiskResult_& expected) {
        Matrix_<Cell_> table;
        EarlyExercisePdeRiskResult_Get_PlanTimeGrid(result, &table);
        ASSERT_NO_FATAL_FAILURE(Header(table, {"TimeIndex", "TimeYears", "CalendarDate", "IsExerciseDate", "IsCarryKnot", "IsMaturity"}));
        ASSERT_EQ(table.Rows(), expected.Plan().timeGrid_.size() + 1);
        for (size_t i = 0; i < expected.Plan().timeGrid_.size(); ++i) {
            const auto& value = expected.Plan().timeGrid_[i];
            ASSERT_NO_FATAL_FAILURE(Same(table(static_cast<int>(i) + 1, 0), i));
            ASSERT_NO_FATAL_FAILURE(Same(table(static_cast<int>(i) + 1, 1), value.timeYears_));
            ASSERT_NO_FATAL_FAILURE(Same(table(static_cast<int>(i) + 1, 2), value.calendarDate_));
            ASSERT_NO_FATAL_FAILURE(Same(table(static_cast<int>(i) + 1, 3), value.isExerciseDate_));
            ASSERT_NO_FATAL_FAILURE(Same(table(static_cast<int>(i) + 1, 4), value.isCarryKnot_));
            ASSERT_NO_FATAL_FAILURE(Same(table(static_cast<int>(i) + 1, 5), value.isMaturity_));
        }
    }
    void PlanGridParity(const Handle_<StorableEarlyExercisePdeRiskResult_>& result, const EarlyExercisePdeRiskResult_& expected) {
        Matrix_<Cell_> table;
        EarlyExercisePdeRiskResult_Get_PlanGrid(result, &table);
        ASSERT_NO_FATAL_FAILURE(Header(table, {"NodeIndex", "Spot"}));
        ASSERT_EQ(table.Rows(), expected.Plan().grid_.size() + 1);
        for (size_t i = 0; i < expected.Plan().grid_.size(); ++i) {
            const auto& value = expected.Plan().grid_[i];
            ASSERT_NO_FATAL_FAILURE(Same(table(static_cast<int>(i) + 1, 0), i));
            ASSERT_NO_FATAL_FAILURE(Same(table(static_cast<int>(i) + 1, 1), value));
        }
    }
    void PlanParity(const Handle_<StorableEarlyExercisePdeRiskResult_>& result, const EarlyExercisePdeRiskResult_& expected) {
        Matrix_<Cell_> table;
        EarlyExercisePdeRiskResult_Get_Plan(result, &table);
        ASSERT_NO_FATAL_FAILURE(Header(table, {"Field", "Value"}));
        ASSERT_EQ(table.Rows(), 23);
        ASSERT_EQ(Cell::ToString(table(1, 0)), "SchemaVersion");
        ASSERT_NO_FATAL_FAILURE(Same(table(1, 1), expected.Plan().schemaVersion_));
        ASSERT_EQ(Cell::ToString(table(2, 0)), "PlanId");
        ASSERT_NO_FATAL_FAILURE(Same(table(2, 1), expected.Plan().planId_));
        ASSERT_EQ(Cell::ToString(table(3, 0)), "EvaluationDate");
        ASSERT_NO_FATAL_FAILURE(Same(table(3, 1), expected.Plan().evaluationDate_));
        ASSERT_EQ(Cell::ToString(table(4, 0)), "MaturityDate");
        ASSERT_NO_FATAL_FAILURE(Same(table(4, 1), expected.Plan().maturityDate_));
        ASSERT_EQ(Cell::ToString(table(5, 0)), "Upper");
        ASSERT_NO_FATAL_FAILURE(Same(table(5, 1), expected.Plan().upper_));
        ASSERT_EQ(Cell::ToString(table(6, 0)), "GridPoints");
        ASSERT_NO_FATAL_FAILURE(Same(table(6, 1), expected.Plan().gridPoints_));
        ASSERT_EQ(Cell::ToString(table(7, 0)), "GridSpacing");
        ASSERT_NO_FATAL_FAILURE(Same(table(7, 1), expected.Plan().gridSpacing_));
        ASSERT_EQ(Cell::ToString(table(8, 0)), "Epsilon");
        ASSERT_NO_FATAL_FAILURE(Same(table(8, 1), expected.Plan().epsilon_));
        ASSERT_EQ(Cell::ToString(table(9, 0)), "Theta");
        ASSERT_NO_FATAL_FAILURE(Same(table(9, 1), expected.Plan().theta_));
        ASSERT_EQ(Cell::ToString(table(10, 0)), "EffectiveRannacherIntervals");
        ASSERT_NO_FATAL_FAILURE(Same(table(10, 1), expected.Plan().effectiveRannacherIntervals_));
        ASSERT_EQ(Cell::ToString(table(11, 0)), "PenaltyFactor");
        ASSERT_NO_FATAL_FAILURE(Same(table(11, 1), expected.Plan().penaltyFactor_));
        ASSERT_EQ(Cell::ToString(table(12, 0)), "MaxIterationsPerStep");
        ASSERT_NO_FATAL_FAILURE(Same(table(12, 1), expected.Plan().maxIterationsPerStep_));
        ASSERT_EQ(Cell::ToString(table(13, 0)), "LinearBackwardErrorLimit");
        ASSERT_NO_FATAL_FAILURE(Same(table(13, 1), expected.Plan().linearBackwardErrorLimit_));
        ASSERT_EQ(Cell::ToString(table(14, 0)), "InterpolationPolicy");
        ASSERT_NO_FATAL_FAILURE(Same(table(14, 1), expected.Plan().interpolationPolicy_));
        ASSERT_EQ(Cell::ToString(table(15, 0)), "SpotBumpOrigin");
        ASSERT_NO_FATAL_FAILURE(Same(table(15, 1), expected.Plan().spotBumpOrigin_));
        ASSERT_EQ(Cell::ToString(table(16, 0)), "VolatilityBumpOrigin");
        ASSERT_NO_FATAL_FAILURE(Same(table(16, 1), expected.Plan().volatilityBumpOrigin_));
        ASSERT_EQ(Cell::ToString(table(17, 0)), "PlannedScenarios");
        ASSERT_NO_FATAL_FAILURE(Same(table(17, 1), expected.Plan().plannedScenarios_));
        ASSERT_EQ(Cell::ToString(table(18, 0)), "BasePlannedSteps");
        ASSERT_NO_FATAL_FAILURE(Same(table(18, 1), expected.Plan().basePlannedSteps_));
        ASSERT_EQ(Cell::ToString(table(19, 0)), "PlannedSteps");
        ASSERT_NO_FATAL_FAILURE(Same(table(19, 1), expected.Plan().plannedSteps_));
        ASSERT_EQ(Cell::ToString(table(20, 0)), "PlannedNumericPayloadBytes");
        ASSERT_NO_FATAL_FAILURE(Same(table(20, 1), expected.Plan().plannedNumericPayloadBytes_));
        ASSERT_EQ(Cell::ToString(table(21, 0)), "PlannedWorkspaceBytes");
        ASSERT_NO_FATAL_FAILURE(Same(table(21, 1), expected.Plan().plannedWorkspaceBytes_));
        ASSERT_EQ(Cell::ToString(table(22, 0)), "PlannedPeakNativeBytes");
        ASSERT_NO_FATAL_FAILURE(Same(table(22, 1), expected.Plan().plannedPeakNativeBytes_));
    }
    void ExecutionParity(const Handle_<StorableEarlyExercisePdeRiskResult_>& result, const EarlyExercisePdeRiskResult_& expected) {
        Matrix_<Cell_> table;
        EarlyExercisePdeRiskResult_Get_Execution(result, &table);
        ASSERT_NO_FATAL_FAILURE(Header(table, {"Field", "Value"}));
        ASSERT_EQ(table.Rows(), 16);
        ASSERT_EQ(Cell::ToString(table(1, 0)), "PlannedScenarios");
        ASSERT_NO_FATAL_FAILURE(Same(table(1, 1), expected.Execution().plannedScenarios_));
        ASSERT_EQ(Cell::ToString(table(2, 0)), "AttemptedScenarios");
        ASSERT_NO_FATAL_FAILURE(Same(table(2, 1), expected.Execution().attemptedScenarios_));
        ASSERT_EQ(Cell::ToString(table(3, 0)), "SuccessfulScenarios");
        ASSERT_NO_FATAL_FAILURE(Same(table(3, 1), expected.Execution().successfulScenarios_));
        ASSERT_EQ(Cell::ToString(table(4, 0)), "FailedScenarios");
        ASSERT_NO_FATAL_FAILURE(Same(table(4, 1), expected.Execution().failedScenarios_));
        ASSERT_EQ(Cell::ToString(table(5, 0)), "NotRunScenarios");
        ASSERT_NO_FATAL_FAILURE(Same(table(5, 1), expected.Execution().notRunScenarios_));
        ASSERT_EQ(Cell::ToString(table(6, 0)), "TotalSolves");
        ASSERT_NO_FATAL_FAILURE(Same(table(6, 1), expected.Execution().totalSolves_));
        ASSERT_EQ(Cell::ToString(table(7, 0)), "WorkUnits");
        ASSERT_NO_FATAL_FAILURE(Same(table(7, 1), expected.Execution().workUnits_));
        ASSERT_EQ(Cell::ToString(table(8, 0)), "PlannedNumericPayloadBytes");
        ASSERT_NO_FATAL_FAILURE(Same(table(8, 1), expected.Execution().plannedNumericPayloadBytes_));
        ASSERT_EQ(Cell::ToString(table(9, 0)), "PlannedWorkspaceBytes");
        ASSERT_NO_FATAL_FAILURE(Same(table(9, 1), expected.Execution().plannedWorkspaceBytes_));
        ASSERT_EQ(Cell::ToString(table(10, 0)), "PlannedPeakNativeBytes");
        ASSERT_NO_FATAL_FAILURE(Same(table(10, 1), expected.Execution().plannedPeakNativeBytes_));
        ASSERT_EQ(Cell::ToString(table(11, 0)), "NumericPayloadBytes");
        ASSERT_NO_FATAL_FAILURE(Same(table(11, 1), expected.Execution().numericPayloadBytes_));
        ASSERT_EQ(Cell::ToString(table(12, 0)), "PeakWorkspaceBytes");
        ASSERT_NO_FATAL_FAILURE(Same(table(12, 1), expected.Execution().peakWorkspaceBytes_));
        ASSERT_EQ(Cell::ToString(table(13, 0)), "PeakNativeBytes");
        ASSERT_NO_FATAL_FAILURE(Same(table(13, 1), expected.Execution().peakNativeBytes_));
        ASSERT_EQ(Cell::ToString(table(14, 0)), "WorkLimitScenarioIndex");
        ASSERT_NO_FATAL_FAILURE(Same(table(14, 1), expected.Execution().workLimitScenarioIndex_));
        ASSERT_EQ(Cell::ToString(table(15, 0)), "WorkLimitSource");
        ASSERT_NO_FATAL_FAILURE(Same(table(15, 1), expected.Execution().workLimitSource_));
    }
    void StencilsParity(const Handle_<StorableEarlyExercisePdeRiskResult_>& result, const EarlyExercisePdeRiskResult_& expected) {
        Matrix_<Cell_> table;
        EarlyExercisePdeRiskResult_Get_Stencils(result, &table);
        ASSERT_NO_FATAL_FAILURE(Header(table, {"RiskIndex", "RiskId", "Scale", "EntryIndex", "ScenarioIndex", "Weight"}));
        int row = 1;
        for (const auto& risk : expected.Risks()) {
            for (int scale = 0; scale < 2; ++scale) {
                const auto& stencil = scale == 0 ? risk.coarseStencil_ : risk.fineStencil_;
                for (size_t i = 0; i < stencil.size(); ++i, ++row) {
                    ASSERT_NO_FATAL_FAILURE(Same(table(row, 0), risk.riskIndex_));
                    ASSERT_NO_FATAL_FAILURE(Same(table(row, 1), risk.riskId_));
                    ASSERT_EQ(Cell::ToString(table(row, 2)), scale == 0 ? "COARSE" : "FINE");
                    ASSERT_NO_FATAL_FAILURE(Same(table(row, 3), i));
                    ASSERT_NO_FATAL_FAILURE(Same(table(row, 4), stencil[i].scenarioIndex_));
                    ASSERT_NO_FATAL_FAILURE(Same(table(row, 5), stencil[i].weight_));
                }
            }
        }
        ASSERT_EQ(table.Rows(), row);
    }
    void SummaryParity(const Handle_<StorableEarlyExercisePdeRiskResult_>& result, const EarlyExercisePdeRiskResult_& expected) {
        Matrix_<Cell_> table;
        EarlyExercisePdeRiskResult_Get_Summary(result, &table);
        ASSERT_EQ(table.Rows(), 11);
        const std::array<const char*, 10> summary = {"SchemaVersion", "Status", "PriceAvailable", "RiskComplete", "Price",
                                                     "PriceUnit",     "Method", "RiskBasis",      "AadUsed",      "PlanId"};
        for (int i = 0; i < summary.size(); ++i)
            ASSERT_EQ(Cell::ToString(table(i + 1, 0)), summary[i]);
        ASSERT_NO_FATAL_FAILURE(Same(table(2, 1), expected.Status()));
        ASSERT_NO_FATAL_FAILURE(Same(table(3, 1), expected.PriceAvailable()));
        ASSERT_EQ(Cell::IsEmpty(table(5, 1)), !expected.PriceAvailable());
        if (expected.PriceAvailable())
            ASSERT_NO_FATAL_FAILURE(Same(table(5, 1), expected.Price()));
        ASSERT_NO_FATAL_FAILURE(Same(table(7, 1), expected.Method()));
        ASSERT_NO_FATAL_FAILURE(Same(table(8, 1), expected.RiskBasis()));
        ASSERT_NO_FATAL_FAILURE(Same(table(9, 1), expected.AadUsed()));
    }
    void BumpOriginsParity(const Handle_<StorableEarlyExercisePdeRiskResult_>& result, const EarlyExercisePdeRiskResult_& expected) {
        Matrix_<Cell_> table;
        EarlyExercisePdeRiskResult_Get_BumpOrigins(result, &table);
        ASSERT_NO_FATAL_FAILURE(Header(table, {"ParameterId", "Origin"}));
        const auto& plan = expected.Plan();
        ASSERT_EQ(table.Rows(), 3 + 2 * plan.rateBumpOrigins_.size());
        ASSERT_EQ(Cell::ToString(table(1, 0)), "Spot");
        ASSERT_NO_FATAL_FAILURE(Same(table(1, 1), plan.spotBumpOrigin_));
        ASSERT_EQ(Cell::ToString(table(2, 0)), "Volatility");
        ASSERT_NO_FATAL_FAILURE(Same(table(2, 1), plan.volatilityBumpOrigin_));
        for (size_t i = 0; i < plan.rateBumpOrigins_.size(); ++i) {
            const auto j = String_(std::to_string(i));
            ASSERT_EQ(Cell::ToString(table(3 + 2 * i, 0)), "Rate[" + j + "]");
            ASSERT_EQ(Cell::ToString(table(4 + 2 * i, 0)), "DividendYield[" + j + "]");
            ASSERT_NO_FATAL_FAILURE(Same(table(3 + 2 * i, 1), plan.rateBumpOrigins_[i]));
            ASSERT_NO_FATAL_FAILURE(Same(table(4 + 2 * i, 1), plan.dividendYieldBumpOrigins_[i]));
        }
    }
    void Parity(const Handle_<StorableEarlyExercisePdeRiskResult_>& result, const EarlyExercisePdeRiskResult_& expected) {
        ASSERT_NO_FATAL_FAILURE(RisksParity(result, expected));
        ASSERT_NO_FATAL_FAILURE(ScenariosParity(result, expected));
        ASSERT_NO_FATAL_FAILURE(PlanStepsParity(result, expected));
        ASSERT_NO_FATAL_FAILURE(PlanTimeGridParity(result, expected));
        ASSERT_NO_FATAL_FAILURE(PlanGridParity(result, expected));
        ASSERT_NO_FATAL_FAILURE(PlanParity(result, expected));
        ASSERT_NO_FATAL_FAILURE(ExecutionParity(result, expected));
        ASSERT_NO_FATAL_FAILURE(StencilsParity(result, expected));
        ASSERT_NO_FATAL_FAILURE(SummaryParity(result, expected));
        ASSERT_NO_FATAL_FAILURE(BumpOriginsParity(result, expected));
    }

} // namespace

TEST(ExcelEarlyExercisePdeRiskTest, TestEverySchemaCellMatchesCoreAcrossFourStates) {
    for (const size_t cap : {size_t(1326), size_t(1325), size_t(78), size_t(77), size_t(390)}) {
        EarlyExercisePdeRiskSettings_ risk;
        risk.maxWorkUnits_ = cap;
        const auto request = Request(Valuation(), risk);
        const auto expected = EvaluateEarlyExercisePdeRisk(request->val_);
        Handle_<StorableEarlyExercisePdeRiskResult_> result;
        EarlyExercisePdeRiskResult_New("result", request, &result);
        ASSERT_NO_FATAL_FAILURE(Parity(result, expected));
        double scalar = -999;
        for (const auto& record : expected.Risks()) {
            if (record.value_) {
                EarlyExercisePdeRiskResult_Get_RiskValue(result, record.riskId_, &scalar);
                ASSERT_DOUBLE_EQ(scalar, *record.value_);
            } else {
                scalar = -999;
                ASSERT_THROW(EarlyExercisePdeRiskResult_Get_RiskValue(result, record.riskId_, &scalar), Exception_);
                ASSERT_DOUBLE_EQ(scalar, -999);
            }
        }
    }
    auto price = Valuation();
    price.settings_.gridPoints_ = 401;
    price.settings_.ordinarySteps_ = 500;
    const auto request = Request(price);
    const auto expected = EvaluateEarlyExercisePdeRisk(request->val_);
    ASSERT_TRUE(expected.IsComplete());
    Handle_<StorableEarlyExercisePdeRiskResult_> result;
    EarlyExercisePdeRiskResult_New("complete", request, &result);
    ASSERT_NO_FATAL_FAILURE(Parity(result, expected));
}

TEST(ExcelEarlyExercisePdeRiskTest, TestStrictSettingsKeysCellsAndBumpColumns) {
    Handle_<StorableEarlyExercisePdeRiskSettings_> settings;
    EarlyExercisePdeRiskSettings_New("", Matrix_<Cell_>(2, 3), Matrix_<Cell_>(2, 3), &settings);
    const auto original = settings;
    for (const auto* key : {"Delta", "DELTA", "rate_bumps", "Key", "unknown"}) {
        ASSERT_NO_FATAL_FAILURE(Error([&] { EarlyExercisePdeRiskSettings_New("", Setting(key, Cell_(true)), {}, &settings); }, key));
        ASSERT_EQ(settings, original);
    }
    for (const auto& bad : {Cell_(), Cell_(true), Cell_("1"), Cell_(Date_(2026, 10, 18)), Cell_(-1.0), Cell_(1.5), Cell_(9007199254740992.0),
                            Cell_(std::numeric_limits<double>::infinity())}) {
        ASSERT_NO_FATAL_FAILURE(Error([&] { EarlyExercisePdeRiskSettings_New("", Setting("max_scenarios", bad), {}, &settings); }, "row=1"));
        ASSERT_EQ(settings, original);
    }
    for (const auto& bad :
         {Cell_(), Cell_(true), Cell_("0.001"), Cell_(Date_(2026, 10, 18)), Cell_(0.), Cell_(-1.), Cell_(std::numeric_limits<double>::quiet_NaN())}) {
        ASSERT_NO_FATAL_FAILURE(
            Error([&] { EarlyExercisePdeRiskSettings_New("", Setting("volatility_bump", bad), {}, &settings); }, "volatility_bump"));
    }
    auto rows = Setting("delta", Cell_(true));
    rows.Resize(2, 2);
    rows(1, 0) = "delta";
    rows(1, 1) = false;
    ASSERT_THROW(EarlyExercisePdeRiskSettings_New("", rows, {}, &settings), Exception_);
    rows(1, 0) = Cell_();
    ASSERT_THROW(EarlyExercisePdeRiskSettings_New("", rows, {}, &settings), Exception_);
    Matrix_<Cell_> bumps(2, 2);
    bumps(0, 0) = 1e-4;
    ASSERT_NO_FATAL_FAILURE(Error([&] { EarlyExercisePdeRiskSettings_New("", {}, bumps, &settings); }, "row=2"));
    bumps(1, 0) = 2e-4;
    EarlyExercisePdeRiskSettings_New("", Setting("spot_bump", Cell_()), bumps, &settings);
    ASSERT_EQ(settings->val_.rateBumps_.size(), 2);
    ASSERT_TRUE(settings->val_.dividendYieldBumps_.empty());
    Matrix_<Cell_> table;
    EarlyExercisePdeRiskSettings_Get_CarryBumps(settings, &table);
    ASSERT_NO_FATAL_FAILURE(Header(table, {"SegmentIndex", "RateBump", "DividendYieldBump"}));
    ASSERT_TRUE(Cell::IsEmpty(table(1, 2)));
    Matrix_<Cell_> roundTrip(2, 2);
    for (int i = 0; i < 2; ++i)
        for (int j = 0; j < 2; ++j)
            roundTrip(i, j) = table(i + 1, j + 1);
    Handle_<StorableEarlyExercisePdeRiskSettings_> copy;
    EarlyExercisePdeRiskSettings_New("", {}, roundTrip, &copy);
    ASSERT_EQ(copy->val_.rateBumps_, settings->val_.rateBumps_);
    const String_ nulName(std::string("bad\0name", 8));
    ASSERT_THROW(EarlyExercisePdeRiskSettings_New(nulName, {}, {}, &settings), Exception_);
}

TEST(ExcelEarlyExercisePdeRiskTest, TestSemanticScalarErrorsKeepCellCoordinates) {
    Handle_<StorableEarlyExercisePdeRiskSettings_> settings;
    for (const auto* key : {"volatility_bump", "delta_stability_tolerance", "spot_bump"}) {
        ASSERT_NO_FATAL_FAILURE(Error([&] { EarlyExercisePdeRiskSettings_New("", Setting(key, Cell_(0.)), {}, &settings); }, "row=1"));
    }
}

TEST(ExcelEarlyExercisePdeRiskTest, TestRiskIdTextErrorNamesItsArgument) {
    Handle_<StorableEarlyExercisePdeRiskResult_> result;
    EarlyExercisePdeRiskResult_New("", Request(Valuation()), &result);
    double scalar = 123.;
    ASSERT_NO_FATAL_FAILURE(Error([&] { EarlyExercisePdeRiskResult_Get_RiskValue(result, String_(std::string("Delta\0", 6)), &scalar); }, "risk_id"));
    ASSERT_DOUBLE_EQ(scalar, 123.);
}

TEST(ExcelEarlyExercisePdeRiskTest, TestDetachedHandlesArchiveAndFailureRecovery) {
    auto valuation = Valuation();
    auto request = Request(valuation);
    Handle_<StorableEarlyExercisePdeRiskResult_> result;
    EarlyExercisePdeRiskResult_New("first", request, &result);
    const auto previous = result;
    const double price = result->val_.Price();
    for (int i = 0; i < 2; ++i) {
        auto bad = request->val_;
        bad.risk_.maxScenarios_ = 0;
        Handle_<StorableEarlyExercisePdeRiskRequest_> preflight(new StorableEarlyExercisePdeRiskRequest_("", bad));
        ASSERT_THROW(EarlyExercisePdeRiskResult_New("bad", preflight, &result), Exception_);
        ASSERT_EQ(result, previous);
        EarlyExercisePdeRiskResult_New("ok", request, &result);
        ASSERT_DOUBLE_EQ(result->val_.Price(), price);
        result = previous;
    }
    auto bad = request->val_.risk_;
    bad.rateBumps_ = {1e-4, 2e-4};
    Handle_<StorableEarlyExercisePdeRequest_> source(new StorableEarlyExercisePdeRequest_("", valuation));
    Handle_<StorableEarlyExercisePdeRiskSettings_> settings(new StorableEarlyExercisePdeRiskSettings_("", bad));
    const auto originalRequest = request;
    ASSERT_THROW(EarlyExercisePdeRiskRequest_New("", source, settings, &request), Exception_);
    ASSERT_EQ(request, originalRequest);
    Handle_<StorableEarlyExercisePdeRiskRequest_> detached;
    EarlyExercisePdeRiskResult_Get_Request(result, &detached);
    ASSERT_NE(detached.get(), request.get());
    request.reset();
    source.reset();
    Matrix_<Cell_> rows;
    EarlyExercisePdeRiskResult_Get_Risks(result, &rows);
    rows(1, 9) = -999.;
    ASSERT_DOUBLE_EQ(result->val_.Price(), price);
    Handle_<StorableEarlyExercisePdeResult_> base;
    EarlyExercisePdeRiskResult_Get_BaseResult(result, &base);
    ASSERT_DOUBLE_EQ(base->val_.Price(), price);
    Handle_<StorableEarlyExercisePdeRequest_> originalValuation;
    EarlyExercisePdeRiskRequest_Get_Valuation(detached, &originalValuation);
    ASSERT_EQ(originalValuation->val_.settings_.upper_, valuation.settings_.upper_);
    EarlyExercisePdeRiskResult_Get_ResolvedSettings(result, &settings);
    ASSERT_TRUE(settings->val_.spotBump_);
    EarlyExercisePdeRiskRequest_Get_Settings(detached, &settings);
    ASSERT_FALSE(settings->val_.spotBump_);
    ASSERT_THROW(JSON::WriteString(*result), Exception_);
    ASSERT_THROW(JSON::WriteString(*detached), Exception_);
    ASSERT_THROW(JSON::WriteString(*settings), Exception_);
}

TEST(ExcelEarlyExercisePdeRiskTest, TestScenarioFailureAndTwoSegmentMetadata) {
    const auto request = Request(Valuation());
    PDE::EarlyExercisePdeRiskInternal::TestControl_ seam;
    seam.scenarioIndex_ = 12;
    seam.failFirstSolve_ = true;
    const auto failed = PDE::EarlyExercisePdeRiskInternal::Evaluate(request->val_, seam);
    Handle_<StorableEarlyExercisePdeRiskResult_> result(new StorableEarlyExercisePdeRiskResult_("", failed));
    ASSERT_EQ(failed.Risks()[3].status_, EarlyExercisePdeRiskAvailability_("UNAVAILABLE_SCENARIO_FAILED"));
    ASSERT_NO_FATAL_FAILURE(Parity(result, failed));
    auto valuation = Valuation();
    const Date_ knot(2027, 3, 4);
    valuation.market_.carry_ = {{knot, .05, .03}, {valuation.contract_.maturityDate_, .06, .02}};
    const auto two = Request(valuation);
    EarlyExercisePdeRiskResult_New("", two, &result);
    ASSERT_EQ(result->val_.Risks().size(), 7);
    ASSERT_EQ(result->val_.Scenarios().size(), 25);
    ASSERT_EQ(result->val_.Risks()[5].startDate_, knot);
    ASSERT_NO_FATAL_FAILURE(Parity(result, EvaluateEarlyExercisePdeRisk(two->val_)));
}

TEST(ExcelEarlyExercisePdeRiskTest, TestRecordingReadyGraphChannelsAndLegacyTapeSurvive) {
    using namespace AAD;
    Clear(*Tape());
    {
        auto mode = SetNumResultsForAAD(true, 2);
        RecordingScope_ scope;
        Number_ x;
        scope.RegisterInput(x, 2.);
        scope.StartRecording();
        Number_ square = x * x;
        const auto request = Request(Valuation());
        Handle_<StorableEarlyExercisePdeRiskResult_> result;
        const auto recordingNodes = Tape()->nodes_.OccupiedSlots();
        EarlyExercisePdeRiskResult_New("", request, &result);
        ASSERT_EQ(Tape()->nodes_.OccupiedSlots(), recordingNodes);
        Number_ cube = square * x;
        scope.FinishRecording();
        scope.ClearAdjoints();
        NativeOperations_::SetSeed(cube, 3., 1);
        const auto nodes = Tape()->nodes_.OccupiedSlots(), events = Tape()->ReverseEventCount();
        auto limited = request->val_;
        limited.risk_.maxWorkUnits_ = 77;
        Handle_<StorableEarlyExercisePdeRiskRequest_> failure(new StorableEarlyExercisePdeRiskRequest_("", limited));
        EarlyExercisePdeRiskResult_New("", failure, &result);
        Matrix_<Cell_> rows;
        EarlyExercisePdeRiskResult_Get_Scenarios(result, &rows);
        ASSERT_EQ(Tape()->nodes_.OccupiedSlots(), nodes);
        ASSERT_EQ(Tape()->ReverseEventCount(), events);
        ASSERT_TRUE(Tape()->multi_);
        ASSERT_EQ(Tape()->numAdj_, 2);
        ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(cube, 1), 3.);
        scope.Reverse();
        ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(x, 1), 36.);
        scope.Close();
    }
    Clear(*Tape());
    Number_ x = 2.;
    NewRecording(*Tape());
    Number_ square = x * x;
    const auto nodes = Tape()->nodes_.OccupiedSlots();
    Handle_<StorableEarlyExercisePdeRiskResult_> result;
    EarlyExercisePdeRiskResult_New("", Request(Valuation()), &result);
    ASSERT_EQ(Tape()->nodes_.OccupiedSlots(), nodes);
    NativeOperations_::SetSeed(square, 1.);
    PropagateToStart(*Tape());
    ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(x), 4.);
    Clear(*Tape());
}

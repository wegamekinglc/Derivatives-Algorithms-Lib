//
// Created by Codex on 2026/10/11.
//

#include <gtest/gtest.h>

#include <array>
#include <limits>
#include <string>

#include <dal-excel/src/__risk.hpp>

#include <dal-excel/src/__europeanpderisk.hpp>
#include <dal-excel/src/__script_test_api.hpp>
#include <dal/storage/json.hpp>

using namespace Dal;

namespace {
    Matrix_<Cell_> Setting(const String_& key, const Cell_& value) {
        Matrix_<Cell_> rows(1, 2);
        rows(0, 0) = key;
        rows(0, 1) = value;
        return rows;
    }

    template <class F_> void AssertError(F_ action, std::initializer_list<const char*> fields) {
        try {
            action();
            FAIL() << "expected contextual error";
        } catch (const Exception_& error) {
            for (const auto* field : fields)
                ASSERT_NE(std::string(error.what()).find(field), std::string::npos) << error.what();
        }
    }

    Handle_<StorableEuropeanPdeRiskSettings_> SmallSettings() {
        Matrix_<Cell_> rows(2, 2);
        rows(0, 0) = "grid_points";
        rows(0, 1) = 9.0;
        rows(1, 0) = "ordinary_steps";
        rows(1, 1) = 8.0;
        Handle_<StorableEuropeanPdeRiskSettings_> settings;
        EuropeanPdeRiskSettings_New("small", rows, &settings);
        return settings;
    }

    Handle_<StorableEuropeanPdeRiskResult_> SmallResult() {
        Handle_<StorableEuropeanPdeRiskRequest_> request;
        EuropeanPdeRiskRequest_New("request", 0.05, 0.20, 110.0, SmallSettings(), &request);
        Handle_<StorableEuropeanPdeRiskResult_> result;
        EuropeanPdeRiskResult_New("risk", request, &result);
        return result;
    }
} // namespace

TEST(ExcelEuropeanPdeRiskTest, TestOwningWorksheetMatchesAcceptedPricesAndEveryRisk) {
    Matrix_<Cell_> rows(2, 2);
    rows(0, 0) = "grid_points";
    rows(0, 1) = 9.0;
    rows(1, 0) = "ordinary_steps";
    rows(1, 1) = 8.0;
    Handle_<StorableEuropeanPdeRiskSettings_> settings;
    EuropeanPdeRiskSettings_New("small", rows, &settings);
    Handle_<StorableEuropeanPdeRiskRequest_> request;
    EuropeanPdeRiskRequest_New("request", 0.05, 0.20, 110.0, settings, &request);
    Handle_<StorableEuropeanPdeRiskResult_> result;
    EuropeanPdeRiskResult_New("risk", request, &result);
    Matrix_<Cell_> prices, risks;
    EuropeanPdeRiskResult_Get_Prices(result, &prices);
    EuropeanPdeRiskResult_Get_Jacobian(result, &risks);
    ASSERT_EQ(prices.Rows(), 3);
    ASSERT_EQ(prices.Cols(), 2);
    ASSERT_EQ(Cell::ToString(prices(0, 0)), "Payoff");
    ASSERT_EQ(Cell::ToString(prices(1, 0)), "Call");
    ASSERT_EQ(Cell::ToString(prices(2, 0)), "Put");
    ASSERT_NEAR(Cell::ToDouble(prices(1, 1)), 4.153690693968586, 1e-10);
    ASSERT_NEAR(Cell::ToDouble(prices(2, 1)), 10.770781220697858, 1e-10);
    const std::array<std::array<double, 3>, 2> expected = {
        {{40.40684028445804, 27.8766807705311, -0.0907395605282994}, {-64.1496803210784, 27.87666960963085, 0.8605082862555484}}};
    ASSERT_EQ(risks.Rows(), 7);
    ASSERT_EQ(risks.Cols(), 4);
    for (int layer = 0; layer < 2; ++layer)
        for (int coordinate = 0; coordinate < 3; ++coordinate) {
            const int row = 1 + 3 * layer + coordinate;
            ASSERT_EQ(Cell::ToString(risks(row, 0)), layer == 0 ? "Call" : "Put");
            ASSERT_EQ(Cell::ToString(risks(row, 1)), result->val_.parameterLabels_[coordinate]);
            ASSERT_EQ(Cell::ToString(risks(row, 2)), result->val_.parameterUnits_[coordinate]);
            ASSERT_NEAR(Cell::ToDouble(risks(row, 3)), expected[layer][coordinate], 1e-9);
        }
}

TEST(ExcelEuropeanPdeRiskTest, TestSettingsRejectInvalidCellsWithLocationAndRetainPreviousHandle) {
    auto settings = SmallSettings();
    const auto previous = settings;
    for (const Cell_& invalid :
         {Cell_(), Cell_(true), Cell_("9"), Cell_(Date_(2026, 10, 11)), Cell_(0.5), Cell_(-1.0), Cell_(std::numeric_limits<double>::infinity())}) {
        ASSERT_NO_FATAL_FAILURE(AssertError([&] { EuropeanPdeRiskSettings_New("bad", Setting("grid_points", invalid), &settings); },
                                            {"EuropeanPdeRiskSettings_New", "row=1 column=2", "grid_points"}));
        ASSERT_EQ(settings, previous);
    }
    for (const auto* key : {"upper", "expiry", "forward_backward_error_limit", "transpose_backward_error_limit"}) {
        ASSERT_NO_FATAL_FAILURE(AssertError([&] { EuropeanPdeRiskSettings_New("bad", Setting(key, Cell_(-1.0)), &settings); },
                                            {"EuropeanPdeRiskSettings_New", "row=1 column=2", key}));
        ASSERT_EQ(settings, previous);
    }
    for (const auto* key : {"forward_backward_error_limit", "transpose_backward_error_limit"})
        ASSERT_THROW(EuropeanPdeRiskSettings_New("bad", Setting(key, Cell_(1.01)), &settings), Exception_);
    ASSERT_THROW(EuropeanPdeRiskSettings_New("bad", Setting("typo", Cell_(1.0)), &settings), Exception_);
    auto duplicate = Setting("grid_points", Cell_(9.0));
    duplicate.Resize(2, 2);
    duplicate(1, 0) = "GRID_POINTS";
    duplicate(1, 1) = 9.0;
    ASSERT_THROW(EuropeanPdeRiskSettings_New("bad", duplicate, &settings), Exception_);
    ASSERT_THROW(EuropeanPdeRiskSettings_New("bad", Matrix_<Cell_>(1, 3), &settings), Exception_);
    const String_ nul(std::string("na\0me", 5));
    ASSERT_THROW(EuropeanPdeRiskSettings_New(nul, {}, &settings), Exception_);
    ASSERT_EQ(settings, previous);
}

TEST(ExcelEuropeanPdeRiskTest, TestDefaultBlankZeroAndWorksheetBoundsWithoutLargeExecution) {
    Handle_<StorableEuropeanPdeRiskSettings_> settings;
    EuropeanPdeRiskSettings_New("defaults", Matrix_<Cell_>(1, 1), &settings);
    ASSERT_EQ(settings->val_.physical_.gridPoints_, 61);
    ASSERT_FALSE(settings->val_.physical_.spotIndex_);
    EuropeanPdeRiskSettings_New("blank", Setting("numeric_payload_budget_bytes", Cell_()), &settings);
    ASSERT_FALSE(settings->val_.numericPayloadBudgetBytes_);
    EuropeanPdeRiskSettings_New("zero", Setting("numeric_payload_budget_bytes", Cell_(0.0)), &settings);
    ASSERT_EQ(*settings->val_.numericPayloadBudgetBytes_, 0);
    Matrix_<Cell_> config;
    EuropeanPdeRiskSettings_Get_Configuration(settings, &config);
    ASSERT_DOUBLE_EQ(Cell::ToDouble(config(8, 1)), 0.0);
    for (const auto* key : {"numeric_payload_budget_bytes", "recording_capacity_budget_bytes"})
        for (const double invalid : {-1.0, 0.5, 9007199254740992.0})
            ASSERT_THROW(EuropeanPdeRiskSettings_New("bad", Setting(key, Cell_(invalid)), &settings), Exception_);
    Matrix_<Cell_> bounds(3, 2);
    bounds(0, 0) = "grid_points";
    bounds(0, 1) = 1048575.0;
    bounds(1, 0) = "spot_index";
    bounds(1, 1) = 2.0;
    bounds(2, 0) = "ordinary_steps";
    bounds(2, 1) = 1048573.0;
    ASSERT_NO_THROW(EuropeanPdeRiskSettings_New("largest", bounds, &settings));
    bounds(0, 1) = 1048576.0;
    ASSERT_THROW(EuropeanPdeRiskSettings_New("bad", bounds, &settings), Exception_);
    bounds(0, 1) = 1048575.0;
    bounds(2, 1) = 1048574.0;
    ASSERT_THROW(EuropeanPdeRiskSettings_New("bad", bounds, &settings), Exception_);
}

TEST(ExcelEuropeanPdeRiskTest, TestExactAndShortBudgetsAtomicFailureAndRecovery) {
    auto config = SmallSettings()->val_;
    Handle_<StorableEuropeanPdeRiskRequest_> request;
    auto result = SmallResult();
    const auto previous = result;
    for (size_t cap : {size_t(0), size_t(687)}) {
        config.numericPayloadBudgetBytes_ = cap;
        Handle_<StorableEuropeanPdeRiskSettings_> settings(new StorableEuropeanPdeRiskSettings_("cap", config));
        EuropeanPdeRiskRequest_New("request", 0.05, 0.20, 110.0, settings, &request);
        ASSERT_THROW(EuropeanPdeRiskResult_New("bad", request, &result), Exception_);
        ASSERT_EQ(result, previous);
    }
    config.numericPayloadBudgetBytes_ = 688;
    config.recordingCapacityBudgetBytes_ = 0;
    Handle_<StorableEuropeanPdeRiskSettings_> settings(new StorableEuropeanPdeRiskSettings_("tape", config));
    EuropeanPdeRiskRequest_New("request", 0.05, 0.20, 110.0, settings, &request);
    ASSERT_THROW(EuropeanPdeRiskResult_New("bad", request, &result), Exception_);
    ASSERT_EQ(result, previous);
    config.recordingCapacityBudgetBytes_.reset();
    settings.reset(new StorableEuropeanPdeRiskSettings_("exact", config));
    EuropeanPdeRiskRequest_New("request", 0.05, 0.20, 110.0, settings, &request);
    EuropeanPdeRiskResult_New("recovered", request, &result);
    ASSERT_DOUBLE_EQ(result->val_.prices_[0], previous->val_.prices_[0]);
    const auto validRequest = request;
    ASSERT_THROW(EuropeanPdeRiskRequest_New("bad", std::numeric_limits<double>::quiet_NaN(), 0.20, 110.0, settings, &request), Exception_);
    ASSERT_EQ(request, validRequest);
    EuropeanPdeRiskRequest_New("kink", 0.05, 0.20, 100.0, settings, &request);
    const auto validResult = result;
    ASSERT_THROW(EuropeanPdeRiskResult_New("bad", request, &result), Exception_);
    ASSERT_EQ(result, validResult);
}

TEST(ExcelEuropeanPdeRiskTest, TestNondefaultNegativeRatePointAndSettingsReachPublicEvaluator) {
    auto config = SmallSettings()->val_;
    config.physical_.upper_ = 480.0;
    config.physical_.expiry_ = 0.75;
    config.physical_.dividendYield_ = -0.01;
    config.physical_.spotIndex_ = 3;
    Handle_<StorableEuropeanPdeRiskSettings_> settings(new StorableEuropeanPdeRiskSettings_("changed", config));
    Handle_<StorableEuropeanPdeRiskRequest_> request;
    EuropeanPdeRiskRequest_New("negative", -0.02, 0.31, 137.0, settings, &request);
    const auto expected = EvaluateEuropeanPdeRisk(request->val_);
    Handle_<StorableEuropeanPdeRiskResult_> result;
    EuropeanPdeRiskResult_New("result", request, &result);
    ASSERT_DOUBLE_EQ(result->val_.spot_, 180.0);
    for (int layer = 0; layer < 2; ++layer) {
        ASSERT_DOUBLE_EQ(result->val_.prices_[layer], expected.prices_[layer]);
        for (int coordinate = 0; coordinate < 3; ++coordinate)
            ASSERT_DOUBLE_EQ(result->val_.jacobian_(layer, coordinate), expected.jacobian_(layer, coordinate));
    }
}

TEST(ExcelEuropeanPdeRiskTest, TestConstructorsAndAllGettersPreserveCallerGraphAndSeed) {
    using namespace AAD;
    auto result = SmallResult();
#ifdef _WIN32
    double adjoint = 0.0;
    Excel::ScriptTestWithRecording(
        [&](size_t nodes) {
#else
    Clear(*Tape());
    auto mode = SetNumResultsForAAD(true, 3);
    RecordingScope_ scope;
    Number_ input;
    scope.RegisterInput(input, 2.0);
    scope.StartRecording();
    Number_ square = input * input;
    scope.FinishRecording();
    scope.ClearAdjoints();
    NativeOperations_::SetSeed(square, 7.0, 2);
    const auto nodes = Tape()->nodes_.OccupiedSlots();
#endif
            const auto settings = SmallSettings();
            Handle_<StorableEuropeanPdeRiskRequest_> request;
            EuropeanPdeRiskRequest_New("passive", 0.05, 0.20, 110.0, settings, &request);
            Matrix_<Cell_> cells;
            EuropeanPdeRiskSettings_Get_Configuration(settings, &cells);
            Handle_<StorableEuropeanPdeRiskSettings_> copy;
            EuropeanPdeRiskRequest_Get_Settings(request, &copy);
            EuropeanPdeRiskRequest_Get_Point(request, &cells);
            Handle_<StorableEuropeanPdeRiskRequest_> resolved;
            EuropeanPdeRiskResult_Get_Request(result, &resolved);
            const auto getters = {
                EuropeanPdeRiskResult_Get_Prices,        EuropeanPdeRiskResult_Get_Jacobian,        EuropeanPdeRiskResult_Get_Grid,
                EuropeanPdeRiskResult_Get_ForwardErrors, EuropeanPdeRiskResult_Get_TransposeErrors, EuropeanPdeRiskResult_Get_Execution};
            for (const auto getter : getters)
                getter(result, &cells);
            const auto previous = result;
            ASSERT_THROW(EuropeanPdeRiskResult_New("active", request, &result), Exception_);
            ASSERT_EQ(result, previous);
#ifdef _WIN32
            ASSERT_EQ(Excel::ScriptTestTapeNodeCount(), nodes);
        },
        true, &adjoint);
    ASSERT_DOUBLE_EQ(adjoint, 4.0);
#else
    ASSERT_EQ(Tape()->nodes_.OccupiedSlots(), nodes);
    ASSERT_EQ(Tape()->numAdj_, 3);
    ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(square, 2), 7.0);
    scope.Reverse();
    ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(input, 2), 28.0);
    scope.Close();
#endif
}

TEST(ExcelEuropeanPdeRiskTest, TestNullHandlesAndUnsupportedArchives) {
    Matrix_<Cell_> cells;
    Handle_<StorableEuropeanPdeRiskSettings_> settings;
    Handle_<StorableEuropeanPdeRiskRequest_> request;
    Handle_<StorableEuropeanPdeRiskResult_> result;
    ASSERT_THROW(EuropeanPdeRiskSettings_Get_Configuration({}, &cells), Exception_);
    ASSERT_THROW(EuropeanPdeRiskRequest_Get_Settings({}, &settings), Exception_);
    ASSERT_THROW(EuropeanPdeRiskRequest_Get_Point({}, &cells), Exception_);
    ASSERT_THROW(EuropeanPdeRiskResult_New("null", {}, &result), Exception_);
    ASSERT_THROW(EuropeanPdeRiskResult_Get_Request({}, &request), Exception_);
    const auto getters = {EuropeanPdeRiskResult_Get_Prices,        EuropeanPdeRiskResult_Get_Jacobian,        EuropeanPdeRiskResult_Get_Grid,
                          EuropeanPdeRiskResult_Get_ForwardErrors, EuropeanPdeRiskResult_Get_TransposeErrors, EuropeanPdeRiskResult_Get_Execution};
    for (const auto getter : getters)
        ASSERT_THROW(getter({}, &cells), Exception_);
    settings = SmallSettings();
    EuropeanPdeRiskRequest_New("request", 0.05, 0.20, 110.0, settings, &request);
    result = SmallResult();
    const std::array<const Storable_*, 3> values = {settings.get(), request.get(), result.get()};
    for (const auto* value : values)
        ASSERT_NO_FATAL_FAILURE(AssertError([&] { JSON::WriteString(*value); }, {"RiskArchiveUnsupported"}));
    ASSERT_EQ(settings->Type(), "EuropeanPdeRiskSettings");
    ASSERT_EQ(request->Type(), "EuropeanPdeRiskRequest");
    ASSERT_EQ(result->Type(), "EuropeanPdeRiskResult");
}

TEST(ExcelEuropeanPdeRiskTest, TestCopiedConfigurationPointAndResolvedRequest) {
    const auto settings = SmallSettings();
    Matrix_<Cell_> config;
    EuropeanPdeRiskSettings_Get_Configuration(settings, &config);
    ASSERT_EQ(config.Rows(), 10);
    ASSERT_EQ(config.Cols(), 2);
    ASSERT_DOUBLE_EQ(Cell::ToDouble(config(0, 1)), 9.0);
    ASSERT_DOUBLE_EQ(Cell::ToDouble(config(1, 1)), 8.0);
    ASSERT_TRUE(Cell::IsEmpty(config(3, 1)));
    ASSERT_TRUE(Cell::IsEmpty(config(8, 1)));
    ASSERT_TRUE(Cell::IsEmpty(config(9, 1)));
    Handle_<StorableEuropeanPdeRiskRequest_> request;
    EuropeanPdeRiskRequest_New("request", 0.05, 0.20, 110.0, settings, &request);
    Handle_<StorableEuropeanPdeRiskSettings_> copy;
    EuropeanPdeRiskRequest_Get_Settings(request, &copy);
    ASSERT_NE(copy.get(), settings.get());
    ASSERT_FALSE(copy->val_.physical_.spotIndex_);
    Matrix_<Cell_> point;
    EuropeanPdeRiskRequest_Get_Point(request, &point);
    ASSERT_EQ(point.Rows(), 3);
    const std::array<const char*, 3> keys = {"rate", "volatility", "strike"};
    for (int index = 0; index < 3; ++index) {
        ASSERT_EQ(Cell::ToString(point(index, 0)), keys[index]);
        ASSERT_DOUBLE_EQ(Cell::ToDouble(point(index, 1)), request->val_.point_[index]);
    }
    Handle_<StorableEuropeanPdeRiskResult_> result;
    EuropeanPdeRiskResult_New("risk", request, &result);
    Handle_<StorableEuropeanPdeRiskRequest_> resolved;
    EuropeanPdeRiskResult_Get_Request(result, &resolved);
    ASSERT_NE(resolved.get(), request.get());
    ASSERT_EQ(*resolved->val_.settings_.spotIndex_, 2);
    ASSERT_FALSE(request->val_.settings_.spotIndex_);
    config(0, 1) = -1.0;
    point(0, 1) = -1.0;
    EuropeanPdeRiskSettings_Get_Configuration(settings, &config);
    EuropeanPdeRiskRequest_Get_Point(request, &point);
    ASSERT_DOUBLE_EQ(Cell::ToDouble(config(0, 1)), 9.0);
    ASSERT_DOUBLE_EQ(Cell::ToDouble(point(0, 1)), 0.05);
}

TEST(ExcelEuropeanPdeRiskTest, TestEveryDiagnosticAndExecutionFieldIsDetachedAndLabeled) {
    const auto result = SmallResult();
    Matrix_<Cell_> grid, forward, transpose, execution;
    EuropeanPdeRiskResult_Get_Grid(result, &grid);
    EuropeanPdeRiskResult_Get_ForwardErrors(result, &forward);
    EuropeanPdeRiskResult_Get_TransposeErrors(result, &transpose);
    EuropeanPdeRiskResult_Get_Execution(result, &execution);
    ASSERT_EQ(grid.Rows(), 10);
    ASSERT_EQ(grid.Cols(), 2);
    ASSERT_EQ(Cell::ToString(grid(0, 0)), "NodeIndex");
    ASSERT_EQ(Cell::ToString(grid(0, 1)), "Spot");
    for (int index = 0; index < 9; ++index) {
        ASSERT_DOUBLE_EQ(Cell::ToDouble(grid(index + 1, 0)), index);
        ASSERT_DOUBLE_EQ(Cell::ToDouble(grid(index + 1, 1)), result->val_.grid_[index]);
    }
    ASSERT_EQ(forward.Rows(), 11);
    ASSERT_EQ(forward.Cols(), 3);
    ASSERT_EQ(transpose.Rows(), 11);
    ASSERT_EQ(transpose.Cols(), 5);
    ASSERT_EQ(Cell::ToString(forward(0, 0)), "Step");
    ASSERT_EQ(Cell::ToString(transpose(0, 0)), "Step");
    for (int layer = 0; layer < 2; ++layer)
        ASSERT_EQ(Cell::ToString(forward(0, layer + 1)), result->val_.payoffLabels_[layer]);
    for (int channel = 0; channel < 4; ++channel)
        ASSERT_EQ(Cell::ToString(transpose(0, channel + 1)), result->val_.transposeErrorLabels_[channel]);
    for (int step = 0; step < 10; ++step) {
        ASSERT_DOUBLE_EQ(Cell::ToDouble(forward(step + 1, 0)), step + 1);
        ASSERT_DOUBLE_EQ(Cell::ToDouble(transpose(step + 1, 0)), step + 1);
        for (int layer = 0; layer < 2; ++layer)
            ASSERT_DOUBLE_EQ(Cell::ToDouble(forward(step + 1, layer + 1)), result->val_.forwardBackwardErrors_(step, layer));
        for (int channel = 0; channel < 4; ++channel)
            ASSERT_DOUBLE_EQ(Cell::ToDouble(transpose(step + 1, channel + 1)), result->val_.transposeBackwardErrors_(step, channel));
    }
    ASSERT_EQ(execution.Rows(), 6);
    ASSERT_EQ(execution.Cols(), 2);
    const std::array<const char*, 6> keys = {"method",          "actual_steps",          "numeric_payload_bytes",
                                             "peak_tape_bytes", "cleanup_reserve_bytes", "reverse_scratch_peak_bytes"};
    for (int index = 0; index < 6; ++index)
        ASSERT_EQ(Cell::ToString(execution(index, 0)), keys[index]);
    ASSERT_EQ(Cell::ToString(execution(0, 1)), result->val_.method_);
    const auto& counts = result->val_.execution_;
    const std::array<double, 5> values = {double(counts.actualSteps_), double(counts.numericPayloadBytes_), double(counts.peakTapeBytes_),
                                          double(counts.cleanupReserveBytes_), double(counts.reverseScratchPeakBytes_)};
    for (int index = 0; index < 5; ++index)
        ASSERT_DOUBLE_EQ(Cell::ToDouble(execution(index + 1, 1)), values[index]);
    grid(1, 1) = -99.0;
    forward(1, 1) = -99.0;
    transpose(1, 1) = -99.0;
    execution(1, 1) = -99.0;
    EuropeanPdeRiskResult_Get_Grid(result, &grid);
    EuropeanPdeRiskResult_Get_ForwardErrors(result, &forward);
    EuropeanPdeRiskResult_Get_TransposeErrors(result, &transpose);
    EuropeanPdeRiskResult_Get_Execution(result, &execution);
    ASSERT_DOUBLE_EQ(Cell::ToDouble(grid(1, 1)), 0.0);
    ASSERT_DOUBLE_EQ(Cell::ToDouble(forward(1, 1)), result->val_.forwardBackwardErrors_(0, 0));
    ASSERT_DOUBLE_EQ(Cell::ToDouble(transpose(1, 1)), result->val_.transposeBackwardErrors_(0, 0));
    ASSERT_DOUBLE_EQ(Cell::ToDouble(execution(1, 1)), 10.0);
}

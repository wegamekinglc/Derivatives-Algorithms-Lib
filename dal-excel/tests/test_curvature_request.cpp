//
// Created by Codex on 2026/10/11.
//

#include <gtest/gtest.h>

#include <limits>
#include <string>

#include <dal/math/aad/native.hpp>

#include <dal-excel/src/__curvaturerequest.hpp>
#include <dal-excel/src/__script_test_api.hpp>
#include <dal/storage/json.hpp>

using namespace Dal;

namespace {
    Matrix_<Cell_> Setting(const String_& key, const Cell_& value) {
        Matrix_<Cell_> cells(1, 2);
        cells(0, 0) = key;
        cells(0, 1) = value;
        return cells;
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
} // namespace

TEST(ExcelCurvatureRequestTest, TestOwnsDirectionsStepsBudgetsAndShape) {
    Matrix_<Cell_> directions(2, 3);
    const double values[6] = {1.0, 0.0, -2.0, -0.5, 0.0, 2.0};
    for (int row = 0; row < 2; ++row)
        for (int column = 0; column < 3; ++column)
            directions(row, column) = values[3 * row + column];
    Matrix_<Cell_> steps(1, 2);
    steps(0, 0) = 0.01;
    steps(0, 1) = 0.02;
    Matrix_<Cell_> settings(2, 2);
    settings(0, 0) = "numeric_payload_budget_bytes";
    settings(0, 1) = 0.0;
    settings(1, 0) = "recording_capacity_budget_bytes";
    settings(1, 1) = 16.0;
    Handle_<StorableBumpOverAADRequest_> request;
    BumpOverAADRequest_New("owned", directions, steps, settings, &request);
    directions(0, 0) = 99.0;
    steps(0, 0) = 99.0;
    settings(0, 1) = 99.0;
    Matrix_<Cell_> actual;
    BumpOverAADRequest_Get_Directions(request, &actual);
    ASSERT_EQ(actual.Rows(), 2);
    ASSERT_EQ(actual.Cols(), 3);
    for (int row = 0; row < 2; ++row)
        for (int column = 0; column < 3; ++column)
            ASSERT_DOUBLE_EQ(Cell::ToDouble(actual(row, column)), values[3 * row + column]);
    actual(0, 0) = 99.0;
    BumpOverAADRequest_Get_Directions(request, &actual);
    ASSERT_DOUBLE_EQ(Cell::ToDouble(actual(0, 0)), 1.0);
    BumpOverAADRequest_Get_Steps(request, &actual);
    ASSERT_EQ(actual.Rows(), 2);
    ASSERT_EQ(actual.Cols(), 1);
    ASSERT_DOUBLE_EQ(Cell::ToDouble(actual(0, 0)), 0.01);
    ASSERT_DOUBLE_EQ(Cell::ToDouble(actual(1, 0)), 0.02);
    BumpOverAADRequest_Get_Settings(request, &actual);
    ASSERT_EQ(actual.Rows(), 3);
    ASSERT_DOUBLE_EQ(Cell::ToDouble(actual(0, 1)), 3.0);
    ASSERT_DOUBLE_EQ(Cell::ToDouble(actual(1, 1)), 0.0);
    ASSERT_DOUBLE_EQ(Cell::ToDouble(actual(2, 1)), 16.0);
    BumpOverAADRequest_Get_Shape(request, &actual);
    ASSERT_EQ(actual.Rows(), 1);
    ASSERT_EQ(actual.Cols(), 2);
    ASSERT_DOUBLE_EQ(Cell::ToDouble(actual(0, 0)), 2.0);
    ASSERT_DOUBLE_EQ(Cell::ToDouble(actual(0, 1)), 3.0);
}

TEST(ExcelCurvatureRequestTest, TestEmptyDirectionsRetainLogicalInputCountAndUnsetCaps) {
    Handle_<StorableBumpOverAADRequest_> request;
    BumpOverAADRequest_New("gradient", {}, {}, Setting("input_count", Cell_(6.0)), &request);
    ASSERT_EQ(request->val_.directions_.Rows(), 0);
    ASSERT_EQ(request->val_.directions_.Cols(), 6);
    ASSERT_TRUE(request->val_.steps_.empty());
    Matrix_<Cell_> cells;
    BumpOverAADRequest_Get_Directions(request, &cells);
    ASSERT_EQ(cells.Rows(), 1);
    ASSERT_EQ(cells.Cols(), 1);
    ASSERT_TRUE(Cell::IsEmpty(cells(0, 0)));
    BumpOverAADRequest_Get_Steps(request, &cells);
    ASSERT_TRUE(Cell::IsEmpty(cells(0, 0)));
    BumpOverAADRequest_Get_Shape(request, &cells);
    ASSERT_DOUBLE_EQ(Cell::ToDouble(cells(0, 0)), 0.0);
    ASSERT_DOUBLE_EQ(Cell::ToDouble(cells(0, 1)), 6.0);
    BumpOverAADRequest_Get_Settings(request, &cells);
    ASSERT_TRUE(Cell::IsEmpty(cells(1, 1)));
    ASSERT_TRUE(Cell::IsEmpty(cells(2, 1)));
    BumpOverAADRequest_New("typed", Matrix_<Cell_>(0, 6), {}, {}, &request);
    ASSERT_EQ(request->val_.directions_.Cols(), 6);
    const auto previous = request;
    ASSERT_THROW(BumpOverAADRequest_New("mismatch", Matrix_<Cell_>(0, 6), {}, Setting("input_count", Cell_(5.0)), &request), Exception_);
    ASSERT_EQ(request, previous);
    BumpOverAADRequest_New("blank", Matrix_<Cell_>(1, 1), Matrix_<Cell_>(1, 1), {}, &request);
    ASSERT_EQ(request->val_.directions_.Rows(), 0);
    ASSERT_EQ(request->val_.directions_.Cols(), 0);
}

TEST(ExcelCurvatureRequestTest, TestDirectionKindsRejectWithLocationAndPreserveOutput) {
    Handle_<StorableBumpOverAADRequest_> request;
    BumpOverAADRequest_New("previous", {}, {}, {}, &request);
    const auto previous = request;
    const Vector_<Cell_> invalid{Cell_(true),
                                 Cell_("1"),
                                 Cell_(Date_(2026, 10, 11)),
                                 Cell_(),
                                 Cell_(std::numeric_limits<double>::infinity()),
                                 Cell_(std::numeric_limits<double>::quiet_NaN())};
    Matrix_<Cell_> directions(1, 2, Cell_(1.0));
    const Matrix_<Cell_> steps(1, 1, Cell_(0.01));
    for (const auto& value : invalid) {
        directions(0, 1) = value;
        ASSERT_NO_FATAL_FAILURE(AssertError([&] { BumpOverAADRequest_New("invalid", directions, steps, {}, &request); },
                                            {"BumpOverAADRequest_New", "directions", "row=1 column=2"}));
        ASSERT_EQ(request, previous);
    }
}

TEST(ExcelCurvatureRequestTest, TestStepKindsPositivityAndShapeRejectContextually) {
    const Matrix_<Cell_> directions(2, 2, Cell_(1.0));
    Matrix_<Cell_> steps(2, 1, Cell_(0.01));
    Handle_<StorableBumpOverAADRequest_> request;
    const Vector_<Cell_> invalid{Cell_(0.0),
                                 Cell_(-1.0),
                                 Cell_(false),
                                 Cell_(".01"),
                                 Cell_(Date_(2026, 10, 11)),
                                 Cell_(),
                                 Cell_(std::numeric_limits<double>::infinity()),
                                 Cell_(std::numeric_limits<double>::quiet_NaN())};
    for (const auto& value : invalid) {
        steps(1, 0) = value;
        ASSERT_NO_FATAL_FAILURE(AssertError([&] { BumpOverAADRequest_New("invalid", directions, steps, {}, &request); },
                                            {"BumpOverAADRequest_New", "steps", "row=2 column=1"}));
    }
    ASSERT_NO_FATAL_FAILURE(AssertError([&] { BumpOverAADRequest_New("shape", directions, Matrix_<Cell_>(2, 2, Cell_(0.01)), {}, &request); },
                                        {"steps", "row or column vector"}));
    ASSERT_NO_FATAL_FAILURE(
        AssertError([&] { BumpOverAADRequest_New("count", directions, Matrix_<Cell_>(1, 1, Cell_(0.01)), {}, &request); }, {"steps", "exactly one"}));
    ASSERT_NO_FATAL_FAILURE(AssertError(
        [&] { BumpOverAADRequest_New("columns", directions, Matrix_<Cell_>(2, 1, Cell_(0.01)), Setting("input_count", Cell_(3.0)), &request); },
        {"input_count", "direction columns"}));
}

TEST(ExcelCurvatureRequestTest, TestBudgetKindsBlankZeroAndExactIntegerLimit) {
    Handle_<StorableBumpOverAADRequest_> request;
    const Vector_<Cell_> invalid{Cell_(true),
                                 Cell_("16"),
                                 Cell_(Date_(2026, 10, 11)),
                                 Cell_(-1.0),
                                 Cell_(1.5),
                                 Cell_(9007199254740992.0),
                                 Cell_(std::numeric_limits<double>::infinity()),
                                 Cell_(std::numeric_limits<double>::quiet_NaN())};
    for (const char* key : {"numeric_payload_budget_bytes", "recording_capacity_budget_bytes"}) {
        for (const auto& value : invalid)
            ASSERT_NO_FATAL_FAILURE(AssertError([&] { BumpOverAADRequest_New("invalid", {}, {}, Setting(key, value), &request); },
                                                {"BumpOverAADRequest_New", "row=1 column=2", "exactly representable"}));
        BumpOverAADRequest_New("limit", {}, {}, Setting(key, Cell_(9007199254740991.0)), &request);
        Matrix_<Cell_> settings;
        BumpOverAADRequest_Get_Settings(request, &settings);
        const int row = String_(key) == "numeric_payload_budget_bytes" ? 1 : 2;
        ASSERT_DOUBLE_EQ(Cell::ToDouble(settings(row, 1)), 9007199254740991.0);
        BumpOverAADRequest_New("zero", {}, {}, Setting(key, Cell_(0.0)), &request);
        BumpOverAADRequest_Get_Settings(request, &settings);
        ASSERT_DOUBLE_EQ(Cell::ToDouble(settings(row, 1)), 0.0);
        BumpOverAADRequest_New("unset", {}, {}, Setting(key, Cell_()), &request);
        BumpOverAADRequest_Get_Settings(request, &settings);
        ASSERT_TRUE(Cell::IsEmpty(settings(row, 1)));
    }
}

TEST(ExcelCurvatureRequestTest, TestInputCountBoundsUnknownDuplicateAndNulSettings) {
    Handle_<StorableBumpOverAADRequest_> request;
    BumpOverAADRequest_New("largest", {}, {}, Setting("input_count", Cell_(1048575.0)), &request);
    ASSERT_EQ(request->val_.directions_.Rows(), 0);
    ASSERT_EQ(request->val_.directions_.Cols(), 1048575);
    const auto previous = request;
    for (const auto& value : Vector_<Cell_>{Cell_(1048576.0), Cell_(-1.0), Cell_(1.5), Cell_(true), Cell_("6")})
        ASSERT_NO_FATAL_FAILURE(AssertError([&] { BumpOverAADRequest_New("invalid", {}, {}, Setting("input_count", value), &request); },
                                            {"BumpOverAADRequest_New", "input_count", "row=1 column=2"}));
    ASSERT_NO_FATAL_FAILURE(
        AssertError([&] { BumpOverAADRequest_New("unknown", {}, {}, Setting("unknown", Cell_(1.0)), &request); }, {"unknown key", "row=1 column=2"}));
    Matrix_<Cell_> duplicate(2, 2);
    duplicate(0, 0) = "input_count";
    duplicate(0, 1) = 6.0;
    duplicate(1, 0) = "INPUT_COUNT";
    duplicate(1, 1) = 6.0;
    ASSERT_NO_FATAL_FAILURE(AssertError([&] { BumpOverAADRequest_New("duplicate", {}, {}, duplicate, &request); },
                                        {"duplicate key INPUT_COUNT", "row=2 column=1", "first row=1"}));
    const String_ nul(std::string("bad\0name", 8));
    ASSERT_NO_FATAL_FAILURE(AssertError([&] { BumpOverAADRequest_New(nul, {}, {}, {}, &request); }, {"name", "embedded NUL"}));
    ASSERT_NO_FATAL_FAILURE(AssertError([&] { BumpOverAADRequest_New("nul", {}, {}, Setting(nul, Cell_(1.0)), &request); }, {"embedded NUL"}));
    ASSERT_THROW(BumpOverAADRequest_New("shape", {}, {}, Matrix_<Cell_>(1, 1, Cell_("input_count")), &request), Exception_);
    ASSERT_EQ(request, previous);
}

TEST(ExcelCurvatureRequestTest, TestNullGettersAndUnsupportedArchive) {
    Matrix_<Cell_> cells;
    const auto getters = {BumpOverAADRequest_Get_Directions, BumpOverAADRequest_Get_Steps, BumpOverAADRequest_Get_Settings,
                          BumpOverAADRequest_Get_Shape};
    for (const auto getter : getters)
        ASSERT_NO_FATAL_FAILURE(AssertError([&] { getter({}, &cells); }, {"BumpOverAADRequest_Get_", "request", "handle is null"}));
    Handle_<StorableBumpOverAADRequest_> request;
    BumpOverAADRequest_New("archive", {}, {}, {}, &request);
    ASSERT_EQ(request->Type(), "BumpOverAADRequest");
    ASSERT_NO_FATAL_FAILURE(AssertError([&] { JSON::WriteString(*request); }, {"RiskArchiveUnsupported"}));
}

TEST(ExcelCurvatureRequestTest, TestConstructorsAndGettersPreserveCallerGraphAndSeed) {
    using namespace AAD;
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
            Handle_<StorableBumpOverAADRequest_> request;
            BumpOverAADRequest_New("passive", {}, {}, Setting("input_count", Cell_(6.0)), &request);
            Matrix_<Cell_> cells;
            const auto getters = {BumpOverAADRequest_Get_Directions, BumpOverAADRequest_Get_Steps, BumpOverAADRequest_Get_Settings,
                                  BumpOverAADRequest_Get_Shape};
            for (const auto getter : getters)
                getter(request, &cells);
            const auto previous = request;
            ASSERT_THROW(BumpOverAADRequest_New("failure", {}, {}, Setting("input_count", Cell_(-1.0)), &request), Exception_);
            ASSERT_EQ(request, previous);
            BumpOverAADRequest_New("recovered", {}, {}, {}, &request);
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

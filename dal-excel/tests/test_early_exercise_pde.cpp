#include <gtest/gtest.h>

#include <cmath>

#include <dal-excel/src/__earlyexercisepde.hpp>

using namespace Dal;

namespace {
    struct Field_ {
        const char* first;
        Cell_ second;
        template <class T_> Field_(const char* key, const T_& value) : first(key), second(value) {}
    };
    Matrix_<Cell_> Rows(std::initializer_list<Field_> fields) {
        Matrix_<Cell_> result(static_cast<int>(fields.size()), 2);
        int i = 0;
        for (const auto& f : fields) {
            result(i, 0) = f.first;
            result(i++, 1) = f.second;
        }
        return result;
    }
    Matrix_<Cell_> WithoutHeader(const Matrix_<Cell_>& input) {
        Matrix_<Cell_> result(input.Rows() - 1, input.Cols());
        for (int i = 0; i < result.Rows(); ++i)
            for (int j = 0; j < result.Cols(); ++j)
                result(i, j) = input(i + 1, j);
        return result;
    }
    Handle_<StorableEarlyExercisePdeRequest_> Request(const char* style = "BERMUDAN", const Matrix_<Cell_>& extra = {}) {
        const auto today = Date_(2026, 10, 18), maturity = Date_(2027, 10, 18);
        auto contract = Rows({{"evaluation_date", double(Date::ToExcel(today))},
                              {"maturity_date", double(Date::ToExcel(maturity))},
                              {"strike", 100.0},
                              {"option_type", "PUT"},
                              {"exercise_style", style}});
        auto market = Rows({{"spot", 80.0}, {"volatility", 0.0}});
        Matrix_<Cell_> carry(1, 3), dates;
        carry(0, 0) = double(Date::ToExcel(maturity));
        carry(0, 1) = carry(0, 2) = 0.1;
        if (std::string(style) == "BERMUDAN") {
            dates.Resize(2, 1);
            dates(0, 0) = double(Date::ToExcel(Date_(2027, 3, 4)));
            dates(1, 0) = double(Date::ToExcel(maturity));
        }
        Handle_<StorableEarlyExercisePdeSettings_> settings;
        auto rows = extra.Rows() ? extra : Rows({{"grid_points", 41.0}, {"ordinary_steps", 6000.0}});
        EarlyExercisePdeSettings_New("settings", rows, &settings);
        Handle_<StorableEarlyExercisePdeRequest_> request;
        EarlyExercisePdeRequest_New("request", contract, market, carry, dates, settings, &request);
        return request;
    }
} // namespace

TEST(ExcelEarlyExercisePdeTest, TestIndependentPriceAndEveryOwningTable) {
    auto request = Request();
    Handle_<StorableEarlyExercisePdeResult_> result;
    EarlyExercisePdeResult_New("result", request, &result);
    request.reset();
    double price = 0.0;
    EarlyExercisePdeResult_Get_Price(result, &price);
    ASSERT_NEAR(price, 20.0 * std::exp(-0.1 * 137.0 / 365.0), 1e-4);
    Matrix_<Cell_> cells;
    const auto getters = {EarlyExercisePdeResult_Get_Summary,   EarlyExercisePdeResult_Get_Grid,           EarlyExercisePdeResult_Get_TimeGrid,
                          EarlyExercisePdeResult_Get_Steps,     EarlyExercisePdeResult_Get_ExerciseLevels, EarlyExercisePdeResult_Get_ExerciseRegions,
                          EarlyExercisePdeResult_Get_Execution, EarlyExercisePdeResult_Get_Failure};
    for (auto getter : getters) {
        getter(result, &cells);
        ASSERT_GT(cells.Rows(), 0);
        ASSERT_TRUE(Cell::IsString(cells(0, 0)));
    }
    EarlyExercisePdeResult_Get_TimeGrid(result, &cells);
    ASSERT_EQ(Cell::ToString(cells(0, 0)), "TimeIndex");
    ASSERT_DOUBLE_EQ(Cell::ToDouble(cells(1, 0)), 0.0);
    ASSERT_DOUBLE_EQ(Cell::ToDouble(cells(1, 1)), 0.0);
    EarlyExercisePdeResult_Get_Request(result, &request);
    const auto inputGetters = {EarlyExercisePdeRequest_Get_Contract, EarlyExercisePdeRequest_Get_Market, EarlyExercisePdeRequest_Get_Carry,
                               EarlyExercisePdeRequest_Get_ExerciseDates};
    for (auto getter : inputGetters) {
        getter(request, &cells);
        ASSERT_GT(cells.Rows(), 0);
        ASSERT_TRUE(Cell::IsString(cells(0, 0)));
    }
    Handle_<StorableEarlyExercisePdeSettings_> settings;
    EarlyExercisePdeRequest_Get_Settings(request, &settings);
    EarlyExercisePdeSettings_Get_Configuration(settings, &cells);
    ASSERT_EQ(cells.Rows(), 15);
    ASSERT_EQ(settings->val_.theta_, 1.0);
    ASSERT_TRUE(settings->val_.upper_);
}

TEST(ExcelEarlyExercisePdeTest, TestStrictCellTypesPartialEmptyRowsAndAtomicHandles) {
    Handle_<StorableEarlyExercisePdeSettings_> settings;
    EarlyExercisePdeSettings_New("ok", {}, &settings);
    const auto previous = settings;
    for (const Cell_& bad : {Cell_(true), Cell_("401"), Cell_(401.5), Cell_()}) {
        ASSERT_THROW(EarlyExercisePdeSettings_New("bad", Rows({{"grid_points", bad}}), &settings), Exception_);
        ASSERT_EQ(previous, settings);
    }
    auto halfEmpty = Rows({{"theta", 1.0}, {"", Cell_()}});
    ASSERT_THROW(EarlyExercisePdeSettings_New("bad", halfEmpty, &settings), Exception_);
    auto duplicate = Rows({{"theta", 1.0}, {"THETA", 1.0}});
    ASSERT_THROW(EarlyExercisePdeSettings_New("bad", duplicate, &settings), Exception_);
    ASSERT_THROW(EarlyExercisePdeSettings_New("bad", Rows({{"unknown", 1.0}}), &settings), Exception_);
    const auto zero = Request("AMERICAN", Rows({{"max_work_units", 0.0}}));
    Handle_<StorableEarlyExercisePdeResult_> failed;
    EarlyExercisePdeResult_New("failed", zero, &failed);
    // Zero solves may be legitimate for a diagonal obstacle already satisfied.
    ASSERT_TRUE(failed->val_.IsSuccess());
    auto input = zero->val_;
    input.market_.volatility_ = 0.2;
    Handle_<StorableEarlyExercisePdeRequest_> stochastic(new StorableEarlyExercisePdeRequest_("stochastic", input));
    EarlyExercisePdeResult_New("failed", stochastic, &failed);
    ASSERT_EQ(failed->val_.Status(), EarlyExercisePdeStatus_("WORK_LIMIT"));
    double price;
    ASSERT_THROW(EarlyExercisePdeResult_Get_Price(failed, &price), Exception_);
    Matrix_<Cell_> grid;
    EarlyExercisePdeResult_Get_Grid(failed, &grid);
    ASSERT_TRUE(Cell::IsEmpty(grid(1, 2)));
}

TEST(ExcelEarlyExercisePdeTest, TestRoundTripDatesCarryAndStrictRequestTables) {
    auto request = Request();
    const auto previous = request;
    Matrix_<Cell_> contract, market, carry, dates;
    EarlyExercisePdeRequest_Get_Contract(request, &contract);
    EarlyExercisePdeRequest_Get_Market(request, &market);
    EarlyExercisePdeRequest_Get_Carry(request, &carry);
    EarlyExercisePdeRequest_Get_ExerciseDates(request, &dates);
    contract = WithoutHeader(contract);
    market = WithoutHeader(market);
    carry = WithoutHeader(carry);
    dates = WithoutHeader(dates);
    EarlyExercisePdeRequest_New("copy", contract, market, carry, dates, {}, &request);
    ASSERT_NE(request.get(), previous.get());
    ASSERT_EQ(request->val_.contract_.exerciseDates_, previous->val_.contract_.exerciseDates_);
    ASSERT_DOUBLE_EQ(request->val_.market_.carry_.front().rate_, 0.1);
    const auto valid = request;
    for (const Cell_& invalid : {Cell_(true), Cell_("2026-10-18"), Cell_(Date_(2026, 10, 18)), Cell_(46313.5), Cell_()}) {
        auto bad = contract;
        bad(0, 1) = invalid;
        ASSERT_THROW(EarlyExercisePdeRequest_New("bad", bad, market, carry, dates, {}, &request), Exception_);
        ASSERT_EQ(request, valid);
    }
    auto badCarry = carry;
    badCarry(0, 1) = true;
    ASSERT_THROW(EarlyExercisePdeRequest_New("bad", contract, market, badCarry, dates, {}, &request), Exception_);
    auto badDates = dates;
    badDates(0, 0) = Cell_();
    ASSERT_THROW(EarlyExercisePdeRequest_New("bad", contract, market, carry, badDates, {}, &request), Exception_);
    auto emptyRow = market;
    emptyRow.Resize(3, 2);
    ASSERT_THROW(EarlyExercisePdeRequest_New("bad", contract, emptyRow, carry, dates, {}, &request), Exception_);
    ASSERT_EQ(request, valid);
}

TEST(ExcelEarlyExercisePdeTest, TestEntireBlankOptionalRangeUsesDefaults) {
    Handle_<StorableEarlyExercisePdeSettings_> settings;
    ASSERT_NO_THROW(EarlyExercisePdeSettings_New("blank", Matrix_<Cell_>(2, 3), &settings));
    ASSERT_EQ(settings->val_.gridPoints_, 401);
    ASSERT_EQ(settings->val_.theta_, 1.0);
}

TEST(ExcelEarlyExercisePdeTest, TestTablesPreserveNativePricesDatesAndMissingDiagnostics) {
    const auto request = Request();
    const auto expected = EvaluateEarlyExercisePde(request->val_);
    Handle_<StorableEarlyExercisePdeResult_> result;
    EarlyExercisePdeResult_New("parity", request, &result);
    Matrix_<Cell_> cells;
    EarlyExercisePdeResult_Get_Grid(result, &cells);
    ASSERT_EQ(cells.Rows(), static_cast<int>(expected.Grid().size()) + 1);
    for (int row = 1; row < cells.Rows(); ++row) {
        ASSERT_DOUBLE_EQ(Cell::ToDouble(cells(row, 1)), expected.Grid()[row - 1]);
        ASSERT_DOUBLE_EQ(Cell::ToDouble(cells(row, 2)), expected.Values()[row - 1]);
    }
    EarlyExercisePdeResult_Get_TimeGrid(result, &cells);
    for (int row = 1; row < cells.Rows(); ++row) {
        const auto& time = expected.TimeGrid()[row - 1];
        ASSERT_DOUBLE_EQ(Cell::ToDouble(cells(row, 0)), row - 1);
        ASSERT_DOUBLE_EQ(Cell::ToDouble(cells(row, 1)), time.timeYears_);
        ASSERT_EQ(Cell::IsEmpty(cells(row, 2)), !time.calendarDate_);
        if (time.calendarDate_)
            ASSERT_DOUBLE_EQ(Cell::ToDouble(cells(row, 2)), Date::ToExcel(*time.calendarDate_));
    }
    EarlyExercisePdeResult_Get_Steps(result, &cells);
    for (int row = 1; row < cells.Rows(); ++row) {
        const auto& step = expected.StepDiagnostics()[row - 1];
        ASSERT_DOUBLE_EQ(Cell::ToDouble(cells(row, 2)), step.timeIndex_);
        ASSERT_DOUBLE_EQ(Cell::ToDouble(cells(row, 4)), step.dt_);
        ASSERT_TRUE(Cell::IsEmpty(cells(row, 15)));
    }
    double price = 0.0;
    EarlyExercisePdeResult_Get_Price(result, &price);
    ASSERT_DOUBLE_EQ(price, expected.Price());
}

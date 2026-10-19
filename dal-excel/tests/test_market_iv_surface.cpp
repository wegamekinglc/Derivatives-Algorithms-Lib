#include <gtest/gtest.h>

#include <dal-excel/src/__marketivsurface.hpp>

using namespace Dal;

TEST(ExcelMarketIvSurfaceTest, TestOwningFitAndBlankSettings) {
    const Date_ d(2026, 10, 19), expiry(2027, 10, 19);
    Handle_<StorableMarketIvFitSettings_> settings;
    MarketIvFitSettings_New("defaults", Matrix_<Cell_>(2, 3), &settings);
    ASSERT_EQ(settings->val_.maxModelRowEvaluations_, 5000000);
    MarketIvFitSettings_New("column", Matrix_<Cell_>(6, 1), &settings);
    auto invalid = Matrix_<Cell_>(2, 3);
    invalid(0, 0) = "rho_limit";
    ASSERT_THROW(MarketIvFitSettings_New("bad", invalid, &settings), Exception_);
    Matrix_<Cell_> dates(1, 1);
    dates(0, 0) = double(Date::ToExcel(expiry));
    Handle_<StorableMarketIvMarket_> market;
    MarketIvFlatMarket_New("market", Date::ToExcel(d), 100., 0., 0., dates, &market);
    Matrix_<Cell_> quotes(3, 9);
    for (int i = 0; i < 3; ++i) {
        quotes(i, 0) = String_(std::to_string(i));
        quotes(i, 1) = double(Date::ToExcel(expiry));
        quotes(i, 2) = 90. + 10. * i;
        quotes(i, 3) = "CALL";
        quotes(i, 4) = "DECIMAL_IV";
        quotes(i, 5) = .2;
    }
    Handle_<StorableMarketIvFitRequest_> request;
    MarketIvFitRequest_New("request", market, quotes, settings, &request);
    Handle_<StorableMarketIvFitResult_> fit;
    MarketIvFitResult_New("fit", request, &fit);
    ASSERT_TRUE(fit->val_.IsSuccess());
    Handle_<StorableMarketIvCalibrationSnapshot_> snapshot;
    MarketIvFitResult_Get_Snapshot(fit, &snapshot);
    Matrix_<Cell_> copied;
    for (auto getter :
         {MarketIvFitResult_Get_Summary, MarketIvFitResult_Get_Quotes, MarketIvFitResult_Get_Parameters, MarketIvFitResult_Get_Constraints,
          MarketIvFitResult_Get_Iterations, MarketIvFitResult_Get_Execution, MarketIvFitResult_Get_Failure}) {
        getter(fit, &copied);
        ASSERT_GT(copied.Rows(), 0);
        ASSERT_TRUE(Cell::IsString(copied(0, 0)));
    }
    for (auto getter : {MarketIvCalibrationSnapshot_Get_Terms, MarketIvCalibrationSnapshot_Get_Theta, MarketIvCalibrationSnapshot_Get_Certificate}) {
        getter(snapshot, &copied);
        ASSERT_GT(copied.Rows(), 1);
        ASSERT_TRUE(Cell::IsString(copied(0, 0)));
    }
    request.reset();
    market.reset();
    fit.reset();
    Handle_<StorableMarketIvQuery_> query;
    MarketIvQuery_New("query", 100., .5, &query);
    Handle_<StorableMarketIvPointResult_> point;
    MarketIvPointResult_New("point", snapshot, query, {}, &point);
    ASSERT_NEAR(*point->val_.LocalVol(), .2, 1e-10);
    Matrix_<Cell_> cells;
    MarketIvPointResult_Get_Values(point, &cells);
    ASSERT_NEAR(Cell::ToDouble(cells(14, 1)), .2, 1e-10);
    quotes(0, 2) = true;
    MarketIvFlatMarket_New("market", Date::ToExcel(d), 100., 0., 0., dates, &market);
    ASSERT_THROW(MarketIvFitRequest_New("bad", market, quotes, {}, &request), Exception_);
}

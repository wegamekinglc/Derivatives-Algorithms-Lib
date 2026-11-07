#include <gtest/gtest.h>

#include <dal-excel/src/__compoundedois.hpp>
#include <dal-excel/src/__script_test_api.hpp>
#include <dal-excel/src/__scriptcurvequoterisk.hpp>
#include <dal-public/tests/scriptquoteriskfixture.hpp>

using namespace Dal;

namespace {
    Matrix_<Cell_> Observations() {
        Matrix_<Cell_> cells(2, 7);
        const Vector_<String_> columns{"coupon_id", "rate_end", "fixing_time", "start", "accrual", "end", "rate_start"};
        for (int j = 0; j < 7; ++j)
            cells(0, j) = columns[j];
        cells(1, 0) = "coupon";
        cells(1, 1) = "2024-03-05";
        cells(1, 2) = "2024-03-01";
        cells(1, 3) = "2024-03-01";
        cells(1, 4) = 4. / 360.;
        cells(1, 5) = "2024-03-05";
        cells(1, 6) = "2024-03-01";
        return cells;
    }
} // namespace

namespace {
    template <class T_> String_ StoreOis(const Handle_<T_>& value) { return Excel::ScriptTestStoreRepository(Handle_<Storable_>(value)); }
    Matrix_<Cell_> OisFields(std::initializer_list<std::pair<const char*, Cell_>> rows) {
        Matrix_<Cell_> cells(static_cast<int>(rows.size()), 2);
        int i = 0;
        for (const auto& row : rows) {
            cells(i, 0) = row.first;
            cells(i++, 1) = row.second;
        }
        return cells;
    }
    std::pair<Handle_<CalibrationInstrument_>, CurveScriptEvaluationRequest_> ExcelOis(bool missing = false, double historical = .021) {
        Excel::ScriptTestInitialize(1);
        const Date_ today(2024, 3, 3), start(2024, 3, 1), end(2024, 3, 6), payment(2024, 3, 8);
        auto observationCells = Observations();
        observationCells.Resize(3, 7);
        observationCells(2, 0) = "coupon";
        observationCells(2, 1) = "2024-03-06";
        observationCells(2, 2) = "2024-03-05";
        observationCells(2, 3) = "2024-03-05";
        observationCells(2, 4) = 1. / 360.;
        observationCells(2, 5) = "2024-03-06";
        observationCells(2, 6) = "2024-03-05";
        Handle_<StorableCompoundedOisObservations_> observations;
        CompoundedOisObservations_New(observationCells, &observations);
        Matrix_<Cell_> fixedCells(2, 4), couponCells(2, 4);
        const char* fixedColumns[] = {"payment", "accrual", "end", "start"};
        const char* couponColumns[] = {"end", "coupon_id", "payment", "start"};
        for (int j = 0; j < 4; ++j) {
            fixedCells(0, j) = fixedColumns[j];
            couponCells(0, j) = couponColumns[j];
        }
        fixedCells(1, 0) = payment;
        fixedCells(1, 1) = 5. / 365.;
        fixedCells(1, 2) = end;
        fixedCells(1, 3) = start;
        couponCells(1, 0) = end;
        couponCells(1, 1) = "coupon";
        couponCells(1, 2) = payment;
        couponCells(1, 3) = start;
        Handle_<StorableCompoundedOisFixedLeg_> fixed;
        Handle_<StorableCompoundedOisCoupons_> coupons;
        CompoundedOisFixedLeg_New(fixedCells, &fixed);
        CompoundedOisCoupons_New(couponCells, &coupons);
        Handle_<StorableCalibrationDates_> holidays;
        CalibrationDates_New({Cell_(Date_(2024, 3, 4))}, &holidays);
        Handle_<StorableCompoundedOisCalendar_> calendar;
        CompoundedOisCalendar_New(OisFields({{"center", Cell_("explicit")}, {"holidays", Cell_(StoreOis(holidays))}}), &calendar);
        Handle_<StorableCompoundedOisScriptSpec_> spec;
        CompoundedOisScriptSpec_New(OisFields({{"instrument_id", Cell_("excel-ois")},
                                               {"direction", Cell_("PayFixed")},
                                               {"discount_route", Cell_("USD.OIS")},
                                               {"projection_route", Cell_("USD.SOFR")},
                                               {"fixing_index", Cell_("USD-SOFR")},
                                               {"notional", Cell_(100.)},
                                               {"coupon", Cell_(.02)},
                                               {"currency", Cell_("USD")},
                                               {"collateral_currency", Cell_("USD")},
                                               {"collateral_type", Cell_("OIS")},
                                               {"observation_basis", Cell_("ACT_360")},
                                               {"fixed_basis", Cell_("ACT_365F")},
                                               {"calendar", Cell_(StoreOis(calendar))},
                                               {"fixed_leg", Cell_(StoreOis(fixed))},
                                               {"coupons", Cell_(StoreOis(coupons))},
                                               {"observations", Cell_(StoreOis(observations))}}),
                                    &spec);
        ScriptValuationSettings_ v;
        v.evaluationDate_ = today;
        v.fixings_.reset(new MarketFixingSnapshot_(missing ? MarketFixingSnapshot_::values_t{}
                                                           : MarketFixingSnapshot_::values_t{{"USD-SOFR", {{DateTime_(start), historical}}}}));
        const Handle_<StorableScriptValuationSettings_> valuation(new StorableScriptValuationSettings_("", v));
        Handle_<StorablePreparedCompoundedOis_> prepared;
        CompoundedOis_Prepare(spec, valuation, &prepared);
        Handle_<CalibrationInstrument_> instrument;
        CompoundedOisScriptInstrument_New(prepared, &instrument);
        Handle_<StorableCalibrationDates_> knots;
        CalibrationDates_New({Cell_(today), Cell_(today.AddDays(20))}, &knots);
        Handle_<StorableCurveInstrumentGeometry_> geometry;
        CurveInstrumentGeometry_New(OisFields({{"today", Cell_(today)},
                                               {"currency", Cell_("USD")},
                                               {"knot_dates", Cell_(StoreOis(knots))},
                                               {"parameterization", Cell_("LOG_DISCOUNT")},
                                               {"calibrate_discount_curve", Cell_(false)},
                                               {"target_overnight_route", Cell_("USD.SOFR")}}),
                                    &geometry);
        Matrix_<Cell_> routeCells(3, 6);
        const char* routeColumns[] = {"id", "projection", "collateral", "tenor", "overnight", "use_target_curve"};
        for (int j = 0; j < 6; ++j)
            routeCells(0, j) = routeColumns[j];
        for (int i = 1; i < 3; ++i) {
            routeCells(i, 0) = i == 1 ? "USD.OIS" : "USD.SOFR";
            routeCells(i, 1) = i == 2;
            routeCells(i, 2) = "OIS";
            routeCells(i, 3) = "";
            routeCells(i, 4) = i == 2;
            routeCells(i, 5) = i == 1;
        }
        Handle_<StorableCurveScriptRoutes_> routes;
        CurveScriptOvernightRoutes_New(routeCells, &routes);
        Handle_<StorableCurveInstrumentMarket_> market;
        CurveInstrumentMarket_New(OisFields({{"routes", Cell_(StoreOis(routes))}}), &market);
        Handle_<StorableCalibrationNumbers_> parameters;
        CalibrationNumbers_New({Cell_(-.002)}, &parameters);
        Handle_<StorableCurveScriptEvaluationRequest_> request;
        CurveScriptEvaluationRequest_New(OisFields({{"geometry", Cell_(StoreOis(geometry))},
                                                    {"market", Cell_(StoreOis(market))},
                                                    {"valuation", Cell_(StoreOis(valuation))},
                                                    {"parameters", Cell_(StoreOis(parameters))},
                                                    {"compounded_ois", Cell_(StoreOis(prepared))}}),
                                         &request);
        return {instrument, request->val_};
    }
    Vector_<> ExcelOisOracle(double theta) {
        const double floating = (1. + .021 * 4. / 360.) * std::exp(-theta / 20.) - 1.;
        return {100. * (floating - .02 * 5. / 365.) * std::exp(theta * 5. / 20.), floating / (5. / 365.)};
    }
} // namespace

TEST(CompoundedOisScriptExcelTest, TestFactoriesEvaluationOracleNativeAxesAndOwnership) {
    auto input = ExcelOis();
    Handle_<StorableCalibrationOutcome_> outcome;
    for (const auto method : {"None", "AAD", "FiniteDifference"}) {
        input.second.derivativeMethod_ = CalibrationDerivative_(method);
        const Handle_<StorableCurveScriptEvaluationRequest_> request(new StorableCurveScriptEvaluationRequest_(input.second));
        CurveScriptInstrument_Evaluate(input.first, request, &outcome);
        ASSERT_EQ(outcome->status_, "Evaluated");
        const auto expected = ExcelOisOracle(-.002);
        double price;
        CurveScriptEvaluation_Get_Value(outcome, "Price", &price);
        ASSERT_NEAR(price, expected[0], 1e-10);
        CurveScriptEvaluation_Get_Value(outcome, "ParRate", &price);
        ASSERT_NEAR(price, expected[1], 1e-10);
        Matrix_<> partials;
        if (input.second.derivativeMethod_ == CalibrationDerivative_::Value_::None)
            ASSERT_THROW(CurveScriptEvaluation_Get_NativePartials(outcome, &partials), Exception_);
        else {
            CurveScriptEvaluation_Get_NativePartials(outcome, &partials);
            for (const double h : {1e-4, 5e-5}) {
                const auto plus = ExcelOisOracle(-.002 + h), minus = ExcelOisOracle(-.002 - h);
                for (int i = 0; i < 2; ++i) {
                    const double fd = (plus[i] - minus[i]) / (2. * h);
                    ASSERT_NEAR(partials(i, 0), fd, 1e-7 + 1e-5 * std::abs(fd));
                }
            }
        }
        ASSERT_EQ(outcome->curveScript_->freeParameters_.size(), 1u);
    }
    input = {};
    Matrix_<Cell_> table;
    CurveScriptEvaluation_Get_Table(outcome, "observations", &table);
    ASSERT_EQ(table.Rows(), 3);
    ASSERT_EQ(Cell::ToString(table(1, 6)), "Historical");
    ASSERT_EQ(Cell::ToString(table(2, 6)), "Projection");
    ASSERT_FALSE(outcome->report_->QuoteRiskAvailable());
}

TEST(CompoundedOisScriptExcelTest, TestMissingFixingOwnsFailedReportButMalformedContextThrows) {
    Handle_<StorableCalibrationOutcome_> outcome;
    {
        const auto input = ExcelOis(true);
        const Handle_<StorableCurveScriptEvaluationRequest_> request(new StorableCurveScriptEvaluationRequest_(input.second));
        CurveScriptInstrument_Evaluate(input.first, request, &outcome);
    }
    ASSERT_EQ(outcome->status_, "Failed");
    ASSERT_EQ(outcome->report_->Errors()[0].code_, "MissingFixing");
    ASSERT_EQ(outcome->report_->Errors()[0].instrumentId_, "excel-ois");
    double value;
    Matrix_<> partials;
    ASSERT_THROW(CurveScriptEvaluation_Get_Value(outcome, "Price", &value), Exception_);
    ASSERT_THROW(CurveScriptEvaluation_Get_NativePartials(outcome, &partials), Exception_);
    {
        const auto numerical = ExcelOis(false, -1000.);
        const Handle_<StorableCurveScriptEvaluationRequest_> badFactor(new StorableCurveScriptEvaluationRequest_(numerical.second));
        CurveScriptInstrument_Evaluate(numerical.first, badFactor, &outcome);
    }
    ASSERT_EQ(outcome->status_, "Failed");
    ASSERT_EQ(outcome->report_->Errors()[0].code_, "CalibrationNumericalFailure");
    ASSERT_FALSE(outcome->curveScript_);
    ASSERT_THROW(CurveScriptEvaluation_Get_NativePartials(outcome, &partials), Exception_);
    auto malformed = ExcelOis();
    malformed.second.geometry_.ccy_ = "EUR";
    const Handle_<StorableCurveScriptEvaluationRequest_> request(new StorableCurveScriptEvaluationRequest_(malformed.second));
    ASSERT_THROW(CurveScriptInstrument_Evaluate(malformed.first, request, &outcome), CalibrationError_);
}

TEST(CompoundedOisScriptExcelTest, TestTableHeadersDatesAndFieldsAfterBlankAreStrict) {
    Handle_<StorableCompoundedOisObservations_> result;
    auto cells = Observations();
    for (const auto header : {"unknown", "rate_end", "Coupon_id"}) {
        cells(0, 0) = header;
        ASSERT_THROW(CompoundedOisObservations_New(cells, &result), Exception_);
    }
    cells = Observations();
    for (const auto invalid : {Cell_(45352.5), Cell_("03/01/2024"), Cell_(true)}) {
        cells(1, 2) = invalid;
        ASSERT_THROW(CompoundedOisObservations_New(cells, &result), Exception_);
    }
    cells = Observations();
    cells(1, 2) = double(Date::ToExcel(Date_(2024, 3, 1)));
    CompoundedOisObservations_New(cells, &result);
    ASSERT_EQ(result->val_[0].fixingTime_, DateTime_(Date_(2024, 3, 1)));
    Matrix_<Cell_> fields(3, 2);
    fields(2, 0) = "unknown_after_blank";
    fields(2, 1) = true;
    Handle_<StorableCurveScriptEvaluationRequest_> request;
    try {
        CurveScriptEvaluationRequest_New(fields, &request);
        FAIL();
    } catch (const Exception_& error) {
        ASSERT_NE(std::string(error.what()).find("row=3 column=1"), std::string::npos);
        ASSERT_NE(std::string(error.what()).find("unknown_after_blank"), std::string::npos);
    }
}

TEST(CompoundedOisScriptExcelTest, TestCalibrationTableListAndP1AdmissionStaySeparate) {
    const auto input = ExcelOis();
    const auto& request = input.second;
    const Handle_<StorablePreparedCompoundedOis_> table(new StorablePreparedCompoundedOis_(request.compoundedOis_));
    Handle_<StorableCompoundedOisPreparations_> tables;
    CompoundedOisPreparations_New({Handle_<Storable_>(table)}, &tables);
    const auto quote = NewCalibrationMarketQuote("q", .025, CalibrationValueUnit_::Parse("Rate"));
    const Handle_<StorableCalibrationObservation_> observation(
        new StorableCalibrationObservation_(NewCalibrationObservation("par", input.first, "ParRate", quote, 1e-10)));
    Handle_<StorableCalibrationObservations_> observations;
    CalibrationObservations_New({Handle_<Storable_>(observation)}, &observations);
    const Handle_<StorableCurveInstrumentGeometry_> geometry(new StorableCurveInstrumentGeometry_(request.geometry_));
    const Handle_<StorableCurveInstrumentMarket_> market(new StorableCurveInstrumentMarket_(request.market_));
    const Handle_<StorableScriptValuationSettings_> valuation(new StorableScriptValuationSettings_("", request.valuation_));
    Handle_<StorableCurveInstrumentCalibrationSpec_> spec;
    CurveInstrumentCalibrationSpec_New(OisFields({{"geometry", Cell_(StoreOis(geometry))},
                                                  {"market", Cell_(StoreOis(market))},
                                                  {"valuation", Cell_(StoreOis(valuation))},
                                                  {"observations", Cell_(StoreOis(observations))},
                                                  {"compounded_ois", Cell_(StoreOis(tables))}}),
                                       &spec);
    Handle_<StorableCalibrationOutcome_> fit;
    Calibrate_CurveInstruments(spec, {}, &fit);
    ASSERT_EQ(fit->status_, "Succeeded");
    ASSERT_NEAR(fit->curveFit_->modelMeasures_[0], .025, 1e-10);
    const Handle_<StorableScriptCurveQuoteRiskAccuracy_> accuracy(new StorableScriptCurveQuoteRiskAccuracy_({{1e-12}, 1e-10, 1e-12}));
    Handle_<StorableScriptCurveQuoteRiskSnapshot_> snapshot;
    ScriptCurveQuoteRiskSnapshot_New(fit, accuracy, &snapshot);
    ASSERT_FALSE(snapshot->val_.RiskAvailable());
    ASSERT_EQ(snapshot->val_.Diagnostics()[0].code_, "ScriptCurveQuoteRiskUnsupportedCapability");

    const Handle_<StorableCurveInstrumentCalibrationSpec_> p1(new StorableCurveInstrumentCalibrationSpec_(Dal::Test::ScriptQuoteRiskSpec()));
    Calibrate_CurveInstruments(p1, {}, &fit);
    const Handle_<StorableScriptCurveQuoteRiskAccuracy_> p1Accuracy(new StorableScriptCurveQuoteRiskAccuracy_({{1e-12, 1e-12}, 1e-10, 1e-12}));
    ScriptCurveQuoteRiskSnapshot_New(fit, p1Accuracy, &snapshot);
    Handle_<StorableScriptCurveQuoteRiskResult_> risk;
    CalibrationInstrument_ValueWithQuoteRisk(input.first, snapshot, p1, {}, &risk);
    ASSERT_FALSE(risk->val_.Available());
    ASSERT_EQ(risk->val_.Diagnostics()[0].code_, "ScriptCurveQuoteRiskUnsupportedCapability");
}

TEST(CompoundedOisScriptExcelTest, TestOvernightRoutesRequireTenorColumnButAcceptBlankCell) {
    Matrix_<Cell_> cells(2, 6);
    const char* columns[] = {"id", "projection", "collateral", "tenor", "overnight", "use_target_curve"};
    for (int j = 0; j < 6; ++j)
        cells(0, j) = columns[j];
    cells(1, 0) = "USD.SOFR";
    cells(1, 1) = true;
    cells(1, 2) = "OIS";
    cells(1, 4) = true;
    cells(1, 5) = false;
    Handle_<StorableCurveScriptRoutes_> routes;
    CurveScriptOvernightRoutes_New(cells, &routes);
    ASSERT_EQ(routes->val_.size(), 1u);
    ASSERT_EQ(routes->val_[0].tenor_, PeriodLength_());
    cells(1, 3) = "3m";
    ASSERT_THROW(CurveScriptOvernightRoutes_New(cells, &routes), Exception_);
}

TEST(CompoundedOisScriptExcelTest, TestNamedColumnsAndStrictDates) {
    auto cells = Observations();
    Handle_<StorableCompoundedOisObservations_> result;
    CompoundedOisObservations_New(cells, &result);
    ASSERT_EQ(result->val_.size(), 1u);
    ASSERT_EQ(result->val_[0].rateEnd_, Date_(2024, 3, 5));
    ASSERT_EQ(result->val_[0].fixingTime_, DateTime_(Date_(2024, 3, 1)));
    cells(1, 2) = DateTime_(Date_(2024, 3, 1), .5);
    ASSERT_THROW(CompoundedOisObservations_New(cells, &result), Exception_);
}

TEST(CompoundedOisScriptExcelTest, TestFullSelectionAfterBlankRowAndOriginalCoordinates) {
    auto cells = Observations();
    cells.Resize(4, 7);
    cells(3, 0) = "coupon";
    cells(3, 1) = "2024-03-05";
    cells(3, 2) = "2024-03-01";
    cells(3, 3) = "2024-03-01";
    cells(3, 4) = true;
    cells(3, 5) = "2024-03-05";
    cells(3, 6) = "2024-03-01";
    Handle_<StorableCompoundedOisObservations_> result;
    try {
        CompoundedOisObservations_New(cells, &result);
        FAIL() << "malformed row after blank accepted";
    } catch (const Exception_& error) {
        ASSERT_NE(std::string(error.what()).find("row=4"), std::string::npos);
        ASSERT_NE(std::string(error.what()).find("column=5"), std::string::npos);
    }
}

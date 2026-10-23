#include <gtest/gtest.h>

#include "xccyinstrumentfixture.hpp"
#include <dal-excel/src/__script_test_api.hpp>
#include <dal-excel/src/__xccyinstrument.hpp>
#include <dal-public/src/global.hpp>

using namespace Dal;

namespace {
    template <class T_> String_ StoreXccy(const Handle_<T_>& value) { return Excel::ScriptTestStoreRepository(handle_cast<Storable_>(value)); }
    Matrix_<Cell_> XccyFields(std::initializer_list<std::pair<const char*, Cell_>> fields) {
        Matrix_<Cell_> result(static_cast<int>(fields.size()), 2);
        int row = 0;
        for (const auto& field : fields) {
            result(row, 0) = field.first;
            result(row++, 1) = field.second;
        }
        return result;
    }
} // namespace

TEST(ExcelXccyInstrumentTest, TestSolveAndOwnedFailureOutcomes) {
    InitGlobalData();
    Excel::ScriptTestInitialize(1);
    auto spec = TestXccy::Spec();
    Handle_<StorableXccyInstrumentCalibrationSpec_> input(new StorableXccyInstrumentCalibrationSpec_(spec));
    Handle_<StorableCalibrationOutcome_> outcome;
    Calibrate_XccyInstruments(input, {}, &outcome);
    ASSERT_EQ(outcome->status_, "Succeeded");
    ASSERT_TRUE(outcome->curve_);
    ASSERT_EQ(outcome->report_->Data().summary_.domain_, "Xccy");
    spec.solver_.maxEvaluations_ = 1;
    input.reset(new StorableXccyInstrumentCalibrationSpec_(spec));
    Calibrate_XccyInstruments(input, {}, &outcome);
    input.reset();
    ASSERT_EQ(outcome->status_, "Failed");
    ASSERT_TRUE(outcome->curve_);
    ASSERT_EQ(outcome->report_->Errors()[0].code_, "CalibrationBudgetExceeded");
    ASSERT_EQ(outcome->report_->Axes().observations_[0]->Id(), "basis-row");
}

TEST(ExcelXccyInstrumentTest, TestStrictImportSettings) {
    Matrix_<Cell_> cells(3, 2);
    cells(0, 0) = "instrument_id";
    cells(0, 1) = "xccy";
    cells(1, 0) = "discount_route";
    cells(1, 1) = "USD.OIS";
    cells(2, 0) = "min_abs_annuity";
    cells(2, 1) = true;
    Handle_<StorableXccyScriptImportSettings_> settings;
    ASSERT_THROW(XccyScriptImportSettings_New(cells, &settings), Exception_);
    cells(2, 1) = 1e-12;
    XccyScriptImportSettings_New(cells, &settings);
    ASSERT_EQ(settings->val_.instrumentId_, "xccy");
    cells(2, 0) = "instrument_id";
    ASSERT_THROW(XccyScriptImportSettings_New(cells, &settings), Exception_);
    cells(2, 0) = "unknown";
    ASSERT_THROW(XccyScriptImportSettings_New(cells, &settings), Exception_);
}



TEST(ExcelXccyInstrumentTest, TestTypedFactoryRepositoryBridgeAndContextFailure) {
    InitGlobalData();
    Excel::ScriptTestInitialize(1);
    const auto input = TestXccy::Spec();
    const Handle_<StorableCurrencyPair_> pair(new StorableCurrencyPair_(input.context_.pair_));
    Handle_<StorableXccyScriptContext_> context;
    XccyScriptContext_New(XccyFields({{"valuation_time", Cell_(input.context_.valuationTime_)},
                                      {"pair", Cell_(StoreXccy(pair))},
                                      {"collateral_currency", Cell_("USD")},
                                      {"discount_route", Cell_("USD.OIS")}}),
                          &context);
    const Handle_<StorableCurveBlock_> domestic(new StorableCurveBlock_(input.domesticCurveBlock_));
    const Handle_<StorableCurveBlock_> foreign(new StorableCurveBlock_(input.foreignCurveBlock_));
    const Handle_<StorableCalibrationDates_> dates(new StorableCalibrationDates_(input.knotDates_));
    const Handle_<StorableCalibrationObservations_> rows(new StorableCalibrationObservations_(input.observations_));
    auto fields = XccyFields({{"context", Cell_(StoreXccy(context))},
                              {"domestic_curve_block", Cell_(StoreXccy(domestic))},
                              {"foreign_curve_block", Cell_(StoreXccy(foreign))},
                              {"fx_spot", Cell_(1.1)},
                              {"knot_dates", Cell_(StoreXccy(dates))},
                              {"observations", Cell_(StoreXccy(rows))}});
    Handle_<StorableXccyInstrumentCalibrationSpec_> spec;
    XccyInstrumentCalibrationSpec_New(fields, &spec);
    Handle_<StorableCalibrationOutcome_> outcome;
    Calibrate_XccyInstruments(spec, {}, &outcome);
    ASSERT_EQ(outcome->status_, "Succeeded");
    ASSERT_TRUE(outcome->curve_);
    fields(0, 1) = StoreXccy(domestic);
    ASSERT_THROW(XccyInstrumentCalibrationSpec_New(fields, &spec), Exception_);
    fields(0, 1) = StoreXccy(context);
    fields(3, 1) = true;
    ASSERT_THROW(XccyInstrumentCalibrationSpec_New(fields, &spec), Exception_);
    auto intraday = input.context_;
    intraday.valuationTime_ = DateTime_(TestXccy::TODAY, 9, 0);
    context.reset(new StorableXccyScriptContext_(intraday));
    fields(0, 1) = StoreXccy(context);
    fields(3, 1) = 1.1;
    XccyInstrumentCalibrationSpec_New(fields, &spec);
    Calibrate_XccyInstruments(spec, {}, &outcome);
    ASSERT_EQ(outcome->status_, "Failed");
    ASSERT_EQ(outcome->report_->Errors()[0].code_, "UnsupportedCalibrationFixingTime");
}

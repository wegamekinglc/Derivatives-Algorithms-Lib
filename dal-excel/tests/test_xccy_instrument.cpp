#include <gtest/gtest.h>

#include "xccyinstrumentfixture.hpp"
#include <dal-excel/src/__curvedata.hpp>
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
    Matrix_<Cell_> ExtraXccyField(const Matrix_<Cell_>& fields, const char* key, const Cell_& value) {
        Matrix_<Cell_> result(fields.Rows() + 1, 2);
        for (int i = 0; i < fields.Rows(); ++i)
            for (int j = 0; j < 2; ++j)
                result(i, j) = fields(i, j);
        result(fields.Rows(), 0) = key;
        result(fields.Rows(), 1) = value;
        return result;
    }
    void SwapXccyRows(Matrix_<Cell_>* fields, int first, int second) {
        for (int j = 0; j < 2; ++j)
            std::swap((*fields)(first, j), (*fields)(second, j));
    }
    template <class T_>
    void XccyFactoryError(void (*factory)(const Matrix_<Cell_>&, Handle_<T_>*),
                          const Matrix_<Cell_>& fields,
                          const char* function,
                          const char* key,
                          int row,
                          int column) {
        try {
            Handle_<T_> value;
            factory(fields, &value);
            FAIL() << "Expected strict worksheet input rejection";
        } catch (const Exception_& error) {
            const std::string text = error.what();
            ASSERT_NE(text.find(function), std::string::npos);
            ASSERT_NE(text.find("settings row=" + std::to_string(row) + " column=" + std::to_string(column)), std::string::npos);
            ASSERT_NE(text.find(std::string("field=") + key), std::string::npos);
        }
    }
    Matrix_<Cell_> XccyContextFields() {
        const auto spec = TestXccy::Spec();
        const Handle_<StorableCurrencyPair_> pair(new StorableCurrencyPair_(spec.context_.pair_));
        return XccyFields({{"valuation_time", Cell_(spec.context_.valuationTime_)},
                           {"pair", Cell_(StoreXccy(pair))},
                           {"collateral_currency", Cell_("USD")},
                           {"discount_route", Cell_("USD.OIS")}});
    }
    Matrix_<Cell_> XccySpecFields() {
        const auto spec = TestXccy::Spec();
        const Handle_<StorableXccyScriptContext_> context(new StorableXccyScriptContext_(spec.context_));
        const Handle_<StorableCurveBlock_> domestic(new StorableCurveBlock_(spec.domesticCurveBlock_));
        const Handle_<StorableCurveBlock_> foreign(new StorableCurveBlock_(spec.foreignCurveBlock_));
        const Handle_<StorableCalibrationDates_> dates(new StorableCalibrationDates_(spec.knotDates_));
        const Handle_<StorableCalibrationObservations_> rows(new StorableCalibrationObservations_(spec.observations_));
        return XccyFields({{"context", Cell_(StoreXccy(context))},
                           {"domestic_curve_block", Cell_(StoreXccy(domestic))},
                           {"foreign_curve_block", Cell_(StoreXccy(foreign))},
                           {"fx_spot", Cell_(1.1)},
                           {"knot_dates", Cell_(StoreXccy(dates))},
                           {"observations", Cell_(StoreXccy(rows))}});
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

TEST(ExcelXccyInstrumentTest, TestImportErrorsFollowActualFieldCells) {
    auto fields = XccyFields({{"min_abs_annuity", Cell_(true)}, {"instrument_id", Cell_("xccy")}, {"discount_route", Cell_("USD.OIS")}});
    XccyFactoryError(XccyScriptImportSettings_New, fields, "XccyScriptImportSettings_New", "min_abs_annuity", 1, 2);
    SwapXccyRows(&fields, 0, 2);
    XccyFactoryError(XccyScriptImportSettings_New, fields, "XccyScriptImportSettings_New", "min_abs_annuity", 3, 2);
    fields(2, 1) = 1e-12;
    const auto valid = fields;
    fields(1, 1) = 1.;
    XccyFactoryError(XccyScriptImportSettings_New, fields, "XccyScriptImportSettings_New", "instrument_id", 2, 2);
    fields = valid;
    fields(1, 0) = "typo";
    XccyFactoryError(XccyScriptImportSettings_New, fields, "XccyScriptImportSettings_New", "typo", 2, 1);
    XccyFactoryError(XccyScriptImportSettings_New, ExtraXccyField(valid, "unknown", Cell_(1.)), "XccyScriptImportSettings_New", "unknown", 4, 1);
    XccyFactoryError(XccyScriptImportSettings_New, ExtraXccyField(valid, "INSTRUMENT_ID", Cell_("again")), "XccyScriptImportSettings_New",
                     "INSTRUMENT_ID", 4, 1);
}

TEST(ExcelXccyInstrumentTest, TestContextErrorsFollowActualFieldCells) {
    InitGlobalData();
    Excel::ScriptTestInitialize(1);
    auto fields = XccyContextFields();
    const auto valid = fields;
    const Handle_<StorableXccyScriptImportSettings_> wrong(new StorableXccyScriptImportSettings_({}));
    fields(1, 1) = StoreXccy(wrong);
    XccyFactoryError(XccyScriptContext_New, fields, "XccyScriptContext_New", "pair", 2, 2);
    SwapXccyRows(&fields, 1, 3);
    XccyFactoryError(XccyScriptContext_New, fields, "XccyScriptContext_New", "pair", 4, 2);
    fields = valid;
    fields(0, 1) = true;
    XccyFactoryError(XccyScriptContext_New, fields, "XccyScriptContext_New", "valuation_time", 1, 2);
    fields = valid;
    fields(1, 0) = "typo";
    XccyFactoryError(XccyScriptContext_New, fields, "XccyScriptContext_New", "typo", 2, 1);
    XccyFactoryError(XccyScriptContext_New, ExtraXccyField(valid, "unknown", Cell_(1.)), "XccyScriptContext_New", "unknown", 5, 1);
    XccyFactoryError(XccyScriptContext_New, ExtraXccyField(valid, "PAIR", valid(1, 1)), "XccyScriptContext_New", "PAIR", 5, 1);
}

TEST(ExcelXccyInstrumentTest, TestSpecErrorsFollowActualFieldCells) {
    InitGlobalData();
    Excel::ScriptTestInitialize(1);
    auto fields = XccySpecFields();
    const auto valid = fields;
    fields(1, 1) = fields(4, 1);
    XccyFactoryError(XccyInstrumentCalibrationSpec_New, fields, "XccyInstrumentCalibrationSpec_New", "domestic_curve_block", 2, 2);
    SwapXccyRows(&fields, 1, 4);
    XccyFactoryError(XccyInstrumentCalibrationSpec_New, fields, "XccyInstrumentCalibrationSpec_New", "domestic_curve_block", 5, 2);
    fields = valid;
    fields(3, 1) = true;
    XccyFactoryError(XccyInstrumentCalibrationSpec_New, fields, "XccyInstrumentCalibrationSpec_New", "fx_spot", 4, 2);
    SwapXccyRows(&fields, 0, 3);
    XccyFactoryError(XccyInstrumentCalibrationSpec_New, fields, "XccyInstrumentCalibrationSpec_New", "fx_spot", 1, 2);
    fields = valid;
    fields(1, 0) = "typo";
    XccyFactoryError(XccyInstrumentCalibrationSpec_New, fields, "XccyInstrumentCalibrationSpec_New", "typo", 2, 1);
    XccyFactoryError(XccyInstrumentCalibrationSpec_New, ExtraXccyField(valid, "unknown", Cell_(1.)), "XccyInstrumentCalibrationSpec_New", "unknown",
                     7, 1);
    XccyFactoryError(XccyInstrumentCalibrationSpec_New, ExtraXccyField(valid, "CONTEXT", valid(0, 1)), "XccyInstrumentCalibrationSpec_New", "CONTEXT",
                     7, 1);
}

TEST(ExcelXccyInstrumentTest, TestDirectScriptCalibrationWithWorksheetCurveBlocks) {
    InitGlobalData();
    Excel::ScriptTestInitialize(1);
    const auto block = [](const String_& ccy, double rate, const String_& tenor) {
        Handle_<StorableDiscountCurve_> curve;
        DiscountPWC_New(ccy, ccy, {TestXccy::TODAY, TestXccy::END}, {rate, rate}, {}, &curve);
        Matrix_<Cell_> discountBindings(2, 2), forwardBindings(2, 2);
        for (int col = 0; col < 2; ++col) {
            discountBindings(0, col) = forwardBindings(0, col) = col == 0 ? Cell_("key") : Cell_("curve");
            discountBindings(1, col) = forwardBindings(1, col) = col == 0 ? Cell_("OIS") : Cell_(StoreXccy(curve));
        }
        forwardBindings(1, 0) = tenor;
        Handle_<StorableCalibrationDiscountCurves_> discounts;
        Handle_<StorableCalibrationForwardCurves_> forwards;
        CalibrationDiscountCurves_New(discountBindings, &discounts);
        CalibrationForwardCurves_New(forwardBindings, &forwards);
        Handle_<StorableCurveBlock_> result;
        CurveBlock_New(ccy, ccy, discounts, forwards, "ACT_365F", &result);
        return result;
    };
    const auto usd = block("USD", .04, "QUARTERLY"), eur = block("EUR", .02, "SEMIANNUAL");
    const auto nativeUsd = TestXccy::Flat("USD", .04), nativeEur = TestXccy::Flat("EUR", .02);
    ASSERT_NEAR(usd->val_->Forward(PeriodLength_("3M"), CollateralType_("OIS"))(TestXccy::TODAY, TestXccy::END),
                (*nativeUsd)(TestXccy::TODAY, TestXccy::END), 1e-14);
    ASSERT_NEAR(eur->val_->Forward(PeriodLength_("6M"), CollateralType_("OIS"))(TestXccy::TODAY, TestXccy::END),
                (*nativeEur)(TestXccy::TODAY, TestXccy::END), 1e-14);
    auto fields = XccySpecFields();
    fields(1, 1) = StoreXccy(usd);
    fields(2, 1) = StoreXccy(eur);
    Handle_<StorableXccyInstrumentCalibrationSpec_> spec;
    XccyInstrumentCalibrationSpec_New(fields, &spec);
    Handle_<StorableCalibrationOutcome_> outcome;
    Calibrate_XccyInstruments(spec, {}, &outcome);
    ASSERT_EQ(outcome->status_, "Succeeded");
    const auto expected = TestXccy::Flat("USD", .003);
    ASSERT_NEAR((*outcome->curve_)(TestXccy::TODAY, TestXccy::END), (*expected)(TestXccy::TODAY, TestXccy::END), 1e-12);
}

#include <gtest/gtest.h>

#include <dal-excel/src/__calibrationinstrument.hpp>
#include <dal-excel/src/__script_test_api.hpp>

TEST(ExcelCalibrationInstrumentTest, TestStrictImportSettingsRejectDuplicateUnknownAndBooleanNotional) {
    const auto valid = [] {
        Dal::Matrix_<Dal::Cell_> cells(5, 2);
        const char* keys[] = {"instrument_id", "currency", "notional", "discount_route", "min_abs_annuity"};
        for (int i = 0; i < 5; ++i)
            cells(i, 0) = keys[i];
        cells(0, 1) = "deposit";
        cells(1, 1) = "USD";
        cells(2, 1) = 100.;
        cells(3, 1) = "USD.OIS";
        cells(4, 1) = 1e-12;
        return cells;
    };
    Dal::Handle_<Dal::StorableScriptInstrumentImportSettings_> settings;
    auto cells = valid();
    Dal::ScriptInstrumentImportSettings_New(cells, &settings);
    ASSERT_TRUE(settings);
    cells(2, 1) = true;
    ASSERT_THROW(Dal::ScriptInstrumentImportSettings_New(cells, &settings), Dal::Exception_);
    cells = valid();
    cells(1, 0) = "instrument_id";
    ASSERT_THROW(Dal::ScriptInstrumentImportSettings_New(cells, &settings), Dal::Exception_);
    cells = valid();
    cells(1, 0) = "Currency";
    ASSERT_THROW(Dal::ScriptInstrumentImportSettings_New(cells, &settings), Dal::Exception_);
}

namespace {
    template <class T_> Dal::String_ Store(const Dal::Handle_<T_>& object) {
        return Dal::Excel::ScriptTestStoreRepository(Dal::handle_cast<Dal::Storable_>(object));
    }
    Dal::Matrix_<Dal::Cell_> Fields(std::initializer_list<std::pair<const char*, Dal::Cell_>> fields) {
        Dal::Matrix_<Dal::Cell_> result(static_cast<int>(fields.size()), 2);
        int row = 0;
        for (const auto& field : fields) {
            result(row, 0) = field.first;
            result(row++, 1) = field.second;
        }
        return result;
    }
} // namespace

TEST(ExcelCalibrationInstrumentTest, TestTypedSheetConstructionSharesAxesAndUnavailableMatrixIsError) {
    using namespace Dal;
    const Date_ today(2026, 10, 21), end(2027, 10, 21);
    Handle_<StorableScriptInstrumentImportSettings_> settings;
    ScriptInstrumentImportSettings_New(Fields({{"instrument_id", Cell_("deposit")},
                                               {"currency", Cell_("USD")},
                                               {"notional", Cell_(100.)},
                                               {"discount_route", Cell_("USD.OIS")},
                                               {"min_abs_annuity", Cell_(1e-12)}}),
                                       &settings);
    const Handle_<StorableYCInstrument_> native(
        new StorableYCInstrument_(Handle_<YCInstrument_>(new Deposit_(today, end, .025, DayBasis_("ACT_360")))));
    Handle_<CalibrationInstrument_> instrument;
    DepositScriptInstrument_New(native, settings, &instrument);
    Handle_<StorableCalibrationMarketQuote_> quote;
    CalibrationMarketQuote_New("q", Cell_(.035), "Rate", {}, &quote);
    Handle_<StorableCalibrationObservation_> observation;
    CalibrationObservation_New("o", instrument, "ParRate", quote, Cell_(1e-10), &observation);
    Handle_<StorableCalibrationObservations_> observations;
    CalibrationObservations_New({Handle_<Storable_>(observation)}, &observations);
    ASSERT_THROW(CalibrationObservations_New({Handle_<Storable_>(quote)}, &observations), Exception_);
    Handle_<StorableCalibrationDates_> dates;
    CalibrationDates_New({Cell_(today), Cell_(end)}, &dates);
    Handle_<StorableCurveInstrumentGeometry_> geometry;
    CurveInstrumentGeometry_New(
        Fields(
            {{"today", Cell_(today)}, {"currency", Cell_("USD")}, {"knot_dates", Cell_(Store(dates))}, {"parameterization", Cell_("LOG_DISCOUNT")}}),
        &geometry);
    Matrix_<Cell_> routes(2, 4);
    routes(0, 0) = "id";
    routes(0, 1) = "projection";
    routes(0, 2) = "collateral";
    routes(0, 3) = "tenor";
    routes(1, 0) = "USD.OIS";
    routes(1, 1) = false;
    routes(1, 2) = "OIS";
    Handle_<StorableCurveScriptRoutes_> routeHandle;
    CurveScriptRoutes_New(routes, &routeHandle);
    Handle_<StorableCurveInstrumentMarket_> market;
    CurveInstrumentMarket_New(Fields({{"routes", Cell_(Store(routeHandle))}}), &market);
    ScriptValuationSettings_ valuation;
    valuation.evaluationDate_ = today;
    valuation.fixings_.reset(new MarketFixingSnapshot_());
    const Handle_<StorableScriptValuationSettings_> valuationHandle(new StorableScriptValuationSettings_("", valuation));
    auto specRows = Fields({{"geometry", Cell_(Store(geometry))},
                            {"market", Cell_(Store(market))},
                            {"valuation", Cell_(Store(valuationHandle))},
                            {"observations", Cell_(Store(observations))}});
    Handle_<StorableCurveInstrumentCalibrationSpec_> spec;
    CurveInstrumentCalibrationSpec_New(specRows, &spec);
    specRows(0, 1) = Store(quote);
    ASSERT_THROW(CurveInstrumentCalibrationSpec_New(specRows, &spec), Exception_);
    Handle_<StorableCalibrationOutcome_> outcome;
    Calibrate_CurveInstruments(spec, {}, &outcome);
    ASSERT_EQ(outcome->status_, "Succeeded");
    Handle_<CalibrationReport_> report;
    CalibrationOutcome_Get_Report(outcome, &report);
    Matrix_<Cell_> rows;
    CalibrationReport_Get_Observations(report, &rows);
    ASSERT_EQ(rows.Rows(), 2);
    ASSERT_EQ(rows.Cols(), 22);
    ASSERT_EQ(Cell::ToString(rows(0, 19)), "backend");
    ASSERT_EQ(Cell::ToString(rows(1, 19)), "FastDeterministic");
    ASSERT_NEAR(Cell::ToDouble(rows(1, 12)), .035, 1e-10);
    Matrix_<> jacobian;
    ASSERT_THROW(CalibrationReport_Get_QuoteJacobian(report, &jacobian), CalibrationError_);
    CalibrationReport_Get_QuoteMap(report, &jacobian);
    ASSERT_DOUBLE_EQ(jacobian(0, 0), 1.);
    auto failed = spec->val_;
    failed.solver_.maxEvaluations_ = 1;
    Calibrate_CurveInstruments(Handle_<StorableCurveInstrumentCalibrationSpec_>(new StorableCurveInstrumentCalibrationSpec_(failed)), {}, &outcome);
    ASSERT_EQ(outcome->status_, "Failed");
    CalibrationReport_Get_Errors(outcome->report_, &rows);
    ASSERT_EQ(rows.Rows(), 2);
    ASSERT_EQ(Cell::ToString(rows(1, 0)), "CalibrationBudgetExceeded");
    double mean = 0.;
    ASSERT_THROW(CalibrationOutcome_Get_Mean(outcome, &mean), Exception_);
}

TEST(ExcelCalibrationInstrumentTest, TestPartialReportHeadersAndAsciiChunks) {
    using namespace Dal;
    CalibrationDiagnostic_ diagnostic;
    diagnostic.code_ = "InvalidCalibrationPlan";
    diagnostic.message_ = String_(std::string(31000, 'x')) + "中文";
    Handle_<CalibrationReport_> report(new CalibrationReport_(CalibrationStage_("Input"), {}, {diagnostic}));
    Matrix_<Cell_> rows;
    CalibrationReport_Get_Observations(report, &rows);
    ASSERT_EQ(rows.Rows(), 1);
    ASSERT_EQ(rows.Cols(), 22);
    Vector_<String_> chunks;
    CalibrationReport_Get_Json(report, &chunks);
    ASSERT_GT(chunks.size(), 1u);
    for (const auto& chunk : chunks) {
        ASSERT_LE(chunk.size(), 30000u);
        for (unsigned char c : chunk)
            ASSERT_LT(c, 128);
    }
    Matrix_<> matrix;
    ASSERT_THROW(CalibrationReport_Get_QuoteMap(report, &matrix), Exception_);
    ASSERT_THROW(CalibrationMarketQuote_New("q", Cell_(true), "Rate", {}, nullptr), Exception_);
}

namespace {
    Dal::Matrix_<Dal::Cell_> DefinitionSource(const Dal::ScriptInstrumentDefinitionSpec_& spec) {
        using namespace Dal;
        Matrix_<Cell_> source(static_cast<int>(spec.source_.rows_.size()) + 1, 3);
        source(0, 0) = "kind";
        source(0, 1) = "key";
        source(0, 2) = "text";
        for (size_t i = 0; i < spec.source_.rows_.size(); ++i) {
            source(static_cast<int>(i) + 1, 0) = spec.source_.rows_[i].kind_.String();
            source(static_cast<int>(i) + 1, 1) = spec.source_.rows_[i].key_;
            source(static_cast<int>(i) + 1, 2) = spec.source_.rows_[i].text_;
        }
        return source;
    }
    Dal::Matrix_<Dal::Cell_> ParameterTable(const Dal::ScriptInstrumentDefinitionSpec_& spec) {
        using namespace Dal;
        Matrix_<Cell_> parameters(4, 9);
        const char* columns[] = {"key", "type", "unit", "role", "bindable", "lower", "upper", "lower_inclusive", "upper_inclusive"};
        for (int c = 0; c < 9; ++c)
            parameters(0, c) = columns[c];
        for (int i = 0; i < 3; ++i) {
            const auto& p = spec.parameters_[i];
            parameters(i + 1, 0) = p.key_;
            parameters(i + 1, 1) = p.type_.String();
            parameters(i + 1, 2) = p.unit_.Token();
            parameters(i + 1, 3) = p.role_.String();
            parameters(i + 1, 4) = p.bindable_;
            if (p.lower_)
                parameters(i + 1, 5) = *p.lower_;
            if (p.upper_)
                parameters(i + 1, 6) = *p.upper_;
            parameters(i + 1, 7) = p.lowerInclusive_;
            parameters(i + 1, 8) = p.upperInclusive_;
        }
        return parameters;
    }
    Dal::Matrix_<Dal::Cell_> MeasureTable(const Dal::ScriptInstrumentDefinitionSpec_& spec) {
        using namespace Dal;
        Matrix_<Cell_> measures(3, 7);
        const char* measureColumns[] = {"id", "kind", "receiver", "parameter", "unit", "annuity_sign", "min_abs_annuity"};
        for (int c = 0; c < 7; ++c)
            measures(0, c) = measureColumns[c];
        for (int i = 0; i < 2; ++i) {
            const auto& m = spec.measures_[i];
            measures(i + 1, 0) = m.id_;
            measures(i + 1, 1) = m.kind_.String();
            measures(i + 1, 2) = m.receiver_;
            measures(i + 1, 4) = m.unit_.Token();
            if (m.parameter_)
                measures(i + 1, 3) = *m.parameter_;
            if (m.annuitySign_)
                measures(i + 1, 5) = double(*m.annuitySign_);
            if (m.minAbsAnnuity_)
                measures(i + 1, 6) = *m.minAbsAnnuity_;
        }
        return measures;
    }
} // namespace

TEST(ExcelCalibrationInstrumentTest, TestDefinitionAndBindingTablesRoundTripAndRejectBadCells) {
    using namespace Dal;
    const Date_ today(2026, 10, 21), end(2027, 10, 21);
    const auto imported = NewDepositScriptInstrument(Deposit_(today, end, .025, DayBasis_("ACT_360")), {"reference", Ccy_("USD"), "USD.OIS", 100.});
    const auto& spec = imported->Definition()->Spec();
    const auto source = DefinitionSource(spec);
    const auto header = Fields({{"pay_currency", Cell_("USD")},
                                {"result_currency", Cell_("USD")},
                                {"collateral_currency", Cell_("USD")},
                                {"discount_route", Cell_("USD.OIS")}});
    auto parameters = ParameterTable(spec);
    const auto measures = MeasureTable(spec);
    Handle_<ScriptInstrumentDefinition_> definition;
    ScriptInstrumentDefinition_New(source, header, parameters, measures, {}, &definition);
    ASSERT_EQ(definition->DefinitionIdentity(), imported->Definition()->DefinitionIdentity());
    Matrix_<Cell_> bindings(2, 5);
    const char* bindingColumns[] = {"key", "value", "unit", "quote_id", "quote_fingerprint"};
    for (int c = 0; c < 5; ++c)
        bindings(0, c) = bindingColumns[c];
    bindings(1, 0) = "K";
    bindings(1, 1) = .03;
    bindings(1, 2) = "Rate";
    Handle_<CalibrationInstrument_> instrument;
    ScriptCalibrationInstrument_New("copy", definition, bindings, Cell_(end), &instrument);
    ASSERT_DOUBLE_EQ(instrument->Binding()[1].value_, .03);
    bindings(1, 1) = true;
    ASSERT_THROW(CalibrationInstrument_BindParameters(instrument, bindings, &instrument), Exception_);
    ASSERT_DOUBLE_EQ(instrument->Binding()[1].value_, .03);
    parameters(0, 1) = "key";
    ASSERT_THROW(ScriptInstrumentDefinition_New(source, header, parameters, measures, {}, &definition), Exception_);
}

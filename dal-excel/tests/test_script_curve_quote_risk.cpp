#include <gtest/gtest.h>

#include <dal-excel/src/__excel_test_api.hpp>
#include <dal-excel/src/__script_test_api.hpp>
#include <dal-excel/src/__scriptcurvequoterisk.hpp>
#include <dal-public/tests/scriptquoteriskfixture.hpp>

using namespace Dal;

namespace {
    Matrix_<Cell_> Fields(std::initializer_list<std::pair<const char*, Cell_>> fields) {
        Matrix_<Cell_> result(static_cast<int>(fields.size()), 2);
        int row = 0;
        for (const auto& field : fields) {
            result(row, 0) = field.first;
            result(row++, 1) = field.second;
        }
        return result;
    }
    Handle_<StorableScriptCurveQuoteRiskSnapshot_> Snapshot(const CurveInstrumentCalibrationSpec_& spec) {
        const Handle_<StorableCurveInstrumentCalibrationSpec_> context(new StorableCurveInstrumentCalibrationSpec_(spec));
        Handle_<StorableCalibrationOutcome_> fit;
        Calibrate_CurveInstruments(context, {}, &fit);
        const Handle_<StorableScriptCurveQuoteRiskAccuracy_> accuracy(new StorableScriptCurveQuoteRiskAccuracy_({{1e-12, 1e-12}, 1e-10, 1e-12}));
        Handle_<StorableScriptCurveQuoteRiskSnapshot_> snapshot;
        ScriptCurveQuoteRiskSnapshot_New(fit, accuracy, &snapshot);
        return snapshot;
    }
} // namespace

TEST(ScriptCurveQuoteRiskExcelTest, TestOutcomeRetainsFitAndProjectsOrderedRisk) {
    const auto spec = Dal::Test::ScriptQuoteRiskSpec();
    const Handle_<StorableCurveInstrumentCalibrationSpec_> context(new StorableCurveInstrumentCalibrationSpec_(spec));
    Handle_<StorableCalibrationOutcome_> fit;
    Calibrate_CurveInstruments(context, {}, &fit);
    ASSERT_EQ(fit->status_, "Succeeded");
    ASSERT_TRUE(fit->curveFit_);
    const Handle_<StorableScriptCurveQuoteRiskAccuracy_> accuracy(new StorableScriptCurveQuoteRiskAccuracy_({{1e-12, 1e-12}, 1e-10, 1e-12}));
    Handle_<StorableScriptCurveQuoteRiskSnapshot_> snapshot;
    ScriptCurveQuoteRiskSnapshot_New(fit, accuracy, &snapshot);
    fit.reset();
    const Handle_<StorableScriptCurveQuoteRiskRequest_> request(new StorableScriptCurveQuoteRiskRequest_({{"last", "first"}}));
    Handle_<StorableScriptCurveQuoteRiskResult_> risk;
    CalibrationInstrument_ValueWithQuoteRisk(spec.observations_[0]->Instrument(), snapshot, context, request, &risk);
    snapshot.reset();
    String_ status;
    ScriptCurveQuoteRiskResult_Get_Status(risk, &status);
    ASSERT_EQ(status, "Available");
    Matrix_<Cell_> rows;
    ScriptCurveQuoteRiskResult_Get_QuoteAxis(risk, &rows);
    ASSERT_EQ(rows.Rows(), 3);
    ASSERT_EQ(Cell::ToString(rows(1, 1)), "last");
    ASSERT_EQ(Cell::ToDouble(rows(1, 0)), 1.);
    ASSERT_EQ(Cell::ToString(rows(2, 3)), "Rate");
    double pv;
    ScriptCurveQuoteRiskResult_Get_PV(risk, &pv);
    const double accrual = 365. / 360., d = 1. + .035 * accrual;
    ASSERT_NEAR(pv, -100. + 100. * (1. + .025 * accrual) / d, 1e-10);
    Vector_<> raw, dv01;
    ScriptCurveQuoteRiskResult_Get_RawSensitivity(risk, &raw);
    ScriptCurveQuoteRiskResult_Get_DV01(risk, &dv01);
    ASSERT_NEAR(raw[1], -100. * (1. + .025 * accrual) * accrual / (d * d), 1e-8);
    ASSERT_DOUBLE_EQ(dv01[1], raw[1] * 1e-4);
}

TEST(ScriptCurveQuoteRiskExcelTest, TestUnavailableMetricsStayBlankAndInputErrorsThrow) {
    const auto spec = Dal::Test::ScriptQuoteRiskSpec();
    const auto snapshot = Snapshot(spec);
    auto stale = spec;
    stale.solver_.maxEvaluations_ += 1;
    Handle_<StorableScriptCurveQuoteRiskResult_> result;
    CalibrationInstrument_ValueWithQuoteRisk(spec.observations_[0]->Instrument(), snapshot,
                                             Handle_<StorableCurveInstrumentCalibrationSpec_>(new StorableCurveInstrumentCalibrationSpec_(stale)), {},
                                             &result);
    String_ status;
    ScriptCurveQuoteRiskResult_Get_Status(result, &status);
    ASSERT_EQ(status, "Unavailable");
    Matrix_<Cell_> rows;
    ScriptCurveQuoteRiskResult_Get_Summary(result, &rows);
    ASSERT_TRUE(Cell::IsEmpty(rows(3, 1)));
    ScriptCurveQuoteRiskResult_Get_Sensitivity(result, &rows);
    ASSERT_TRUE(Cell::IsEmpty(rows(1, 4)));
    ASSERT_TRUE(Cell::IsEmpty(rows(1, 5)));
    ScriptCurveQuoteRiskResult_Get_Diagnostics(result, &rows);
    ASSERT_EQ(Cell::ToString(rows(1, 0)), "ScriptCurveQuoteRiskIdentityMismatch");
    double pv;
    ASSERT_THROW(ScriptCurveQuoteRiskResult_Get_PV(result, &pv), Exception_);
    stale.valuation_.fixings_.reset();
    try {
        CalibrationInstrument_ValueWithQuoteRisk(spec.observations_[0]->Instrument(), snapshot,
                                                 Handle_<StorableCurveInstrumentCalibrationSpec_>(new StorableCurveInstrumentCalibrationSpec_(stale)),
                                                 {}, &result);
        FAIL() << "malformed context must remain a worksheet input error";
    } catch (const CalibrationError_& error) {
        ASSERT_EQ(error.Failure().diagnostic_.code_, "InvalidScriptCurveQuoteRiskInput");
    }
    ASSERT_EQ(result->val_.Diagnostics()[0].code_, "ScriptCurveQuoteRiskIdentityMismatch");
}

TEST(ScriptCurveQuoteRiskExcelTest, TestStrictTypedFieldTablesAndLocations) {
    Handle_<StorableCalibrationNumbers_> limits;
    CalibrationNumbers_New({Cell_(1e-12), Cell_(1e-12)}, &limits);
    const auto limitsTag = Excel::ScriptTestStoreRepository(Handle_<Storable_>(limits));
    auto settings = Fields({{"residual_absolute_limits", Cell_(limitsTag)},
                            {"min_reciprocal_condition", Cell_(1e-10)},
                            {"response_backward_error_limit", Cell_(1e-12)}});
    Handle_<StorableScriptCurveQuoteRiskAccuracy_> accuracy;
    ScriptCurveQuoteRiskAccuracy_New(settings, &accuracy);
    ASSERT_EQ(accuracy->val_.residualAbsoluteLimits_.size(), 2u);
    settings(1, 1) = true;
    try {
        ScriptCurveQuoteRiskAccuracy_New(settings, &accuracy);
        FAIL() << "boolean is not a condition number";
    } catch (const Exception_& error) {
        const std::string message = error.what();
        ASSERT_NE(message.find("ScriptCurveQuoteRiskAccuracy_New"), std::string::npos);
        ASSERT_NE(message.find("row=2 column=2"), std::string::npos);
        ASSERT_NE(message.find("min_reciprocal_condition"), std::string::npos);
    }
    settings(1, 0) = "residual_absolute_limits";
    ASSERT_THROW(ScriptCurveQuoteRiskAccuracy_New(settings, &accuracy), Exception_);
    Handle_<StorableScriptCurveQuoteRiskRequest_> request;
    ASSERT_THROW(ScriptCurveQuoteRiskRequest_New(Fields({{"Unknown", Cell_(true)}}), &request), Exception_);
    ASSERT_THROW(ScriptCurveQuoteRiskRequest_New(Fields({{"quote_ids", Cell_(limitsTag)}}), &request), Exception_);
    Handle_<StorableScriptCurveQuoteIds_> ids;
    ScriptCurveQuoteIds_New({Cell_("last"), Cell_("first")}, &ids);
    const auto idTag = Excel::ScriptTestStoreRepository(Handle_<Storable_>(ids));
    ScriptCurveQuoteRiskRequest_New(Fields({{"quote_ids", Cell_(idTag)}}), &request);
    ASSERT_EQ(request->val_.quoteIds_[0], "last");
    ASSERT_THROW(ScriptCurveQuoteIds_New({Cell_(true)}, &ids), Exception_);
}

TEST(ScriptCurveQuoteRiskExcelTest, TestReportOnlyOutcomeCannotInventFitContext) {
    const auto fit = CalibrateCurveInstruments(Dal::Test::ScriptQuoteRiskSpec());
    const Handle_<StorableCalibrationOutcome_> legacy(new StorableCalibrationOutcome_("Succeeded", fit.report_, fit.curve_));
    Handle_<CalibrationReport_> report;
    CalibrationOutcome_Get_Report(legacy, &report);
    ASSERT_EQ(report, fit.report_);
    const Handle_<StorableScriptCurveQuoteRiskAccuracy_> accuracy(new StorableScriptCurveQuoteRiskAccuracy_({{1e-12, 1e-12}, 1e-10, 1e-12}));
    Handle_<StorableScriptCurveQuoteRiskSnapshot_> snapshot;
    ASSERT_THROW(ScriptCurveQuoteRiskSnapshot_New(legacy, accuracy, &snapshot), Exception_);
    ASSERT_FALSE(report->QuoteRiskAvailable());
}

TEST(ScriptCurveQuoteRiskExcelTest, TestBasisSpreadAxisAndNumericalRejection) {
    Excel::ScriptTestInitialize(1);
    const auto spec = Dal::Test::ScriptBasisQuoteRiskSpec();
    const Handle_<StorableCurveInstrumentCalibrationSpec_> context(new StorableCurveInstrumentCalibrationSpec_(spec));
    Handle_<StorableCalibrationOutcome_> fit;
    Calibrate_CurveInstruments(context, {}, &fit);
    const Handle_<StorableScriptCurveQuoteRiskAccuracy_> accuracy(new StorableScriptCurveQuoteRiskAccuracy_({{1e-12}, 1e-10, 1e-12}));
    Handle_<StorableScriptCurveQuoteRiskSnapshot_> snapshot;
    ScriptCurveQuoteRiskSnapshot_New(fit, accuracy, &snapshot);
    Handle_<StorableScriptCurveQuoteRiskResult_> result;
    CalibrationInstrument_ValueWithQuoteRisk(spec.observations_[0]->Instrument(), snapshot, context, {}, &result);
    ASSERT_TRUE(result->val_.Available());
    Matrix_<Cell_> rows;
    ScriptCurveQuoteRiskResult_Get_QuoteAxis(result, &rows);
    ASSERT_EQ(Cell::ToString(rows(1, 3)), "BasisSpread");
    ASSERT_EQ(Cell::ToString(rows(1, 4)), "DecimalSpread");
    ASSERT_DOUBLE_EQ(result->val_.DV01()[0], result->val_.RawSensitivity()[0] * 1e-4);
    auto loose = spec;
    loose.observations_[0] = NewCalibrationObservation("basis-o", spec.observations_[0]->Instrument(), "BasisSpread",
                                                       NewCalibrationMarketQuote("basis-q", .05, CalibrationValueUnit_::Parse("BasisSpread")), 1.);
    const Handle_<StorableCurveInstrumentCalibrationSpec_> looseContext(new StorableCurveInstrumentCalibrationSpec_(loose));
    Calibrate_CurveInstruments(looseContext, {}, &fit);
    ScriptCurveQuoteRiskSnapshot_New(fit, accuracy, &snapshot);
    ASSERT_TRUE(snapshot->val_.StructureValid());
    ASSERT_FALSE(snapshot->val_.RiskAvailable());
    CalibrationInstrument_ValueWithQuoteRisk(loose.observations_[0]->Instrument(), snapshot, looseContext, {}, &result);
    ScriptCurveQuoteRiskResult_Get_Sensitivity(result, &rows);
    ASSERT_TRUE(Cell::IsEmpty(rows(1, 4)));
    ASSERT_TRUE(Cell::IsEmpty(rows(1, 5)));
}

#ifdef _WIN32
TEST(ScriptCurveQuoteRiskExcelTest, TestGeneratedXllRegistration) {
    const auto registrations = RegisteredFunctionsForTest();
    const std::pair<const char*, const char*> contracts[] = {{"ScriptCurveQuoteIds_New", "ids"},
                                                             {"ScriptCurveQuoteRiskAccuracy_New", "settings"},
                                                             {"ScriptCurveQuoteRiskRequest_New", "settings"},
                                                             {"ScriptCurveQuoteRiskSnapshot_New", "calibration,accuracy"},
                                                             {"CalibrationInstrument_ValueWithQuoteRisk", "instrument,snapshot,context,[request]"},
                                                             {"ScriptCurveQuoteRiskSnapshot_Get_Status", "snapshot"},
                                                             {"ScriptCurveQuoteRiskSnapshot_Get_Summary", "snapshot"},
                                                             {"ScriptCurveQuoteRiskSnapshot_Get_QuoteAxis", "snapshot"},
                                                             {"ScriptCurveQuoteRiskSnapshot_Get_Diagnostics", "snapshot"},
                                                             {"ScriptCurveQuoteRiskResult_Get_Status", "result"},
                                                             {"ScriptCurveQuoteRiskResult_Get_Summary", "result"},
                                                             {"ScriptCurveQuoteRiskResult_Get_QuoteAxis", "result"},
                                                             {"ScriptCurveQuoteRiskResult_Get_Diagnostics", "result"},
                                                             {"ScriptCurveQuoteRiskResult_Get_Sensitivity", "result"},
                                                             {"ScriptCurveQuoteRiskResult_Get_PV", "result"},
                                                             {"ScriptCurveQuoteRiskResult_Get_RawSensitivity", "result"},
                                                             {"ScriptCurveQuoteRiskResult_Get_DV01", "result"}};
    for (const auto& contract : contracts) {
        const auto found =
            std::find_if(registrations.begin(), registrations.end(), [&](const auto& r) { return r.cName_ == String_("xl_") + contract.first; });
        ASSERT_NE(found, registrations.end()) << contract.first;
        ASSERT_EQ(std::string(found->argNames_.c_str()), contract.second);
        ASSERT_FALSE(found->volatile_);
        ASSERT_FALSE(found->help_.empty());
    }
}
#endif

//
// Created by Codex on 2026/9/15.
//

#include <gtest/gtest.h>

#include <cmath>
#include <limits>

#ifdef _WIN32
#define NOMINMAX
#include <dal-excel/src/__europeanpderisk.hpp>
#include <dal-excel/src/__script_test_api.hpp>
#include <dal-excel/src/_excel.hpp>
#include <dal-excel/src/_xlcall.hpp>

// Parse DAL's VOID enum before Windows.h defines its VOID macro.
#include <Windows.h>

using namespace Dal;

namespace {
    struct RawText_ {
        std::wstring text_;
        OPER_ cell_{};
        explicit RawText_(const std::wstring& text) : text_(1, static_cast<wchar_t>(text.size())) {
            text_ += text;
            cell_.xltype = xltypeStr;
            cell_.val.str = text_.data();
        }
    };
    OPER_ Number(double value) {
        OPER_ result{};
        result.xltype = xltypeNum;
        result.val.num = value;
        return result;
    }
    OPER_ Blank() {
        OPER_ result{};
        result.xltype = xltypeNil;
        return result;
    }
    OPER_ Multi(OPER_* data, int rows, int columns) {
        OPER_ result{};
        result.xltype = xltypeMulti;
        result.val.array.lparray = data;
        result.val.array.rows = rows;
        result.val.array.columns = columns;
        return result;
    }
    template <class... A_> OPER_* Call(const char* name, A_... args) {
        const auto address = GetProcAddress(GetModuleHandleA("dal_excel.xll"), name);
        REQUIRE(address, "missing generated export " + String_(name));
        return reinterpret_cast<OPER_* (*)(A_...)>(address)(args...);
    }
    struct Output_ {
        OPER_* value_;
        explicit Output_(OPER_* value) : value_(value) {}
        ~Output_() {
            const auto free = reinterpret_cast<void (*)(OPER_*)>(GetProcAddress(GetModuleHandleA("dal_excel.xll"), "xlAutoFree12"));
            free(value_);
        }
        const OPER_* Scalar() const {
            REQUIRE((value_->xltype & xltypeMulti) && value_->val.array.rows == 1 && value_->val.array.columns == 1, "expected scalar result");
            return value_->val.array.lparray;
        }
        std::string Text() const {
            const auto* scalar = Scalar();
            REQUIRE(scalar->xltype == xltypeStr, "expected text result");
            const auto* text = scalar->val.str;
            std::string result;
            for (int i = 0; i < text[0]; ++i)
                result += static_cast<char>(text[i + 1]);
            return result;
        }
    };

    void CheckError(const Output_& output, std::initializer_list<const char*> fields) {
        const auto text = output.Text();
        ASSERT_EQ(text.substr(0, 7), "#Error:");
        for (const auto* field : fields)
            ASSERT_NE(text.find(field), std::string::npos) << text;
    }

    struct CurvatureRuntime_ {
        std::pair<size_t, bool> workers_;
        Date_ date_;
        CurvatureRuntime_() {
            Excel::ScriptTestInitialize(1);
            workers_ = Excel::ScriptTestStartWorkers(1);
            date_ = Excel::ScriptTestSetDate(Date_(2026, 9, 12));
        }
        ~CurvatureRuntime_() {
            Excel::ScriptTestSetDate(date_);
            Excel::ScriptTestRestoreWorkers(workers_);
        }
    };

    OPER_* CalibrateRawRate(const OPER_* instrument, const OPER_* today, const OPER_* maturity, const OPER_* currency, const OPER_* name) {
        RawText_ nameKey(L"curveName"), kindKey(L"parameterization"), kind(L"LOG_DISCOUNT"), toleranceKey(L"tolerance");
        OPER_ settingCells[]{nameKey.cell_, *name, kindKey.cell_, kind.cell_, toleranceKey.cell_, Number(1.0e-14)};
        OPER_ dates[]{*today, *maturity};
        auto settings = Multi(settingCells, 3, 2), knots = Multi(dates, 2, 1), blank = Blank();
        return Call("xl_Calibrate_SingleCurve", today, currency, instrument, &knots, &settings, &blank, &blank);
    }

    struct RawRateFixture_ {
        CurvatureRuntime_ runtime_;
        RawText_ name_{L"raw-rate"}, tenor_{L"3M"}, basis_{L"ACT_365F"}, collateral_{L"OIS"}, currency_{L"USD"};
        OPER_ blank_ = Blank(), today_ = Number(Date::ToExcel(Date_(2026, 1, 15))), maturity_ = Number(Date::ToExcel(Date_(2027, 1, 15))),
              quote_ = Number(0.025), notional_ = Number(1.0), contract_ = Number(0.028);
        OPER_ lend_ = [] {
            OPER_ value{};
            value.xltype = xltypeBool;
            value.val.xbool = 1;
            return value;
        }();
        Output_ index_{Call("xl_RateIndexConvention_New", &tenor_.cell_, &basis_.cell_, &collateral_.cell_, &blank_)};
        Output_ instrument_{Call("xl_Deposit_New", &today_, &today_, &maturity_, &quote_, index_.Scalar())};
        Output_ calibration_{CalibrateRawRate(instrument_.Scalar(), &today_, &maturity_, &currency_.cell_, &name_.cell_)};
        Output_ snapshot_{Call("xl_RateCalibration_New", &name_.cell_, calibration_.Scalar())};
        Output_ header_{Call("xl_RateTradeHeader_New", &name_.cell_, &today_, &today_, &maturity_, &currency_.cell_)};
        Output_ trade_{Call("xl_RateDepositTrade_New", header_.Scalar(), &notional_, &contract_, &lend_, index_.Scalar(), &name_.cell_)};
    };

    void CheckRateFinancialSpills(const Output_& value, const Output_& gradient, const Output_& products) {
        const double payment = 1.028, discount = 1.025;
        ASSERT_NEAR(value.Scalar()->val.num, -2.0 * (payment / discount - 1.0), 1.0e-10);
        ASSERT_NEAR(gradient.Scalar()->val.num, 2.0 * payment / (discount * discount), 1.0e-10);
        ASSERT_EQ(products.value_->val.array.rows, 2);
        ASSERT_EQ(products.value_->val.array.columns, 1);
        const auto* cells = products.value_->val.array.lparray;
        ASSERT_NEAR(cells[0].val.num, -4.0 * payment / std::pow(discount, 3), 4.0e-7);
        ASSERT_NEAR(cells[1].val.num, 8.0 * payment / std::pow(discount, 3), 1.0e-6);
    }

    void CheckCurvatureFinancialSpills(const Output_& gradient, const Output_& products) {
        ASSERT_EQ(gradient.value_->val.array.rows, 6);
        ASSERT_EQ(gradient.value_->val.array.columns, 1);
        ASSERT_EQ(products.value_->val.array.rows, 3);
        ASSERT_EQ(products.value_->val.array.columns, 6);
        for (int quote = 0; quote < 6; ++quote) {
            ASSERT_NEAR(gradient.value_->val.array.lparray[quote].val.num, quote == 3 ? 0.002 * std::exp(-0.05) : 0.0, 1e-12);
            for (int row = 0; row < 3; ++row) {
                const double multiplier = row == 0 ? 1.0 : row == 1 ? -2.0 : 0.0;
                ASSERT_NEAR(products.value_->val.array.lparray[row * 6 + quote].val.num, quote == 3 ? multiplier * 2.0 * std::exp(-0.05) : 0.0,
                            1e-10);
            }
        }
    }
} // namespace

TEST(ScriptExcelRawTest, TestRateCurvatureAllGeneratedExportsAndSignedAnalyticSpills) {
    RawRateFixture_ fixture;
    const auto* name = &fixture.name_.cell_;
    const auto* snapshot = fixture.snapshot_.Scalar();
    auto blank = Blank();
    ASSERT_EQ(fixture.snapshot_.Text().find("#Error:"), std::string::npos) << fixture.snapshot_.Text();
    Output_ point(Call("xl_RateCalibration_Get_Point", snapshot));
    ASSERT_DOUBLE_EQ(point.Scalar()->val.num, 0.025);
    Output_ parameters(Call("xl_RateCalibration_Get_Parameters", snapshot));
    ASSERT_NEAR(parameters.Scalar()->val.num, -std::log(1.025), 1.0e-10);
    Output_ quotePlan(Call("xl_RateCalibration_Get_QuotePlan", snapshot));
    OPER_ complete{};
    complete.xltype = xltypeBool;
    complete.val.xbool = 1;
    Output_ axis(Call("xl_CalibrationRiskPlan_Get_Inputs", quotePlan.Scalar(), &complete));
    ASSERT_EQ(axis.value_->val.array.rows, 2);
    ASSERT_EQ(axis.value_->val.array.columns, 12);
    auto changed = Number(0.03);
    Output_ replay(Call("xl_RateCalibration_Recalibrate", name, snapshot, &changed));
    Output_ replayPoint(Call("xl_RateCalibration_Get_Point", replay.Scalar()));
    ASSERT_DOUBLE_EQ(replayPoint.Scalar()->val.num, 0.03);
    OPER_ weight{};
    weight.xltype = xltypeInt;
    weight.val.w = -2;
    Output_ settings(Call("xl_RateTradeQuoteCurvatureSettings_New", name, &weight, &blank));
    Output_ weights(Call("xl_RateTradeQuoteCurvatureSettings_Get_Weights", settings.Scalar()));
    ASSERT_DOUBLE_EQ(weights.Scalar()->val.num, -2.0);
    Output_ fixings(Call("xl_RateTradeQuoteCurvatureSettings_Get_Fixings", settings.Scalar()));
    ASSERT_EQ(fixings.value_->val.array.columns, 3);
    ASSERT_EQ(fixings.value_->val.array.lparray[1].xltype, xltypeBool);
    ASSERT_EQ(fixings.value_->val.array.lparray[1].val.xbool, 0);
    OPER_ directionCells[]{Number(1.0), weight}, stepCells[]{Number(0.0001), Number(0.0001)};
    auto directions = Multi(directionCells, 2, 1), steps = Multi(stepCells, 2, 1);
    Output_ bumps(Call("xl_BumpOverAADRequest_New", name, &directions, &steps, &blank));
    Output_ result(Call("xl_RateTradeQuoteCurvatureResult_New", name, fixture.trade_.Scalar(), snapshot, bumps.Scalar(), settings.Scalar()));
    ASSERT_EQ(result.Text().find("#Error:"), std::string::npos) << result.Text();
    Output_ value(Call("xl_RateTradeQuoteCurvatureResult_Get_Value", result.Scalar()));
    Output_ currency(Call("xl_RateTradeQuoteCurvatureResult_Get_Currency", result.Scalar()));
    ASSERT_EQ(currency.Text(), "USD");
    Output_ gradient(Call("xl_RateTradeQuoteCurvatureResult_Get_Gradient", result.Scalar()));
    Output_ products(Call("xl_RateTradeQuoteCurvatureResult_Get_HessianProducts", result.Scalar()));
    CheckRateFinancialSpills(value, gradient, products);
    for (const auto* function : {"xl_RateTradeQuoteCurvatureResult_Get_Point", "xl_RateTradeQuoteCurvatureResult_Get_Directions",
                                 "xl_RateTradeQuoteCurvatureResult_Get_Steps", "xl_RateTradeQuoteCurvatureResult_Get_Shape"}) {
        Output_ copied(Call(function, result.Scalar()));
        ASSERT_NE(copied.value_->val.array.lparray[0].xltype, xltypeStr);
        copied.value_->val.array.lparray[0].val.num = 77.0;
    }
    Output_ retained(Call("xl_RateTradeQuoteCurvatureResult_Get_Point", result.Scalar()));
    ASSERT_DOUBLE_EQ(retained.Scalar()->val.num, 0.025);
    Output_ execution(Call("xl_RateTradeQuoteCurvatureResult_Get_Execution", result.Scalar()));
    ASSERT_EQ(execution.value_->val.array.rows, 7);
    ASSERT_DOUBLE_EQ(execution.value_->val.array.lparray[3].val.num, 5.0);
    ASSERT_DOUBLE_EQ(execution.value_->val.array.lparray[9].val.num, 72.0);
    Output_ base(Call("xl_RateTradeQuoteCurvatureResult_Get_BaseCalibration", result.Scalar()));
    Output_ basePoint(Call("xl_RateCalibration_Get_Point", base.Scalar()));
    ASSERT_DOUBLE_EQ(basePoint.Scalar()->val.num, 0.025);
}

TEST(ScriptExcelRawTest, TestRateCurvatureTradeRangesRejectBlankGapsAndRetainDuplicates) {
    RawRateFixture_ fixture;
    const auto* name = &fixture.name_.cell_;
    auto blank = Blank(), one = Number(1.0), step = Number(0.0001);
    Output_ bumps(Call("xl_BumpOverAADRequest_New", name, &one, &step, &blank));
    RawText_ empty(L"");
    OPER_ missing{};
    missing.xltype = xltypeMissing;
    for (const auto& gap : {blank, missing, empty.cell_})
        for (int position = 0; position < 3; ++position)
            for (const bool column : {false, true}) {
                OPER_ cells[]{*fixture.trade_.Scalar(), *fixture.trade_.Scalar(), *fixture.trade_.Scalar()};
                cells[position] = gap;
                auto trades = Multi(cells, column ? 3 : 1, column ? 1 : 3);
                Output_ rejected(Call("xl_RateTradeQuoteCurvatureResult_New", name, &trades, fixture.snapshot_.Scalar(), bumps.Scalar(), &blank));
                const auto row = "trade_row=" + std::to_string(position + 1);
                CheckError(rejected, {"trades", row.c_str()});
            }
    OPER_ duplicates[]{*fixture.trade_.Scalar(), *fixture.trade_.Scalar()};
    auto trades = Multi(duplicates, 2, 1);
    Output_ result(Call("xl_RateTradeQuoteCurvatureResult_New", name, &trades, fixture.snapshot_.Scalar(), bumps.Scalar(), &blank));
    ASSERT_EQ(result.Text().find("#Error:"), std::string::npos) << result.Text();
    Output_ value(Call("xl_RateTradeQuoteCurvatureResult_Get_Value", result.Scalar()));
    ASSERT_NEAR(value.Scalar()->val.num, 2.0 * (1.028 / 1.025 - 1.0), 1.0e-10);
}

TEST(ScriptExcelRawTest, TestRateCurvatureRawKindsNulAndWrongHandlesReject) {
    RawRateFixture_ fixture;
    const auto* name = &fixture.name_.cell_;
    const auto* snapshot = fixture.snapshot_.Scalar();
    auto blank = Blank();
    RawText_ text(L"2"), nul(std::wstring(L"bad\0name", 8));
    OPER_ boolean{}, error{};
    boolean.xltype = xltypeBool;
    boolean.val.xbool = 1;
    error.xltype = xltypeErr;
    error.val.err = 15;
    OPER_ values[]{Number(1.0), Number(1.0)};
    auto vector = Multi(values, 1, 2);
    for (const auto& invalid :
         {blank, boolean, error, text.cell_, Number(std::numeric_limits<double>::infinity()), Number(std::numeric_limits<double>::quiet_NaN())}) {
        values[1] = invalid;
        Output_ rejected(Call("xl_RateTradeQuoteCurvatureSettings_New", name, &vector, &blank));
        CheckError(rejected, {"weights", "row=1", "column=2"});
        Output_ badQuotes(Call("xl_RateCalibration_Recalibrate", name, snapshot, &vector));
        CheckError(badQuotes, {"quotes", "row=1", "column=2"});
    }
    Output_ badName(Call("xl_RateCalibration_New", &nul.cell_, fixture.calibration_.Scalar()));
    CheckError(badName, {"name", "NUL"});
    Output_ badSettingsName(Call("xl_RateTradeQuoteCurvatureSettings_New", &nul.cell_, &blank, &blank));
    CheckError(badSettingsName, {"name", "NUL"});
    Output_ wrongSource(Call("xl_RateCalibration_New", name, snapshot));
    CheckError(wrongSource, {"source"});
    Output_ wrongSnapshot(Call("xl_RateCalibration_Get_Point", fixture.trade_.Scalar()));
    CheckError(wrongSnapshot, {"calibration"});
    Output_ wrongResult(Call("xl_RateTradeQuoteCurvatureResult_Get_Gradient", snapshot));
    CheckError(wrongResult, {"result"});
    auto one = Number(1.0), step = Number(0.0001);
    Output_ bumps(Call("xl_BumpOverAADRequest_New", name, &one, &step, &blank));
    Output_ wrongTrade(Call("xl_RateTradeQuoteCurvatureResult_New", name, snapshot, snapshot, bumps.Scalar(), &blank));
    CheckError(wrongTrade, {"trades", "row=1"});
    Output_ missing(Call("xl_RateTradeQuoteCurvatureResult_New", name, fixture.trade_.Scalar(), snapshot, &blank, &blank));
    CheckError(missing, {"bumps"});
}

TEST(ScriptExcelRawTest, TestRateCurvatureEmptyDirectionsExplicitEmptyHistoryAndSeparateCaps) {
    RawRateFixture_ fixture;
    const auto* name = &fixture.name_.cell_;
    const auto* snapshot = fixture.snapshot_.Scalar();
    auto blank = Blank(), one = Number(1.0), step = Number(0.0001);
    RawText_ inputKey(L"input_count"), numericKey(L"numeric_payload_budget_bytes"), recordingKey(L"recording_capacity_budget_bytes");
    OPER_ emptyCells[]{inputKey.cell_, one};
    auto emptySettings = Multi(emptyCells, 1, 2);
    Output_ emptyBumps(Call("xl_BumpOverAADRequest_New", name, &blank, &blank, &emptySettings));
    Output_ emptyResult(Call("xl_RateTradeQuoteCurvatureResult_New", name, fixture.trade_.Scalar(), snapshot, emptyBumps.Scalar(), &blank));
    ASSERT_EQ(emptyResult.Text().find("#Error:"), std::string::npos) << emptyResult.Text();
    Output_ products(Call("xl_RateTradeQuoteCurvatureResult_Get_HessianProducts", emptyResult.Scalar()));
    ASSERT_EQ(products.Scalar()->xltype, xltypeNil);
    Output_ shape(Call("xl_RateTradeQuoteCurvatureResult_Get_Shape", emptyResult.Scalar()));
    ASSERT_DOUBLE_EQ(shape.value_->val.array.lparray[0].val.num, 0.0);
    ASSERT_DOUBLE_EQ(shape.value_->val.array.lparray[1].val.num, 1.0);
    Output_ execution(Call("xl_RateTradeQuoteCurvatureResult_Get_Execution", emptyResult.Scalar()));
    ASSERT_DOUBLE_EQ(execution.value_->val.array.lparray[3].val.num, 1.0);
    Output_ history(Call("xl_MarketFixingSnapshot_New", &blank, &blank, &blank));
    Output_ settings(Call("xl_RateTradeQuoteCurvatureSettings_New", name, &blank, history.Scalar()));
    Output_ saved(Call("xl_RateTradeQuoteCurvatureSettings_Get_Fixings", settings.Scalar()));
    ASSERT_EQ(saved.value_->val.array.rows, 1);
    ASSERT_EQ(saved.value_->val.array.lparray[1].val.xbool, 1);
    OPER_ integer{};
    integer.xltype = xltypeInt;
    integer.val.w = 1;
    Output_ replay(Call("xl_RateCalibration_Recalibrate", name, snapshot, &integer));
    Output_ point(Call("xl_RateCalibration_Get_Point", replay.Scalar()));
    ASSERT_DOUBLE_EQ(point.Scalar()->val.num, 1.0);
    for (const auto* key : {&numericKey.cell_, &recordingKey.cell_}) {
        OPER_ capCells[]{*key, Number(0.0)};
        auto capSettings = Multi(capCells, 1, 2);
        Output_ limited(Call("xl_BumpOverAADRequest_New", name, &one, &step, &capSettings));
        Output_ rejected(Call("xl_RateTradeQuoteCurvatureResult_New", name, fixture.trade_.Scalar(), snapshot, limited.Scalar(), &blank));
        CheckError(rejected, {});
    }
}

TEST(ScriptExcelRawTest, TestDupireCurvatureAllGeneratedExportsAndCompleteQuoteMetadata) {
    const CurvatureRuntime_ runtime;
    RawText_ name(L"curvature_raw"), index(L"EQ[LOCAL]"), currency(L"USD"), factor(L"F_LOCAL"), component(L"equity"), quote(L"QUOTE"),
        quoteId(L"quote:3"), constant(L"0.001"), script(L"pay PAYS QUOTE * QUOTE");
    auto blank = Blank(), spot = Number(100.0), vol = Number(0.2), rate = Number(0.05), div = Number(0.02), spacing = Number(10.0),
         timeSpacing = Number(0.5), maxStep = Number(0.25), paths = Number(17.0);
    Output_ base(Call("xl_BSModelData_New", &name.cell_, &spot, &vol, &rate, &div));
    ASSERT_EQ(base.Text().find("#Error:"), std::string::npos) << base.Text();
    OPER_ spots[]{Number(60.0), Number(100.0), Number(140.0)}, times[]{Number(0.5), Number(1.0)};
    auto spotRange = Multi(spots, 3, 1), timeRange = Multi(times, 2, 1);
    Output_ grid(Call("xl_DupireGrid_New", &name.cell_, &spotRange, &spacing, &timeRange, &timeSpacing));
    OPER_ strikes[]{Number(75.0), Number(105.0), Number(135.0)}, maturities[]{Number(0.4), Number(1.2)};
    OPER_ spreads[]{Number(0.001), Number(0.001), Number(0.001), Number(0.001), Number(0.001), Number(0.001)};
    auto strikeRange = Multi(strikes, 3, 1), maturityRange = Multi(maturities, 2, 1), spreadRange = Multi(spreads, 3, 2);
    Output_ inputs(Call("xl_DupireRiskInputs_New", &name.cell_, &strikeRange, &maturityRange, &spreadRange, grid.Scalar()));
    Output_ calibration(Call("xl_DupireCalibration_New", &name.cell_, base.Scalar(), inputs.Scalar()));
    Output_ model(Call("xl_DupireModelData_New", &name.cell_, calibration.Scalar(), &index.cell_, &currency.cell_, &factor.cell_, &maxStep));
    OPER_ dates[]{quote.cell_, Number(Date::ToExcel(Date_(2027, 9, 12)))}, events[]{constant.cell_, script.cell_};
    auto dateRange = Multi(dates, 2, 1), eventRange = Multi(events, 2, 1);
    Output_ product(Call("xl_Product_New", &name.cell_, &dateRange, &eventRange));
    Output_ executionSettings(Call("xl_DupireScriptRiskSettings_New", &name.cell_, &paths, &blank, &blank));
    OPER_ bindings[]{Number(0.0), quoteId.cell_};
    auto bindingRange = Multi(bindings, 1, 2);
    Output_ risk(Call("xl_DupireScriptRiskRequest_New", &name.cell_, executionSettings.Scalar(), &blank, &bindingRange, &blank));
    OPER_ directionCells[18];
    for (auto& cell : directionCells)
        cell = Number(0.0);
    directionCells[3] = Number(1.0);
    directionCells[9].xltype = xltypeInt;
    directionCells[9].val.w = -2;
    directionCells[12] = Number(1.0);
    OPER_ stepCells[]{Number(0.0002), Number(0.0001), Number(0.0002)};
    auto directions = Multi(directionCells, 3, 6), steps = Multi(stepCells, 1, 3);
    Output_ bumps(Call("xl_BumpOverAADRequest_New", &name.cell_, &directions, &steps, &blank));
    ASSERT_EQ(bumps.Text().find("#Error:"), std::string::npos) << bumps.Text();
    Output_ bumpDirections(Call("xl_BumpOverAADRequest_Get_Directions", bumps.Scalar()));
    ASSERT_EQ(bumpDirections.value_->val.array.rows, 3);
    ASSERT_EQ(bumpDirections.value_->val.array.columns, 6);
    ASSERT_DOUBLE_EQ(bumpDirections.value_->val.array.lparray[9].val.num, -2.0);
    Output_ bumpSteps(Call("xl_BumpOverAADRequest_Get_Steps", bumps.Scalar()));
    ASSERT_EQ(bumpSteps.value_->val.array.rows, 3);
    ASSERT_EQ(bumpSteps.value_->val.array.columns, 1);
    Output_ bumpSettings(Call("xl_BumpOverAADRequest_Get_Settings", bumps.Scalar()));
    ASSERT_EQ(bumpSettings.value_->val.array.rows, 3);
    ASSERT_DOUBLE_EQ(bumpSettings.value_->val.array.lparray[1].val.num, 6.0);
    Output_ bumpShape(Call("xl_BumpOverAADRequest_Get_Shape", bumps.Scalar()));
    ASSERT_DOUBLE_EQ(bumpShape.value_->val.array.lparray[0].val.num, 3.0);
    ASSERT_DOUBLE_EQ(bumpShape.value_->val.array.lparray[1].val.num, 6.0);
    Output_ request(Call("xl_DupireScriptCurvatureRequest_New", &name.cell_, risk.Scalar(), bumps.Scalar()));
    ASSERT_EQ(request.Text().find("#Error:"), std::string::npos) << request.Text();
    Output_ retainedRisk(Call("xl_DupireScriptCurvatureRequest_Get_Risk", request.Scalar()));
    Output_ retainedBumps(Call("xl_DupireScriptCurvatureRequest_Get_Bumps", request.Scalar()));
    ASSERT_EQ(retainedRisk.Text().find("#Error:"), std::string::npos) << retainedRisk.Text();
    ASSERT_EQ(retainedBumps.Text().find("#Error:"), std::string::npos) << retainedBumps.Text();
    Output_ plan(Call("xl_DupireScriptCurvaturePlan_New", &name.cell_, product.Scalar(), model.Scalar(), calibration.Scalar(), &component.cell_,
                      request.Scalar()));
    ASSERT_EQ(plan.Text().find("#Error:"), std::string::npos) << plan.Text();
    Output_ basePlan(Call("xl_DupireScriptCurvaturePlan_Get_BasePlan", plan.Scalar()));
    ASSERT_EQ(basePlan.Text().find("#Error:"), std::string::npos) << basePlan.Text();
    for (const auto* exportName : {"xl_DupireScriptCurvaturePlan_Get_Point", "xl_DupireScriptCurvaturePlan_Get_Directions",
                                   "xl_DupireScriptCurvaturePlan_Get_Steps", "xl_DupireScriptCurvaturePlan_Get_Shape"}) {
        Output_ spill(Call(exportName, plan.Scalar()));
        ASSERT_EQ(spill.value_->xltype & xltypeMulti, xltypeMulti) << exportName;
        ASSERT_GT(spill.value_->val.array.rows, 0);
    }
    Output_ payload(Call("xl_DupireScriptCurvaturePlan_Get_Payload", plan.Scalar()));
    ASSERT_DOUBLE_EQ(payload.Scalar()->val.num, 720.0);
    Output_ result(Call("xl_DupireScriptCurvatureResult_New", &name.cell_, plan.Scalar()));
    ASSERT_EQ(result.Text().find("#Error:"), std::string::npos) << result.Text();
    Output_ retainedBase(Call("xl_DupireScriptCurvatureResult_Get_Base", result.Scalar()));
    ASSERT_EQ(retainedBase.Text().find("#Error:"), std::string::npos) << retainedBase.Text();
    Output_ quotePlan(Call("xl_DupireScriptCurvatureResult_Get_QuotePlan", result.Scalar()));
    OPER_ complete{};
    complete.xltype = xltypeBool;
    complete.val.xbool = 1;
    Output_ axis(Call("xl_CalibrationRiskPlan_Get_Inputs", quotePlan.Scalar(), &complete));
    ASSERT_EQ(axis.value_->val.array.rows, 7);
    ASSERT_EQ(axis.value_->val.array.columns, 12);
    Output_ point(Call("xl_DupireScriptCurvatureResult_Get_Point", result.Scalar()));
    ASSERT_EQ(point.value_->val.array.rows, 6);
    ASSERT_DOUBLE_EQ(point.value_->val.array.lparray[3].val.num, 0.001);
    Output_ gradient(Call("xl_DupireScriptCurvatureResult_Get_Gradient", result.Scalar()));
    Output_ products(Call("xl_DupireScriptCurvatureResult_Get_HessianProducts", result.Scalar()));
    ASSERT_NO_FATAL_FAILURE(CheckCurvatureFinancialSpills(gradient, products));
    for (const auto* exportName :
         {"xl_DupireScriptCurvatureResult_Get_Directions", "xl_DupireScriptCurvatureResult_Get_Steps", "xl_DupireScriptCurvatureResult_Get_Shape"}) {
        Output_ spill(Call(exportName, result.Scalar()));
        ASSERT_EQ(spill.value_->xltype & xltypeMulti, xltypeMulti) << exportName;
        ASSERT_GT(spill.value_->val.array.rows, 0);
    }
    Output_ counts(Call("xl_DupireScriptCurvatureResult_Get_Execution", result.Scalar()));
    ASSERT_EQ(counts.value_->val.array.rows, 4);
    ASSERT_EQ(counts.value_->val.array.columns, 2);
    ASSERT_DOUBLE_EQ(counts.value_->val.array.lparray[3].val.num, 7.0);
    ASSERT_DOUBLE_EQ(counts.value_->val.array.lparray[5].val.num, 17.0);
    ASSERT_DOUBLE_EQ(counts.value_->val.array.lparray[7].val.num, 720.0);
    products.value_->val.array.lparray[3].val.num = 99.0;
    Output_ fresh(Call("xl_DupireScriptCurvatureResult_Get_HessianProducts", result.Scalar()));
    ASSERT_NEAR(fresh.value_->val.array.lparray[3].val.num, 2.0 * std::exp(-0.05), 1e-10);

    RawText_ inputCount(L"input_count"), numericCap(L"numeric_payload_budget_bytes"), recordingCap(L"recording_capacity_budget_bytes");
    OPER_ emptySettingsCells[]{inputCount.cell_, Number(6.0)};
    auto emptySettings = Multi(emptySettingsCells, 1, 2);
    Output_ emptyBumps(Call("xl_BumpOverAADRequest_New", &name.cell_, &blank, &blank, &emptySettings));
    Output_ emptyRequest(Call("xl_DupireScriptCurvatureRequest_New", &name.cell_, risk.Scalar(), emptyBumps.Scalar()));
    Output_ emptyPlan(Call("xl_DupireScriptCurvaturePlan_New", &name.cell_, product.Scalar(), model.Scalar(), calibration.Scalar(), &component.cell_,
                           emptyRequest.Scalar()));
    Output_ emptyResult(Call("xl_DupireScriptCurvatureResult_New", &name.cell_, emptyPlan.Scalar()));
    ASSERT_EQ(emptyResult.Text().find("#Error:"), std::string::npos) << emptyResult.Text();
    Output_ emptyShape(Call("xl_DupireScriptCurvatureResult_Get_Shape", emptyResult.Scalar()));
    ASSERT_DOUBLE_EQ(emptyShape.value_->val.array.lparray[0].val.num, 0.0);
    ASSERT_DOUBLE_EQ(emptyShape.value_->val.array.lparray[1].val.num, 6.0);
    Output_ emptyProducts(Call("xl_DupireScriptCurvatureResult_Get_HessianProducts", emptyResult.Scalar()));
    ASSERT_EQ(emptyProducts.Scalar()->xltype, xltypeStr);
    ASSERT_EQ(emptyProducts.Scalar()->val.str[0], 0);
    Output_ emptyCounts(Call("xl_DupireScriptCurvatureResult_Get_Execution", emptyResult.Scalar()));
    ASSERT_DOUBLE_EQ(emptyCounts.value_->val.array.lparray[3].val.num, 1.0);
    ASSERT_DOUBLE_EQ(emptyCounts.value_->val.array.lparray[7].val.num, 408.0);
    for (const auto& key : {numericCap.cell_, recordingCap.cell_}) {
        OPER_ cappedCells[]{key, Number(0.0)};
        auto cappedSettings = Multi(cappedCells, 1, 2);
        Output_ cappedBumps(Call("xl_BumpOverAADRequest_New", &name.cell_, &directions, &steps, &cappedSettings));
        Output_ cappedRequest(Call("xl_DupireScriptCurvatureRequest_New", &name.cell_, risk.Scalar(), cappedBumps.Scalar()));
        Output_ rejected(Call("xl_DupireScriptCurvaturePlan_New", &name.cell_, product.Scalar(), model.Scalar(), calibration.Scalar(),
                              &component.cell_, cappedRequest.Scalar()));
        CheckError(rejected, {key.val.str == numericCap.cell_.val.str ? "budget" : "worker recording capacity"});
    }
}

TEST(ScriptExcelRawTest, TestCurvatureRawKindsIntegerNormalizationNulAndWrongHandles) {
    const CurvatureRuntime_ runtime;
    RawText_ name(L"curvature_kinds"), text(L"2"), countKey(L"input_count"), capKey(L"numeric_payload_budget_bytes");
    auto blank = Blank(), step = Number(0.0001);
    OPER_ integer{};
    integer.xltype = xltypeInt;
    integer.val.w = 1;
    auto scalarRange = Multi(&integer, 1, 1);
    OPER_ settingsCells[]{countKey.cell_, integer, capKey.cell_, Number(0.0)};
    auto settings = Multi(settingsCells, 2, 2);
    for (const OPER_* direction : {&integer, &scalarRange}) {
        Output_ accepted(Call("xl_BumpOverAADRequest_New", &name.cell_, direction, &step, &settings));
        ASSERT_EQ(accepted.Text().find("#Error:"), std::string::npos) << accepted.Text();
        Output_ copied(Call("xl_BumpOverAADRequest_Get_Directions", accepted.Scalar()));
        ASSERT_EQ(copied.Scalar()->xltype, xltypeNum);
        ASSERT_DOUBLE_EQ(copied.Scalar()->val.num, 1.0);
        Output_ retained(Call("xl_BumpOverAADRequest_Get_Settings", accepted.Scalar()));
        ASSERT_DOUBLE_EQ(retained.value_->val.array.lparray[1].val.num, 1.0);
        ASSERT_DOUBLE_EQ(retained.value_->val.array.lparray[3].val.num, 0.0);
    }
    OPER_ boolean{}, error{};
    boolean.xltype = xltypeBool;
    boolean.val.xbool = 1;
    error.xltype = xltypeErr;
    error.val.err = 15;
    for (const auto& invalid :
         {boolean, error, blank, text.cell_, Number(std::numeric_limits<double>::infinity()), Number(std::numeric_limits<double>::quiet_NaN())}) {
        OPER_ directionCells[]{Number(1.0), invalid};
        auto directionRange = Multi(directionCells, 1, 2);
        Output_ badDirection(Call("xl_BumpOverAADRequest_New", &name.cell_, &directionRange, &step, &blank));
        CheckError(badDirection, {"BumpOverAADRequest_New", "directions", "row=1 column=2"});
        Output_ badStep(Call("xl_BumpOverAADRequest_New", &name.cell_, &integer, &invalid, &blank));
        CheckError(badStep, {"BumpOverAADRequest_New", "steps"});
        settingsCells[3] = invalid;
        if (invalid.xltype != xltypeNil) {
            Output_ badCap(Call("xl_BumpOverAADRequest_New", &name.cell_, &integer, &step, &settings));
            CheckError(badCap, {"BumpOverAADRequest_New", "row=2 column=2"});
        }
    }
    RawText_ nulKey(std::wstring(L"input\0_count", 12)), nulName(std::wstring(L"bad\0name", 8));
    settingsCells[0] = nulKey.cell_;
    settingsCells[3] = blank;
    Output_ badKey(Call("xl_BumpOverAADRequest_New", &name.cell_, &integer, &step, &settings));
    CheckError(badKey, {"BumpOverAADRequest_New", "row=1 column=1", "NUL"});
    Output_ badName(Call("xl_BumpOverAADRequest_New", &nulName.cell_, &integer, &step, &blank));
    CheckError(badName, {"BumpOverAADRequest_New", "name", "NUL"});
    Output_ bumps(Call("xl_BumpOverAADRequest_New", &name.cell_, &integer, &step, &blank));
    Output_ wrongRequest(Call("xl_DupireScriptCurvatureRequest_New", &name.cell_, bumps.Scalar(), bumps.Scalar()));
    CheckError(wrongRequest, {"risk"});
    Output_ wrongResult(Call("xl_DupireScriptCurvatureResult_Get_Gradient", bumps.Scalar()));
    CheckError(wrongResult, {"result"});
    Output_ missing(Call("xl_DupireScriptCurvaturePlan_Get_Point", &blank));
    CheckError(missing, {"plan"});
}

TEST(ScriptExcelRawTest, TestEuropeanPdeAllGeneratedExportsAndOwningSpills) {
    Excel::ScriptTestInitialize(1);
    RawText_ name(L"pde_raw"), gridKey(L"grid_points"), stepKey(L"ordinary_steps");
    OPER_ gridCount{};
    gridCount.xltype = xltypeInt;
    gridCount.val.w = 9;
    OPER_ cells[]{gridKey.cell_, gridCount, stepKey.cell_, Number(8.0)};
    auto rows = Multi(cells, 2, 2), rate = Number(0.05), volatility = Number(0.20), strike = Number(110.0);
    Output_ settings(Call("xl_EuropeanPdeRiskSettings_New", &name.cell_, &rows));
    ASSERT_EQ(settings.Text().find("#Error:"), std::string::npos) << settings.Text();
    Output_ config(Call("xl_EuropeanPdeRiskSettings_Get_Configuration", settings.Scalar()));
    ASSERT_EQ(config.value_->val.array.rows, 10);
    ASSERT_DOUBLE_EQ(config.value_->val.array.lparray[1].val.num, 9.0);
    Output_ request(Call("xl_EuropeanPdeRiskRequest_New", &name.cell_, &rate, &volatility, &strike, settings.Scalar()));
    ASSERT_EQ(request.Text().find("#Error:"), std::string::npos) << request.Text();
    Output_ requestSettings(Call("xl_EuropeanPdeRiskRequest_Get_Settings", request.Scalar()));
    ASSERT_EQ(requestSettings.Text().find("#Error:"), std::string::npos) << requestSettings.Text();
    Output_ point(Call("xl_EuropeanPdeRiskRequest_Get_Point", request.Scalar()));
    ASSERT_EQ(point.value_->val.array.rows, 3);
    ASSERT_DOUBLE_EQ(point.value_->val.array.lparray[1].val.num, 0.05);
    ASSERT_DOUBLE_EQ(point.value_->val.array.lparray[3].val.num, 0.20);
    ASSERT_DOUBLE_EQ(point.value_->val.array.lparray[5].val.num, 110.0);
    Output_ result(Call("xl_EuropeanPdeRiskResult_New", &name.cell_, request.Scalar()));
    ASSERT_EQ(result.Text().find("#Error:"), std::string::npos) << result.Text();
    Output_ retained(Call("xl_EuropeanPdeRiskResult_Get_Request", result.Scalar()));
    ASSERT_EQ(retained.Text().find("#Error:"), std::string::npos) << retained.Text();
    Output_ resolvedSettings(Call("xl_EuropeanPdeRiskRequest_Get_Settings", retained.Scalar()));
    Output_ resolved(Call("xl_EuropeanPdeRiskSettings_Get_Configuration", resolvedSettings.Scalar()));
    ASSERT_DOUBLE_EQ(resolved.value_->val.array.lparray[7].val.num, 2.0);
    Output_ prices(Call("xl_EuropeanPdeRiskResult_Get_Prices", result.Scalar()));
    ASSERT_EQ(prices.value_->val.array.rows, 3);
    ASSERT_EQ(prices.value_->val.array.columns, 2);
    ASSERT_NEAR(prices.value_->val.array.lparray[3].val.num, 4.153690693968586, 1e-10);
    ASSERT_NEAR(prices.value_->val.array.lparray[5].val.num, 10.770781220697858, 1e-10);
    Output_ risks(Call("xl_EuropeanPdeRiskResult_Get_Jacobian", result.Scalar()));
    ASSERT_EQ(risks.value_->val.array.rows, 7);
    ASSERT_EQ(risks.value_->val.array.columns, 4);
    const double expected[] = {40.40684028445804, 27.8766807705311, -0.0907395605282994, -64.1496803210784, 27.87666960963085, 0.8605082862555484};
    for (int index = 0; index < 6; ++index)
        ASSERT_NEAR(risks.value_->val.array.lparray[4 * (index + 1) + 3].val.num, expected[index], 1e-9);
    Output_ grid(Call("xl_EuropeanPdeRiskResult_Get_Grid", result.Scalar()));
    ASSERT_EQ(grid.value_->val.array.rows, 10);
    ASSERT_DOUBLE_EQ(grid.value_->val.array.lparray[6].val.num, 2.0);
    ASSERT_DOUBLE_EQ(grid.value_->val.array.lparray[7].val.num, 100.0);
    for (const auto* exportName : {"xl_EuropeanPdeRiskResult_Get_ForwardErrors", "xl_EuropeanPdeRiskResult_Get_TransposeErrors"}) {
        Output_ diagnostics(Call(exportName, result.Scalar()));
        ASSERT_EQ(diagnostics.value_->val.array.rows, 11);
        const int columns = diagnostics.value_->val.array.columns;
        ASSERT_EQ(columns, std::string(exportName).find("Forward") != std::string::npos ? 3 : 5);
        for (int step = 0; step < 10; ++step) {
            ASSERT_DOUBLE_EQ(diagnostics.value_->val.array.lparray[columns * (step + 1)].val.num, step + 1);
            for (int column = 1; column < columns; ++column) {
                const auto& cell = diagnostics.value_->val.array.lparray[columns * (step + 1) + column];
                ASSERT_EQ(cell.xltype, xltypeNum);
                ASSERT_GE(cell.val.num, 0.0);
                ASSERT_LE(cell.val.num, 1e-12);
            }
        }
    }
    Output_ execution(Call("xl_EuropeanPdeRiskResult_Get_Execution", result.Scalar()));
    ASSERT_EQ(execution.value_->val.array.rows, 6);
    ASSERT_DOUBLE_EQ(execution.value_->val.array.lparray[3].val.num, 10.0);
    ASSERT_DOUBLE_EQ(execution.value_->val.array.lparray[5].val.num, 688.0);
    prices.value_->val.array.lparray[3].val.num = -99.0;
    Output_ fresh(Call("xl_EuropeanPdeRiskResult_Get_Prices", result.Scalar()));
    ASSERT_NEAR(fresh.value_->val.array.lparray[3].val.num, 4.153690693968586, 1e-10);
}

TEST(ScriptExcelRawTest, TestEuropeanPdeRawScalarAdmissionAndIntegerNormalization) {
    Excel::ScriptTestInitialize(1);
    RawText_ name(L"pde_types"), text(L"0.05");
    auto blank = Blank(), rate = Number(0.05), volatility = Number(0.20), strike = Number(110.0);
    OPER_ boolean{}, error{}, integer{};
    boolean.xltype = xltypeBool;
    boolean.val.xbool = 1;
    error.xltype = xltypeErr;
    error.val.err = 15;
    integer.xltype = xltypeInt;
    integer.val.w = 110;
    auto integerRange = Multi(&integer, 1, 1);
    for (const OPER_* input : {&integer, &integerRange}) {
        Output_ request(Call("xl_EuropeanPdeRiskRequest_New", &name.cell_, &rate, &volatility, input, &blank));
        ASSERT_EQ(request.Text().find("#Error:"), std::string::npos) << request.Text();
        Output_ point(Call("xl_EuropeanPdeRiskRequest_Get_Point", request.Scalar()));
        ASSERT_DOUBLE_EQ(point.value_->val.array.lparray[5].val.num, 110.0);
    }
    const char* fields[] = {"rate", "volatility", "strike"};
    for (int coordinate = 0; coordinate < 3; ++coordinate)
        for (const auto& invalid : {blank, boolean, error, text.cell_, Number(std::numeric_limits<double>::infinity())}) {
            const OPER_* parameters[] = {&rate, &volatility, &strike};
            parameters[coordinate] = &invalid;
            Output_ rejected(Call("xl_EuropeanPdeRiskRequest_New", &name.cell_, parameters[0], parameters[1], parameters[2], &blank));
            CheckError(rejected, {"EuropeanPdeRiskRequest_New", fields[coordinate]});
        }
    Output_ defaults(Call("xl_EuropeanPdeRiskSettings_New", &name.cell_, &blank));
    Output_ wrong(Call("xl_EuropeanPdeRiskResult_Get_Prices", defaults.Scalar()));
    CheckError(wrong, {"result"});
    Output_ missing(Call("xl_EuropeanPdeRiskResult_Get_Prices", &blank));
    CheckError(missing, {"result"});
}

TEST(ScriptExcelRawTest, TestEuropeanPdeSettingsPhysicalErrorsNulAndBudgetFailure) {
    Excel::ScriptTestInitialize(1);
    RawText_ name(L"pde_settings"), key(L"grid_points"), text(L"9"), gridKey(L"grid_points"), stepKey(L"ordinary_steps"),
        budgetKey(L"numeric_payload_budget_bytes");
    OPER_ cells[]{key.cell_, Number(9.0)};
    auto rows = Multi(cells, 1, 2);
    OPER_ boolean{}, error{};
    boolean.xltype = xltypeBool;
    boolean.val.xbool = 1;
    error.xltype = xltypeErr;
    error.val.err = 15;
    for (const auto& invalid : {Blank(), boolean, error, text.cell_, Number(0.5)}) {
        cells[1] = invalid;
        Output_ rejected(Call("xl_EuropeanPdeRiskSettings_New", &name.cell_, &rows));
        CheckError(rejected, {"EuropeanPdeRiskSettings_New", "row=1 column=2"});
    }
    RawText_ nul(std::wstring(L"grid\0_points", 12));
    cells[0] = nul.cell_;
    cells[1] = Number(9.0);
    Output_ embedded(Call("xl_EuropeanPdeRiskSettings_New", &name.cell_, &rows));
    CheckError(embedded, {"EuropeanPdeRiskSettings_New", "row=1 column=1", "NUL"});
    OPER_ budgetCells[]{gridKey.cell_, Number(9.0), stepKey.cell_, Number(8.0), budgetKey.cell_, Number(0.0)};
    auto budgetRows = Multi(budgetCells, 3, 2), rate = Number(0.05), volatility = Number(0.20), strike = Number(110.0);
    Output_ settings(Call("xl_EuropeanPdeRiskSettings_New", &name.cell_, &budgetRows));
    ASSERT_EQ(settings.Text().find("#Error:"), std::string::npos) << settings.Text();
    Output_ request(Call("xl_EuropeanPdeRiskRequest_New", &name.cell_, &rate, &volatility, &strike, settings.Scalar()));
    ASSERT_EQ(request.Text().find("#Error:"), std::string::npos) << request.Text();
    Output_ rejected(Call("xl_EuropeanPdeRiskResult_New", &name.cell_, request.Scalar()));
    CheckError(rejected, {"numeric_payload_budget_bytes"});
}

TEST(ScriptExcelRawTest, TestWeightedRequestGeneratedExportRejectsInvalidPhysicalInputs) {
    Excel::ScriptTestInitialize(1);
    RawText_ name(L"weighted_raw"), key(L"weights"), valid(L"-1;0;2");
    OPER_ cells[]{key.cell_, valid.cell_};
    auto input = Multi(cells, 1, 2);
    {
        Output_ result(Call("xl_WeightedRiskRequest_New", &name.cell_, &input));
        ASSERT_EQ(result.Text().find("#Error:"), std::string::npos) << result.Text();
    }
    for (unsigned long type : {xltypeBool, xltypeNum, xltypeErr}) {
        cells[1] = {};
        cells[1].xltype = type;
        if (type == xltypeBool)
            cells[1].val.xbool = 1;
        else if (type == xltypeNum)
            cells[1].val.num = 1.0;
        else
            cells[1].val.err = 15;
        Output_ result(Call("xl_WeightedRiskRequest_New", &name.cell_, &input));
        CheckError(result, {"WeightedRiskRequest_New", "settings row=1 column=2"});
    }
    RawText_ embedded(std::wstring(L"1\0;2", 4));
    cells[1] = embedded.cell_;
    {
        Output_ result(Call("xl_WeightedRiskRequest_New", &name.cell_, &input));
        CheckError(result, {"WeightedRiskRequest_New", "settings row=1 column=2", "NUL"});
    }
    OPER_ wideCells[]{key.cell_, valid.cell_, valid.cell_};
    auto wide = Multi(wideCells, 1, 3);
    {
        Output_ result(Call("xl_WeightedRiskRequest_New", &name.cell_, &wide));
        CheckError(result, {"WeightedRiskRequest_New", "settings"});
    }
}

TEST(ScriptExcelRawTest, TestWeightedValuationGeneratedExportRetainsIntegerPathCount) {
    Excel::ScriptTestInitialize(1);
    RawText_ name(L"weighted_integer"), script(L"pay PAYS 5");
    auto date = Number(60000.0), spot = Number(100.0), zero = Number(0.0), blank = Blank();
    Output_ product(Call("xl_Product_New", &name.cell_, &date, &script.cell_));
    Output_ model(Call("xl_BSModelData_New", &name.cell_, &spot, &zero, &zero, &zero));
    OPER_ paths{};
    paths.xltype = xltypeInt;
    paths.val.w = 17;
    auto range = Multi(&paths, 1, 1);
    for (const OPER_* input : {&paths, &range}) {
        Output_ result(Call("xl_MonteCarlo_ValueWithWeightedRisk", product.Scalar(), model.Scalar(), input, &blank, &blank, &blank));
        ASSERT_EQ(result.Text().find("#Error:"), std::string::npos) << result.Text();
        Output_ value(Call("xl_WeightedRiskResult_Get_WeightedValue", result.Scalar()));
        ASSERT_EQ(value.Scalar()->xltype, xltypeNum);
        ASSERT_DOUBLE_EQ(value.Scalar()->val.num, 5.0);
        Output_ provenance(Call("xl_WeightedRiskResult_Get_Provenance", result.Scalar()));
        ASSERT_GE(provenance.value_->val.array.rows, 7);
        ASSERT_EQ(provenance.value_->val.array.columns, 2);
        const auto& retained = provenance.value_->val.array.lparray[13];
        ASSERT_EQ(retained.xltype, xltypeNum);
        ASSERT_DOUBLE_EQ(retained.val.num, 17.0);
    }
}

TEST(ScriptExcelRawTest, TestJacobianRequestGeneratedExportRejectsPhysicalCoercion) {
    Excel::ScriptTestInitialize(1);
    RawText_ name(L"jacobian_raw"), key(L"max_block_width"), text(L"2");
    OPER_ cells[]{key.cell_, Number(2.0)};
    auto input = Multi(cells, 1, 2);
    {
        Output_ result(Call("xl_JacobianRiskRequest_New", &name.cell_, &input));
        ASSERT_EQ(result.Text().find("#Error:"), std::string::npos) << result.Text();
    }
    for (const auto& invalid : {Number(0.0), Number(0.5), Number(-1.0), text.cell_}) {
        cells[1] = invalid;
        Output_ result(Call("xl_JacobianRiskRequest_New", &name.cell_, &input));
        CheckError(result, {"JacobianRiskRequest_New", "max_block_width"});
    }
    cells[1] = {};
    cells[1].xltype = xltypeBool;
    cells[1].val.xbool = 1;
    {
        Output_ result(Call("xl_JacobianRiskRequest_New", &name.cell_, &input));
        CheckError(result, {"JacobianRiskRequest_New", "max_block_width"});
    }
    RawText_ outputKey(L"outputs"), nul(std::wstring(L"pay\0off", 7));
    cells[0] = outputKey.cell_;
    cells[1] = nul.cell_;
    {
        Output_ result(Call("xl_JacobianRiskRequest_New", &name.cell_, &input));
        CheckError(result, {"JacobianRiskRequest_New", "NUL"});
    }
    input.val.array = {cells, 1, 3};
    {
        Output_ result(Call("xl_JacobianRiskRequest_New", &name.cell_, &input));
        CheckError(result, {"JacobianRiskRequest_New", "settings"});
    }
}

TEST(ScriptExcelRawTest, TestJacobianGeneratedValuationMatrixIntegerPathsAndEmptyColumns) {
    Excel::ScriptTestInitialize(1);
    RawText_ name(L"jacobian_integer"), x(L"X"), y(L"Y"), xValue(L"2"), yValue(L"3"), script(L"a = X * Y b = X + Y pay PAYS 5");
    OPER_ dates[]{x.cell_, y.cell_, Number(60000.0)};
    OPER_ events[]{xValue.cell_, yValue.cell_, script.cell_};
    auto dateRange = Multi(dates, 3, 1), eventRange = Multi(events, 3, 1), spot = Number(100.0), zero = Number(0.0), blank = Blank();
    Output_ product(Call("xl_Product_New", &name.cell_, &dateRange, &eventRange));
    Output_ model(Call("xl_BSModelData_New", &name.cell_, &spot, &zero, &zero, &zero));
    RawText_ outputs(L"outputs"), outputIds(L"output:0;output:1;payoff"), inputs(L"inputs"), inputIds(L"constant:0;constant:1"),
        width(L"max_block_width");
    OPER_ cells[]{outputs.cell_, outputIds.cell_, inputs.cell_, inputIds.cell_, width.cell_, Number(2.0)};
    auto settings = Multi(cells, 3, 2);
    Output_ request(Call("xl_JacobianRiskRequest_New", &name.cell_, &settings));
    OPER_ paths{};
    paths.xltype = xltypeInt;
    paths.val.w = 17;
    auto pathRange = Multi(&paths, 1, 1);
    for (const OPER_* input : {&paths, &pathRange}) {
        Output_ result(Call("xl_MonteCarlo_ValueWithJacobianRisk", product.Scalar(), model.Scalar(), input, request.Scalar(), &blank, &blank));
        ASSERT_EQ(result.Text().find("#Error:"), std::string::npos) << result.Text();
        Output_ jacobian(Call("xl_JacobianRiskResult_Get_Jacobian", result.Scalar(), &blank));
        ASSERT_EQ(jacobian.value_->val.array.rows, 3);
        ASSERT_EQ(jacobian.value_->val.array.columns, 2);
        ASSERT_DOUBLE_EQ(jacobian.value_->val.array.lparray[0].val.num, 3.0);
        ASSERT_DOUBLE_EQ(jacobian.value_->val.array.lparray[1].val.num, 2.0);
        ASSERT_DOUBLE_EQ(jacobian.value_->val.array.lparray[4].val.num, 0.0);
        Output_ values(Call("xl_JacobianRiskResult_Get_Values", result.Scalar()));
        ASSERT_EQ(values.value_->val.array.rows, 4);
        ASSERT_EQ(values.value_->val.array.columns, 4);
        ASSERT_DOUBLE_EQ(values.value_->val.array.lparray[7].val.num, 6.0);
        Output_ execution(Call("xl_JacobianRiskResult_Get_Execution", result.Scalar()));
        ASSERT_DOUBLE_EQ(execution.value_->val.array.lparray[5].val.num, 34.0);
        auto badFlag = Number(1.0);
        Output_ rejected(Call("xl_JacobianRiskResult_Get_Jacobian", result.Scalar(), &badFlag));
        CheckError(rejected, {"JacobianRiskResult_Get_Jacobian", "reported"});
    }
    OPER_ emptyCells[]{inputs.cell_, blank};
    auto emptySettings = Multi(emptyCells, 1, 2);
    Output_ emptyRequest(Call("xl_JacobianRiskRequest_New", &name.cell_, &emptySettings));
    Output_ result(Call("xl_MonteCarlo_ValueWithJacobianRisk", product.Scalar(), model.Scalar(), &paths, emptyRequest.Scalar(), &blank, &blank));
    ASSERT_EQ(result.Text().find("#Error:"), std::string::npos) << result.Text();
    Output_ jacobian(Call("xl_JacobianRiskResult_Get_Jacobian", result.Scalar(), &blank));
    ASSERT_EQ(jacobian.Scalar()->xltype, xltypeStr);
    ASSERT_EQ(jacobian.Scalar()->val.str[0], 0);
    Output_ shape(Call("xl_JacobianRiskResult_Get_Shape", result.Scalar()));
    ASSERT_EQ(shape.value_->val.array.columns, 2);
    ASSERT_DOUBLE_EQ(shape.value_->val.array.lparray[0].val.num, 1.0);
    ASSERT_DOUBLE_EQ(shape.value_->val.array.lparray[1].val.num, 0.0);
}

TEST(ScriptExcelRawTest, TestPhysicalRangeErrorsAndIntegerBooleans) {
    Excel::ScriptTestInitialize(1);
    RawText_ name(L"raw"), key(L"enable_aad"), empty(L"");
    auto nil = Blank();
    OPER_ cells[] = {nil, nil, key.cell_, Number(1)};
    auto input = Multi(cells, 2, 2);
    {
        Output_ result(Call("xl_MonteCarloSettings_New", &name.cell_, &input));
        ASSERT_EQ(result.Text().find("#Error:"), std::string::npos) << result.Text();
    }
    cells[3].xltype = xltypeInt;
    cells[3].val.w = 1;
    {
        Output_ result(Call("xl_MonteCarloSettings_New", &name.cell_, &input));
        ASSERT_EQ(result.Text().find("#Error:"), std::string::npos) << result.Text();
    }
    for (unsigned long type : {xltypeErr, xltypeRef, xltypeMulti}) {
        cells[3] = {};
        cells[3].xltype = type;
        Output_ result(Call("xl_MonteCarloSettings_New", &name.cell_, &input));
        CheckError(result, {"InvalidSetting", "MonteCarloSettings_New", "settings row=2 column=2", "Excel type="});
    }
    OPER_ blanks[] = {nil, nil, nil, nil, nil, nil};
    auto wide = Multi(blanks, 2, 3);
    Output_ badWidth(Call("xl_MonteCarloSettings_New", &name.cell_, &wide));
    CheckError(badWidth, {"row=1 column=3", "rows=2", "cols=3", "2 columns"});
}

TEST(ScriptExcelRawTest, TestPortfolioFactoryPreservesPhysicalCellsAndOriginalOwnerTags) {
    Excel::ScriptTestInitialize(1);
    RawText_ name(L"portfolio_raw"), script(L"pay PAYS SPOT()"), a(L"A"), b(L"B");
    auto date = Number(60000), spot = Number(1), zero = Number(0), paths = Number(17), nil = Blank();
    Output_ product(Call("xl_Product_New", &name.cell_, &date, &script.cell_));
    Output_ model(Call("xl_BSModelData_New", &name.cell_, &spot, &zero, &zero, &zero));
    OPER_ cells[]{a.cell_, *product.Scalar(), *model.Scalar(), b.cell_, *product.Scalar(), *model.Scalar()};
    auto table = Multi(cells, 2, 3);
    Output_ portfolio(Call("xl_ScriptPortfolio_New", &name.cell_, &table));
    ASSERT_EQ(portfolio.Text().find("#Error:"), std::string::npos) << portfolio.Text();
    Output_ result(Call("xl_PortfolioMonteCarlo_ValueWithJacobianRisk", portfolio.Scalar(), &paths, &nil, &nil, &nil));
    ASSERT_EQ(result.Text().find("#Error:"), std::string::npos) << result.Text();
    Output_ trades(Call("xl_PortfolioRiskResult_Get_Trades", result.Scalar()));
    ASSERT_EQ(trades.value_->val.array.rows, 3);
    ASSERT_EQ(trades.value_->val.array.columns, 4);
    ASSERT_DOUBLE_EQ(trades.value_->val.array.lparray[6].val.num, 0.0);
    ASSERT_DOUBLE_EQ(trades.value_->val.array.lparray[10].val.num, 0.0);
    auto narrow = Multi(cells, 2, 2);
    Output_ widthError(Call("xl_ScriptPortfolio_New", &name.cell_, &narrow));
    CheckError(widthError, {"trades", "three-column"});
    const auto original = cells[0];
    for (const int type : {xltypeBool, xltypeNum, xltypeErr, xltypeNil}) {
        cells[0] = {};
        cells[0].xltype = type;
        Output_ error(Call("xl_ScriptPortfolio_New", &name.cell_, &table));
        CheckError(error, {"trades row=1 column=1", "without coercion"});
    }
    RawText_ nul(std::wstring(L"A\0B", 3));
    cells[0] = nul.cell_;
    Output_ nulError(Call("xl_ScriptPortfolio_New", &name.cell_, &table));
    CheckError(nulError, {"column=1", "NUL"});
    cells[0] = original;
    cells[1] = *model.Scalar();
    Output_ wrongType(Call("xl_ScriptPortfolio_New", &name.cell_, &table));
    CheckError(wrongType, {"trade=A", "column=2", "typed repository handle"});
}

TEST(ScriptExcelRawTest, TestPortfolioValueExportsKeepStrictPathsRequestsAndZeroColumnShapes) {
    Excel::ScriptTestInitialize(1);
    RawText_ nameA(L"portfolio_raw_A"), nameB(L"portfolio_raw_B"), x(L"X"), five(L"5"), seven(L"7"), eventA(L"pay PAYS 2 * SPOT() + X"),
        eventB(L"pay PAYS 3 * SPOT() + X"), a(L"A"), b(L"B");
    OPER_ dates[]{x.cell_, Number(60000)}, eventsA[]{five.cell_, eventA.cell_}, eventsB[]{seven.cell_, eventB.cell_};
    auto dateRange = Multi(dates, 2, 1), rangeA = Multi(eventsA, 2, 1), rangeB = Multi(eventsB, 2, 1);
    auto spot = Number(1), zero = Number(0), paths = Number(17), nil = Blank();
    Output_ productA(Call("xl_Product_New", &nameA.cell_, &dateRange, &rangeA));
    Output_ productB(Call("xl_Product_New", &nameB.cell_, &dateRange, &rangeB));
    Output_ model(Call("xl_BSModelData_New", &nameA.cell_, &spot, &zero, &zero, &zero));
    OPER_ tradeCells[]{a.cell_, *productA.Scalar(), *model.Scalar(), b.cell_, *productB.Scalar(), *model.Scalar()};
    auto table = Multi(tradeCells, 2, 3);
    Output_ portfolio(Call("xl_ScriptPortfolio_New", &nameA.cell_, &table));
    RawText_ inputs(L"inputs"), ids(L"model:0:parameter:0;trade:0:constant:0;trade:1:constant:0"), width(L"max_block_width");
    OPER_ requestCells[]{inputs.cell_, ids.cell_, width.cell_, Number(2)};
    auto requestRange = Multi(requestCells, 2, 2);
    Output_ request(Call("xl_PortfolioJacobianRiskRequest_New", &nameA.cell_, &requestRange));
    ASSERT_EQ(request.Text().find("#Error:"), std::string::npos) << request.Text();
    Output_ result(Call("xl_PortfolioMonteCarlo_ValueWithJacobianRisk", portfolio.Scalar(), &paths, request.Scalar(), &nil, &nil));
    ASSERT_EQ(result.Text().find("#Error:"), std::string::npos) << result.Text();
    Output_ raw(Call("xl_PortfolioRiskResult_Get_Jacobian", result.Scalar(), &nil));
    ASSERT_EQ(raw.value_->val.array.rows, 2);
    ASSERT_EQ(raw.value_->val.array.columns, 3);
    ASSERT_DOUBLE_EQ(raw.value_->val.array.lparray[0].val.num, 2.0);
    ASSERT_DOUBLE_EQ(raw.value_->val.array.lparray[1].val.num, 1.0);
    ASSERT_DOUBLE_EQ(raw.value_->val.array.lparray[2].val.num, 0.0);
    ASSERT_DOUBLE_EQ(raw.value_->val.array.lparray[3].val.num, 3.0);
    RawText_ weights(L"weights"), signedWeights(L"2;-1");
    OPER_ weightedCells[]{inputs.cell_, ids.cell_, weights.cell_, signedWeights.cell_};
    auto weightedRange = Multi(weightedCells, 2, 2);
    Output_ weightedRequest(Call("xl_PortfolioWeightedRiskRequest_New", &nameB.cell_, &weightedRange));
    ASSERT_EQ(weightedRequest.Text().find("#Error:"), std::string::npos) << weightedRequest.Text();
    Output_ weighted(Call("xl_PortfolioMonteCarlo_ValueWithWeightedRisk", portfolio.Scalar(), &paths, weightedRequest.Scalar(), &nil, &nil));
    ASSERT_EQ(weighted.Text().find("#Error:"), std::string::npos) << weighted.Text();
    Output_ objective(Call("xl_PortfolioRiskResult_Get_Objective", weighted.Scalar()));
    ASSERT_DOUBLE_EQ(objective.Scalar()->val.num, 4.0);
    Output_ gradient(Call("xl_PortfolioRiskResult_Get_Jacobian", weighted.Scalar(), &nil));
    ASSERT_EQ(gradient.value_->val.array.rows, 1);
    ASSERT_EQ(gradient.value_->val.array.columns, 3);
    ASSERT_DOUBLE_EQ(gradient.value_->val.array.lparray[0].val.num, 1.0);
    ASSERT_DOUBLE_EQ(gradient.value_->val.array.lparray[1].val.num, 2.0);
    ASSERT_DOUBLE_EQ(gradient.value_->val.array.lparray[2].val.num, -1.0);
    OPER_ boolean{};
    boolean.xltype = xltypeBool;
    boolean.val.xbool = true;
    requestCells[3] = boolean;
    Output_ widthType(Call("xl_PortfolioJacobianRiskRequest_New", &nameA.cell_, &requestRange));
    CheckError(widthType, {"max_block_width", "size_t integer"});
    requestCells[3] = Number(2);
    for (const OPER_* invalid : {&boolean, &zero, &ids.cell_}) {
        Output_ error(Call("xl_PortfolioMonteCarlo_ValueWithJacobianRisk", portfolio.Scalar(), invalid, request.Scalar(), &nil, &nil));
        CheckError(error, {"InvalidPathCount", "n_paths"});
    }
    Output_ reportType(Call("xl_PortfolioRiskResult_Get_Jacobian", result.Scalar(), &spot));
    CheckError(reportType, {"reported", "boolean"});
    Output_ tradeType(Call("xl_PortfolioRiskResult_Get_Product", result.Scalar(), &boolean));
    CheckError(tradeType, {"trade", "excluding bool"});
    auto fractional = Number(0.5);
    Output_ tradeFraction(Call("xl_PortfolioRiskResult_Get_Product", result.Scalar(), &fractional));
    CheckError(tradeFraction, {"trade", "exactly representable"});
    requestCells[1] = nil;
    Output_ emptyRequest(Call("xl_PortfolioJacobianRiskRequest_New", &nameB.cell_, &requestRange));
    Output_ emptyResult(Call("xl_PortfolioMonteCarlo_ValueWithJacobianRisk", portfolio.Scalar(), &paths, emptyRequest.Scalar(), &nil, &nil));
    ASSERT_EQ(emptyResult.Text().find("#Error:"), std::string::npos) << emptyResult.Text();
    Output_ shape(Call("xl_PortfolioRiskResult_Get_Shape", emptyResult.Scalar()));
    ASSERT_DOUBLE_EQ(shape.value_->val.array.lparray[0].val.num, 2.0);
    ASSERT_DOUBLE_EQ(shape.value_->val.array.lparray[1].val.num, 0.0);
    Output_ emptyRaw(Call("xl_PortfolioRiskResult_Get_Jacobian", emptyResult.Scalar(), &nil));
    ASSERT_EQ(emptyRaw.Scalar()->xltype, xltypeStr);
    ASSERT_EQ(emptyRaw.Scalar()->val.str[0], 0);
}

TEST(ScriptExcelRawTest, TestNullableInputsAndExplicitEmptySnapshot) {
    Excel::ScriptTestInitialize(1);
    const auto previous = Excel::ScriptTestSetDate(Date_(2026, 9, 12));
    RawText_ name(L"raw"), script(L"pay PAYS SPOT()"), empty(L"");
    auto date = Number(46287), spot = Number(100), zero = Number(0), paths = Number(1), nil = Blank();
    auto blankRange = Multi(&nil, 1, 1);
    Output_ product(Call("xl_Product_New", &name.cell_, &date, &script.cell_));
    Output_ model(Call("xl_BSModelData_New", &name.cell_, &spot, &zero, &zero, &zero));
    for (const auto* blank : {&nil, &blankRange, &empty.cell_}) {
        Output_ result(Call("xl_MonteCarlo_ValueWithSettings", product.Scalar(), model.Scalar(), &paths, blank, blank));
        ASSERT_EQ(result.value_->val.array.rows, 1);
        ASSERT_EQ(result.value_->val.array.columns, 2);
        const auto& pv = result.value_->val.array.lparray[1];
        ASSERT_EQ(pv.xltype, xltypeNum);
        ASSERT_DOUBLE_EQ(pv.val.num, 100.);
        Output_ valuation(Call("xl_ScriptValuationSettings_New", &name.cell_, blank, blank));
        ASSERT_EQ(valuation.Text().find("#Error:"), std::string::npos) << valuation.Text();
    }
    Output_ snapshot(Call("xl_MarketFixingSnapshot_New", &nil, &nil, &nil));
    ASSERT_FALSE(snapshot.Text().empty());
    ASSERT_EQ(snapshot.Text().find("#Error:"), std::string::npos);
    Output_ valuation(Call("xl_ScriptValuationSettings_New", &name.cell_, &nil, snapshot.Scalar()));
    RawText_ historical(L"pay PAYS FIX(EQ[AAPL], 2026-09-11)");
    Output_ historyProduct(Call("xl_Product_New", &name.cell_, &date, &historical.cell_));
    Output_ missing(Call("xl_MonteCarlo_ValueWithSettings", historyProduct.Scalar(), model.Scalar(), &paths, valuation.Scalar(), &nil));
    CheckError(missing, {"MissingFixing", "ExplicitSnapshot", "2026-09-11 00:00:00"});
    Output_ wrongClass(Call("xl_MonteCarlo_ValueWithSettings", product.Scalar(), model.Scalar(), &paths, model.Scalar(), &nil));
    CheckError(wrongClass, {"valuation"});
    Output_ zeroHandle(Call("xl_MonteCarlo_ValueWithSettings", product.Scalar(), model.Scalar(), &paths, &zero, &nil));
    CheckError(zeroHandle, {"valuation"});
    Excel::ScriptTestSetDate(previous);
}
#endif

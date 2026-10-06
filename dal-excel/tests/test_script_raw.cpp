//
// Created by Codex on 2026/9/15.
//

#include <gtest/gtest.h>

#ifdef _WIN32
#define NOMINMAX
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
} // namespace

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
    ASSERT_EQ(jacobian.Scalar()->xltype, xltypeNil);
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

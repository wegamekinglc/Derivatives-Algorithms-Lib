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

#include <gtest/gtest.h>

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#include <dal-excel/src/__script_test_api.hpp>
#include <dal-excel/src/_excel.hpp>
#include <dal-excel/src/_xlcall.hpp>

using namespace Dal;

namespace {
    struct RawText_ {
        std::wstring storage_;
        OPER_ cell_{};
        explicit RawText_(const std::wstring& text) : storage_(1, static_cast<wchar_t>(text.size())) {
            storage_ += text;
            cell_.xltype = xltypeStr;
            cell_.val.str = storage_.data();
        }
    };
    struct RawOutput_ {
        OPER_* value_;
        explicit RawOutput_(OPER_* value) : value_(value) {}
        ~RawOutput_() {
            const auto free = reinterpret_cast<void (*)(OPER_*)>(GetProcAddress(GetModuleHandleA("dal_excel.xll"), "xlAutoFree12"));
            if (free && value_)
                free(value_);
        }
    };
} // namespace

TEST(ExcelXccyAcceptanceRawTest, TestGeneratedEntryChecksEntireSelectedRange) {
    Excel::ScriptTestInitialize(1);
    const auto address = GetProcAddress(GetModuleHandleA("dal_excel.xll"), "xl_XccyMonteCarloPrecision_New");
    ASSERT_TRUE(address);
    const auto call = reinterpret_cast<OPER_* (*)(const OPER_*)>(address);
    RawText_ scale(L"error_scale"), sigma(L"sigma");
    for (const bool rawError : {false, true}) {
        OPER_ cells[8]{};
        for (auto& cell : cells)
            cell.xltype = xltypeNil;
        cells[0] = scale.cell_;
        cells[1].xltype = xltypeNum;
        cells[1].val.num = .01;
        cells[6] = sigma.cell_;
        cells[7].xltype = rawError ? xltypeErr : xltypeBool;
        if (rawError)
            cells[7].val.err = xlerrValue;
        else
            cells[7].val.xbool = 1;
        OPER_ selected{};
        selected.xltype = xltypeMulti;
        selected.val.array.lparray = cells;
        selected.val.array.rows = 4;
        selected.val.array.columns = 2;
        const RawOutput_ output(call(&selected));
        ASSERT_TRUE(output.value_);
        ASSERT_TRUE(output.value_->xltype & xltypeMulti);
        ASSERT_EQ(output.value_->val.array.rows, 1);
        ASSERT_EQ(output.value_->val.array.columns, 1);
        const auto& scalar = output.value_->val.array.lparray[0];
        ASSERT_EQ(scalar.xltype, xltypeStr);
        const std::wstring message(scalar.val.str + 1, scalar.val.str[0]);
        for (const wchar_t* expected : {L"#Error:", L"XccyMonteCarloPrecision_New", L"settings row=4 column=2", L"field=sigma"})
            ASSERT_NE(message.find(expected), std::wstring::npos);
        ASSERT_NE(message.find(rawError ? L"received Excel type=" : L"finite number excluding bool"), std::wstring::npos);
    }
}
#endif

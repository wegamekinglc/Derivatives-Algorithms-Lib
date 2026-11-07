#include <gtest/gtest.h>

#ifdef _WIN32
#define NOMINMAX
#include <dal-excel/src/__compoundedois.hpp>
#include <dal-excel/src/__script_test_api.hpp>
#include <dal-excel/src/_excel.hpp>
#include <dal-excel/src/_xlcall.hpp>

#include <Windows.h>

using namespace Dal;

namespace {
    struct OisRawTable_ {
        std::vector<std::wstring> text_;
        std::vector<OPER_> cells_;
        OPER_ range_{};
        OisRawTable_(int rows, int cols) : cells_(static_cast<size_t>(rows) * cols) {
            text_.reserve(cells_.size());
            for (auto& cell : cells_)
                cell.xltype = xltypeNil;
            range_.xltype = xltypeMulti;
            range_.val.array.lparray = cells_.data();
            range_.val.array.rows = rows;
            range_.val.array.columns = cols;
        }
        void Text(int row, int col, const std::wstring& value) {
            text_.emplace_back(1, static_cast<wchar_t>(value.size()));
            text_.back() += value;
            auto& cell = cells_[static_cast<size_t>(row) * range_.val.array.columns + col];
            cell.xltype = xltypeStr;
            cell.val.str = text_.back().data();
        }
        void Number(int row, int col, double value) {
            auto& cell = cells_[static_cast<size_t>(row) * range_.val.array.columns + col];
            cell.xltype = xltypeNum;
            cell.val.num = value;
        }
    };
    std::string OisRawCall(const char* function, OPER_* input) {
        const auto address = GetProcAddress(GetModuleHandleA("dal_excel.xll"), function);
        REQUIRE(address, "missing generated OIS export");
        auto* output = reinterpret_cast<OPER_* (*)(const OPER_*)>(address)(input);
        const auto free = reinterpret_cast<void (*)(OPER_*)>(GetProcAddress(GetModuleHandleA("dal_excel.xll"), "xlAutoFree12"));
        REQUIRE(output && free, "missing native result or release export");
        const std::unique_ptr<OPER_, decltype(free)> owned(output, free);
        REQUIRE((output->xltype & xltypeMulti) && output->val.array.rows == 1 && output->val.array.columns == 1, "expected scalar output");
        const auto& scalar = output->val.array.lparray[0];
        REQUIRE(scalar.xltype == xltypeStr && scalar.val.str, "expected text output");
        std::string result;
        for (int i = 0; i < scalar.val.str[0]; ++i)
            result.push_back(static_cast<char>(scalar.val.str[i + 1]));
        return result;
    }
    void OisRawObservations(OisRawTable_* table, int row) {
        table->Text(row, 0, L"coupon");
        table->Text(row, 1, L"2024-03-05");
        table->Number(row, 2, Date::ToExcel(Date_(2024, 3, 1)));
        table->Text(row, 3, L"2024-03-01");
        table->Number(row, 4, 4. / 360.);
        table->Text(row, 5, L"2024-03-05");
        table->Text(row, 6, L"2024-03-01");
    }
    void OisRawHeaders(OisRawTable_* table) {
        const wchar_t* headers[] = {L"coupon_id", L"rate_end", L"fixing_time", L"start", L"accrual", L"end", L"rate_start"};
        for (int j = 0; j < 7; ++j)
            table->Text(0, j, headers[j]);
    }
} // namespace

TEST(CompoundedOisNativeExcelTest, TestGeneratedExportsAndRequiredReorderedTable) {
    Excel::ScriptTestInitialize(1);
    for (const auto* function : {"CompoundedOisFixedLeg_New", "CompoundedOisCoupons_New", "CompoundedOisObservations_New",
                                 "CompoundedOisCalendar_New", "CompoundedOisScriptSpec_New", "CompoundedOis_Prepare", "CompoundedOisPreparations_New",
                                 "CompoundedOisScriptInstrument_New", "CurveScriptEvaluationRequest_New", "CurveScriptInstrument_Evaluate",
                                 "CurveScriptEvaluation_Get_Value", "CurveScriptEvaluation_Get_NativePartials", "CurveScriptEvaluation_Get_Table",
                                 "CalibrationOvernightCurves_New", "CurveScriptOvernightRoutes_New", "CalibrationStrings_New"}) {
        const auto name = std::string("xl_") + function;
        ASSERT_NE(GetProcAddress(GetModuleHandleA("dal_excel.xll"), name.c_str()), nullptr) << name;
    }
    OisRawTable_ table(2, 7);
    OisRawHeaders(&table);
    OisRawObservations(&table, 1);
    const auto valid = OisRawCall("xl_CompoundedOisObservations_New", &table.range_);
    ASSERT_NE(valid.substr(0, 7), "#Error:") << valid;
    table.cells_[11].xltype = xltypeInt;
    table.cells_[11].val.w = 1;
    const auto integer = OisRawCall("xl_CompoundedOisObservations_New", &table.range_);
    ASSERT_NE(integer.substr(0, 7), "#Error:") << integer;
    table.Number(1, 2, Date::ToExcel(Date_(2024, 3, 1)) + .5);
    const auto fractional = OisRawCall("xl_CompoundedOisObservations_New", &table.range_);
    ASSERT_EQ(fractional.substr(0, 7), "#Error:");
    ASSERT_NE(fractional.find("row=2 column=3"), std::string::npos);
}

TEST(CompoundedOisNativeExcelTest, TestFullSelectionRawErrorAndTypedErrorAfterBlank) {
    Excel::ScriptTestInitialize(1);
    OisRawTable_ table(4, 7);
    OisRawHeaders(&table);
    OisRawObservations(&table, 1);
    OisRawObservations(&table, 3);
    auto& errorCell = table.cells_[26];
    errorCell.xltype = xltypeErr;
    errorCell.val.err = xlerrNA;
    const auto raw = OisRawCall("xl_CompoundedOisObservations_New", &table.range_);
    ASSERT_EQ(raw.substr(0, 7), "#Error:");
    ASSERT_NE(raw.find("row=4 column=6"), std::string::npos);
    ASSERT_NE(raw.find("Excel type="), std::string::npos);
    errorCell.xltype = xltypeBool;
    errorCell.val.xbool = 1;
    const auto typed = OisRawCall("xl_CompoundedOisObservations_New", &table.range_);
    ASSERT_EQ(typed.substr(0, 7), "#Error:");
    ASSERT_NE(typed.find("row=4 column=6"), std::string::npos);
    ASSERT_NE(typed.find("field=end"), std::string::npos);
}

TEST(CompoundedOisNativeExcelTest, TestUnknownContextFieldAfterBlankKeepsCoordinates) {
    Excel::ScriptTestInitialize(1);
    OisRawTable_ fields(3, 2);
    fields.Text(2, 0, L"unknown_after_blank");
    fields.Number(2, 1, 1.);
    for (const auto* function : {"xl_CompoundedOisScriptSpec_New", "xl_CurveScriptEvaluationRequest_New", "xl_CompoundedOisCalendar_New",
                                 "xl_CurveInstrumentMarket_New", "xl_CurveInstrumentGeometry_New"}) {
        const auto error = OisRawCall(function, &fields.range_);
        ASSERT_EQ(error.substr(0, 7), "#Error:");
        ASSERT_NE(error.find("row=3 column=1"), std::string::npos) << error;
        ASSERT_NE(error.find("unknown_after_blank"), std::string::npos) << error;
    }
}
#endif

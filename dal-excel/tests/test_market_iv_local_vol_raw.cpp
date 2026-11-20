#include <gtest/gtest.h>

#ifdef _WIN32
#define NOMINMAX
#include <dal-excel/src/__excel_test_api.hpp>
#include <dal-excel/src/__script_test_api.hpp>
#include <dal-excel/src/_excel.hpp>
#include <dal-excel/src/_xlcall.hpp>

#include "support/excelmodule.hpp"

using namespace Dal;

TEST(MarketIvLocalVolNativeExcelTest, TestGeneratedExportsAndStrictSettings) {
    Excel::ScriptTestInitialize(1);
    const auto module = Dal::Excel::ModuleForTest();
    ASSERT_NE(module, nullptr);
    const auto registrations = RegisteredFunctionsForTest();
    for (const auto* suffix : {"ModelSettings_New", "ModelSettings_Get_Configuration", "ModelData_New", "ModelData_Get_Summary",
                               "ModelData_Get_Configuration", "ModelData_Get_Carry", "ModelData_Get_Capabilities", "ModelData_Get_Warnings",
                               "ModelData_Get_ConstructionExecution", "ModelData_Get_Execution"}) {
        const auto name = std::string("xl_MarketIvLocalVol") + suffix;
        ASSERT_NE(GetProcAddress(module, name.c_str()), nullptr) << name;
        ASSERT_TRUE(std::any_of(registrations.begin(), registrations.end(), [&](const auto& x) { return x.cName_ == String_(name); })) << name;
    }
    std::vector<std::wstring> strings{L"settings", L"index", L"EQ[A]", L"currency", L"USD", L"factor", L"W_EQ", L"horizon"};
    for (auto& text : strings)
        text.insert(text.begin(), static_cast<wchar_t>(text.size()));
    OPER_ name{}, rows{};
    name.xltype = xltypeStr;
    name.val.str = strings[0].data();
    std::vector<OPER_> cells(8);
    for (size_t i = 0; i < 7; ++i) {
        cells[i].xltype = xltypeStr;
        cells[i].val.str = strings[i + 1].data();
    }
    cells[7].xltype = xltypeBool;
    cells[7].val.xbool = true;
    rows.xltype = xltypeMulti;
    rows.val.array.rows = 4;
    rows.val.array.columns = 2;
    rows.val.array.lparray = cells.data();
    const auto create = reinterpret_cast<OPER_* (*)(const OPER_*, const OPER_*)>(GetProcAddress(module, "xl_MarketIvLocalVolModelSettings_New"));
    const auto free = reinterpret_cast<void (*)(OPER_*)>(GetProcAddress(module, "xlAutoFree12"));
    ASSERT_NE(free, nullptr);
    const std::unique_ptr<OPER_, decltype(free)> result(create(&name, &rows), free);
    ASSERT_NE(result, nullptr);
    ASSERT_EQ(result->xltype & xltypeMulti, xltypeMulti);
    const auto& error = result->val.array.lparray[0];
    ASSERT_EQ(error.xltype, xltypeStr);
    const std::wstring message(error.val.str + 1, error.val.str[0]);
    ASSERT_NE(message.find(L"InvalidMarketIvLocalVolRequest"), std::wstring::npos);
}
#endif

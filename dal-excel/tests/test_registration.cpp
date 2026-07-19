//
// Created by wegam on 2026/7/19.
//

#ifdef _WIN32

#include <gtest/gtest.h>

#define NOMINMAX
#include <Windows.h>

#include <algorithm>
#include <cctype>
#include <set>
#include <string>

#include <dal-excel/src/__excel_test_api.hpp>

using namespace Dal;

namespace {
    int DeclaredArgCount(const ExcelFuncRegistration_& reg) {
        // trailing Excel type-text modifiers (e.g. '!' for volatile) declare no argument
        String_ types = reg.argTypes_;
        while (!types.empty() && (types.back() == '!' || types.back() == '#' || types.back() == '$'))
            types.pop_back();
        return static_cast<int>(types.size()) - 1;
    }

    int NamedArgCount(const ExcelFuncRegistration_& reg) {
        if (reg.argNames_.empty())
            return 0;
        return 1 + static_cast<int>(std::count(reg.argNames_.begin(), reg.argNames_.end(), ','));
    }

    std::string UpperDotted(const String_& c_name) {
        std::string retval(c_name.begin(), c_name.end());
        for (auto& ch : retval)
            ch = ch == '_' ? '.' : static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
        return retval;
    }
} // namespace

TEST(ExcelRegistrationTest, TestRegistrationTableIsPopulated) {
    ASSERT_GE(RegisteredFunctionsForTest().size(), 62);
}

TEST(ExcelRegistrationTest, TestEveryRegisteredFunctionResolvesToAnEntryPoint) {
    const HMODULE dll = ::GetModuleHandleA("dal_excel.xll");
    ASSERT_TRUE(dll != nullptr);
    for (const auto& reg : RegisteredFunctionsForTest())
        ASSERT_TRUE(::GetProcAddress(dll, reg.cName_.c_str()) != nullptr) << reg.cName_.c_str();
}

TEST(ExcelRegistrationTest, TestNoDuplicateExcelNames) {
    std::set<std::string> seen;
    for (const auto& reg : RegisteredFunctionsForTest()) {
        const std::string name(reg.xlName_.c_str());
        ASSERT_TRUE(seen.insert(name).second) << "duplicate Excel name: " << name;
    }
}

TEST(ExcelRegistrationTest, TestNoDuplicateCNames) {
    std::set<std::string> seen;
    for (const auto& reg : RegisteredFunctionsForTest()) {
        const std::string name(reg.cName_.c_str());
        ASSERT_TRUE(seen.insert(name).second) << "duplicate C entry point: " << name;
    }
}

TEST(ExcelRegistrationTest, TestArgumentMetadataIsConsistent) {
    for (const auto& reg : RegisteredFunctionsForTest()) {
        const int declared = DeclaredArgCount(reg);
        ASSERT_EQ(declared, NamedArgCount(reg)) << reg.cName_.c_str();
        ASSERT_EQ(declared, reg.argHelpCount_) << reg.cName_.c_str();
    }
}

TEST(ExcelRegistrationTest, TestCNamesFollowWrapperConvention) {
    for (const auto& reg : RegisteredFunctionsForTest()) {
        ASSERT_EQ(reg.cName_.substr(0, 3), String_("xl_")) << reg.cName_.c_str();
        ASSERT_FALSE(reg.xlName_.empty());
    }
}

TEST(ExcelRegistrationTest, TestExcelNameMatchesCNameConvention) {
    for (const auto& reg : RegisteredFunctionsForTest()) {
        const std::string expected = UpperDotted(reg.cName_.substr(3));
        const std::string actual(reg.xlName_.c_str());
        ASSERT_TRUE(actual == expected || actual == "DA." + expected) << reg.cName_.c_str() << " -> " << actual;
    }
}

#endif

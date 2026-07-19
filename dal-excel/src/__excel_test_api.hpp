//
// Created by wegam on 2026/7/19.
//

#pragma once

#include <dal/platform/platform.hpp>
#include <dal/math/vectors.hpp>
#include <dal/string/strings.hpp>

#if defined(_WIN32) && defined(DAL_EXCEL_TEST_API_EXPORTS)
#define DAL_EXCEL_TEST_API __declspec(dllexport)
#elif defined(_WIN32) && defined(DAL_EXCEL_TEST_API_IMPORTS)
#define DAL_EXCEL_TEST_API __declspec(dllimport)
#else
#define DAL_EXCEL_TEST_API
#endif

namespace Dal {
    struct ExcelFuncRegistration_ {
        String_ cName_;
        String_ xlName_;
        String_ argTypes_;
        String_ argNames_;
        int argHelpCount_;
        bool volatile_;
    };

    DAL_EXCEL_TEST_API Vector_<ExcelFuncRegistration_> RegisteredFunctionsForTest();
    DAL_EXCEL_TEST_API bool EmptyHandleOutputIsErrorForTest(String_* message);
    DAL_EXCEL_TEST_API String_ ErrorCellTextForTest(const String_& what);
    DAL_EXCEL_TEST_API bool EmptyVectorOutputIsBlankForTest(int* rows, int* cols);
} // namespace Dal

#undef DAL_EXCEL_TEST_API

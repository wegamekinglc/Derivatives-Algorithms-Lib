//
// Created by wegamekinglc on 2026-08-15.
//

#include <gtest/gtest.h>
#include <dal/platform/config.hpp>
#include <dal/io/exceldriverlite.hpp>

// dal/io/exceldriverlite is a COM automation driver guarded by USE_EXCEL_REPORT
// and requires MSVC plus a local Microsoft Office installation (see
// dal/io/excelimport.hpp, which #errors on every other toolchain). On Linux the
// module compiles to nothing, so there is no runtime behavior to exercise; this
// guard fails loudly if the driver is ever enabled without matching tests.

TEST(IoTest, TestExcelDriverLiteIsCompiledOutWithoutExcelReport) {
#ifdef USE_EXCEL_REPORT
    FAIL() << "USE_EXCEL_REPORT is defined; add platform-specific ExcelDriver_ tests";
#else
    SUCCEED();
#endif
}

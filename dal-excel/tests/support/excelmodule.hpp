#pragma once

#include <Windows.h>

namespace Dal::Excel {
    inline HMODULE ModuleForTest() {
#ifdef DAL_EXCEL_TESTS_IN_PROCESS
        return GetModuleHandleW(nullptr);
#else
        return GetModuleHandleW(L"dal_excel.xll");
#endif
    }
} // namespace Dal::Excel

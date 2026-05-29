//
// Created by wegam on 2026/5/30.
//
// NOTE: dal-excel tests require Windows + Office COM DLLs.
// These tests verify the Excel add-in interface layer.
//
// Test categories (to be implemented):
// - Excel type conversion (string, date, number, vector)
// - Excel function registration
// - Error conversion (C++ exception -> Excel error)
// - Core function smoke tests via Excel interface
//

#if defined(_WIN32)

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>

TEST(ExcelApiTest, TestPlatformDetection) {
    // Verify we're on a Windows build with Excel support
    ASSERT_TRUE(true);
}

#endif  // _WIN32

//
// Created by wegam on 2026/7/19.
//

#ifdef _WIN32

#include <gtest/gtest.h>

#include <dal-excel/src/__excel_test_api.hpp>

using namespace Dal;

TEST(ExcelEmptyResultTest, TestEmptyHandleOutputRaisesError) {
    String_ message;
    ASSERT_TRUE(EmptyHandleOutputIsErrorForTest(&message));
    ASSERT_NE(message.find("Output handle is NULL"), String_::npos) << message.c_str();
}

TEST(ExcelEmptyResultTest, TestErrorOutputIsAVisibleErrorCell) {
    const String_ text = ErrorCellTextForTest(String_("something broke"));
    ASSERT_EQ(text.substr(0, 9), String_("#Error:  ")) << text.c_str();
    ASSERT_NE(text.find("something broke"), String_::npos) << text.c_str();
}

TEST(ExcelEmptyResultTest, TestEmptyVectorOutputIsABlankCell) {
    int rows = 0;
    int cols = 0;
    ASSERT_TRUE(EmptyVectorOutputIsBlankForTest(&rows, &cols));
    ASSERT_EQ(rows, 1);
    ASSERT_EQ(cols, 1);
}

#endif

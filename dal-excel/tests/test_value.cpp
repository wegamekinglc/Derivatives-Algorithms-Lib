//
// Created by wegam on 2026/7/11.
//

#include <gtest/gtest.h>

#include <dal-excel/src/__value.hpp>

#include <limits>

TEST(ExcelValueTest, TestPathCountRejectsInvalidWorksheetNumbers) {
    ASSERT_THROW(Dal::Excel::CheckedMonteCarloPathCount(std::numeric_limits<double>::quiet_NaN()), Dal::Exception_);
    ASSERT_THROW(Dal::Excel::CheckedMonteCarloPathCount(std::numeric_limits<double>::infinity()), Dal::Exception_);
    ASSERT_THROW(Dal::Excel::CheckedMonteCarloPathCount(-std::numeric_limits<double>::infinity()), Dal::Exception_);
    ASSERT_THROW(Dal::Excel::CheckedMonteCarloPathCount(1.5), Dal::Exception_);
    ASSERT_THROW(Dal::Excel::CheckedMonteCarloPathCount(0.0), Dal::Exception_);
    ASSERT_THROW(Dal::Excel::CheckedMonteCarloPathCount(-1.0), Dal::Exception_);
    ASSERT_THROW(Dal::Excel::CheckedMonteCarloPathCount(static_cast<double>(std::numeric_limits<int>::max()) + 1.0), Dal::Exception_);
}

TEST(ExcelValueTest, TestPathCountAcceptsNativeBounds) {
    ASSERT_EQ(Dal::Excel::CheckedMonteCarloPathCount(1.0), 1);
    ASSERT_EQ(Dal::Excel::CheckedMonteCarloPathCount(static_cast<double>(std::numeric_limits<int>::max())), std::numeric_limits<int>::max());
}

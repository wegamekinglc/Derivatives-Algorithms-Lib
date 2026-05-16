//
// Created by Copilot on 2026/5/14.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/currency/currency.hpp>
#include <dal/time/date.hpp>
#include <dal/time/periodlength.hpp>
#include <dal/utilities/exceptions.hpp>

using namespace Dal;

TEST(PeriodLengthTest, TestMonthsForNamedAndAliasValues) {
    ASSERT_EQ(PeriodLength_("ANNUAL").Months(), 12);
    ASSERT_EQ(PeriodLength_("12M").Months(), 12);
    ASSERT_EQ(PeriodLength_("SEMI").Months(), 6);
    ASSERT_EQ(PeriodLength_("6M").Months(), 6);
    ASSERT_EQ(PeriodLength_("QUARTERLY").Months(), 3);
    ASSERT_EQ(PeriodLength_("3M").Months(), 3);
    ASSERT_EQ(PeriodLength_("MONTHLY").Months(), 1);
    ASSERT_EQ(PeriodLength_("1M").Months(), 1);
}

TEST(PeriodLengthTest, TestConstructionRejectsInvalidValue) {
    ASSERT_THROW(PeriodLength_("2M"), Dal::Exception_);
}

TEST(PeriodLengthTest, TestNominalMaturityPreservesDayWhenAvailable) {
    const Date_ start(2024, 1, 15);
    const Date_ maturity = Date::NominalMaturity(start, PeriodLength_("3M"), Ccy_("USD"));
    ASSERT_EQ(maturity, Date_(2024, 4, 15));
}

TEST(PeriodLengthTest, TestNominalMaturityClampsEndOfMonthAcrossLeapYear) {
    const Date_ start(2024, 2, 29);
    const Date_ maturity = Date::NominalMaturity(start, PeriodLength_("12M"), Ccy_("USD"));
    ASSERT_EQ(maturity, Date_(2025, 2, 28));
}

TEST(PeriodLengthTest, TestNominalMaturityWrapsIntoNextYear) {
    const Date_ start(2024, 10, 31);
    const Date_ maturity = Date::NominalMaturity(start, PeriodLength_("6M"), Ccy_("USD"));
    ASSERT_EQ(maturity, Date_(2025, 4, 30));
}

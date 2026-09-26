//
// Created by wegamekinglc on 2021/9/26.
//

#include <gtest/gtest.h>
#include <string>
#include <utility>
#include <dal/platform/platform.hpp>
#include <dal/time/daybasis.hpp>

using Dal::Date_;
using Dal::DayBasis_;
using Dal::DayBasis::Context_;

TEST(DayBasisTest, TestAct_365F) {
    Date_ from(2008, 2, 1);
    Date_ to(2009, 5, 31);

    DayBasis_ basis("ACT_365F");
    ASSERT_NEAR(basis(from, to, nullptr), 1.32877, 1e-4);
}

TEST(DayBasisTest, TestBond) {
    Date_ from(2008, 2, 1);
    Date_ to(2009, 5, 31);

    DayBasis_ basis("BOND");
    ASSERT_NEAR(basis(from, to, nullptr), 1.33333, 1e-4);
}

TEST(DayBasisTest, TestAct_360) {
    Date_ from(2008, 2, 1);
    Date_ to(2009, 5, 31);

    DayBasis_ basis("ACT_360");
    ASSERT_NEAR(basis(from, to, nullptr), 1.34722, 1e-4);
}

TEST(DayBasisTest, TestAct_Act) {
    Date_ from(2008, 2, 1);
    Date_ to(2009, 5, 31);

    DayBasis_ basis("ACT_ACT");
    ASSERT_NEAR(basis(from, to, nullptr), 1.32626, 1e-4);
}

TEST(DayBasisTest, TestBondEndOfMonthRules) {
    const DayBasis_ basis("30_360");
    const auto days = [&basis](const Date_& from, const Date_& to) { return basis(from, to, nullptr) * 360.0; };

    ASSERT_NEAR(days(Date_(2023, 3, 31), Date_(2023, 6, 30)), 90.0, 1e-10);
    ASSERT_NEAR(days(Date_(2024, 1, 31), Date_(2024, 2, 29)), 29.0, 1e-10);
    ASSERT_NEAR(days(Date_(2023, 3, 30), Date_(2023, 5, 31)), 60.0, 1e-10);
    ASSERT_NEAR(days(Date_(2023, 3, 31), Date_(2023, 5, 31)), 60.0, 1e-10);
    ASSERT_NEAR(days(Date_(2023, 1, 15), Date_(2023, 3, 31)), 76.0, 1e-10);
    ASSERT_NEAR(days(Date_(2023, 2, 28), Date_(2023, 8, 31)), 180.0, 1e-10);
    ASSERT_NEAR(days(Date_(2023, 2, 28), Date_(2023, 3, 31)), 30.0, 1e-10);
    ASSERT_NEAR(days(Date_(2024, 2, 29), Date_(2025, 2, 28)), 360.0, 1e-10);
}

TEST(DayBasisTest, TestBondAliasesAgree) {
    const Date_ from(2023, 3, 31);
    const Date_ to(2023, 6, 30);
    const double expected = DayBasis_("BOND")(from, to, nullptr);
    for (const char* name : {"30_360", "30/360", "30_360_US"})
        ASSERT_DOUBLE_EQ(DayBasis_(name)(from, to, nullptr), expected);
}

TEST(DayBasisTest, TestAct_365LAnnualCoupons) {
    const DayBasis_ basis("ACT_365L");
    const auto annual = [&basis](const Date_& from, const Date_& to) {
        const Context_ context(true, from, to, 12);
        return basis(from, to, &context);
    };

    ASSERT_NEAR(annual(Date_(2021, 6, 15), Date_(2022, 6, 15)), 365.0 / 365.0, 1e-10);
    ASSERT_NEAR(annual(Date_(2023, 6, 15), Date_(2024, 6, 15)), 366.0 / 366.0, 1e-10);
    ASSERT_NEAR(annual(Date_(2023, 3, 1), Date_(2024, 2, 29)), 365.0 / 366.0, 1e-10);
    ASSERT_NEAR(annual(Date_(2024, 2, 29), Date_(2025, 2, 28)), 365.0 / 365.0, 1e-10);
    ASSERT_NEAR(annual(Date_(2024, 3, 1), Date_(2025, 3, 1)), 365.0 / 365.0, 1e-10);
    ASSERT_NEAR(annual(Date_(2024, 1, 10), Date_(2024, 2, 28)), 49.0 / 365.0, 1e-10);
    ASSERT_NEAR(annual(Date_(2021, 1, 1), Date_(2025, 1, 1)), 1461.0 / 366.0, 1e-10);
}

TEST(DayBasisTest, TestAct_365LAnnualUsesNominalEnd) {
    const DayBasis_ basis("ACT_365L");
    const Context_ context(true, Date_(2023, 6, 15), Date_(2024, 6, 15), 12);
    ASSERT_NEAR(basis(Date_(2023, 6, 15), Date_(2023, 12, 15), &context), 183.0 / 366.0, 1e-10);
}

TEST(DayBasisTest, TestAct_365LNonAnnualCoupons) {
    const DayBasis_ basis("ACT_365L");
    const Context_ leapEnd(false, Date_(2023, 12, 1), Date_(2024, 3, 1), 3);
    ASSERT_NEAR(basis(Date_(2023, 12, 1), Date_(2024, 3, 1), &leapEnd), 91.0 / 366.0, 1e-10);

    const Context_ plainEnd(false, Date_(2024, 12, 1), Date_(2025, 3, 1), 3);
    ASSERT_NEAR(basis(Date_(2024, 12, 1), Date_(2025, 3, 1), &plainEnd), 90.0 / 365.0, 1e-10);
}

TEST(DayBasisTest, TestAct_365LRequiresContext) {
    ASSERT_THROW(DayBasis_("ACT_365L")(Date_(2024, 1, 1), Date_(2024, 7, 1), nullptr), Dal::Exception_);
}

TEST(DayBasisTest, TestNonAsciiNameIsRejected) {
    ASSERT_THROW(DayBasis_(Dal::String_(std::string("ACT\xE9_365F"))), Dal::Exception_);
    ASSERT_THROW(DayBasis_(Dal::String_(std::string("\xFF\xFE\x80"))), Dal::Exception_);
}

TEST(DayBasisTest, TestYearBasedCountsNearUpperBound) {
    const Date_ from(2149, 1, 1);
    const Date_ to(2149, 6, 5);
    ASSERT_NEAR(DayBasis_("ACT_ACT")(from, to, nullptr), 155.0 / 365.0, 1e-10);
    ASSERT_NEAR(DayBasis_("ACT_ACT")(Date_(2147, 7, 1), to, nullptr), 184.0 / 365.0 + 1.0 + 155.0 / 365.0, 1e-10);

    const Context_ semiAnnual(true, from, to, 6);
    ASSERT_NEAR(DayBasis_("ACT_365L")(from, to, &semiAnnual), 155.0 / 365.0, 1e-10);
    const Context_ annual(true, Date_(2148, 6, 5), to, 12);
    ASSERT_NEAR(DayBasis_("ACT_365L")(Date_(2148, 6, 5), to, &annual), 365.0 / 365.0, 1e-10);
}

TEST(DayBasisTest, TestAct_ActReversedPeriodsAreAntisymmetric) {
    const DayBasis_ basis("ACT_ACT");
    // Reference values from QuantLib ActualActual(ISDA)
    ASSERT_NEAR(basis(Date_(2019, 1, 1), Date_(2015, 1, 1), nullptr), -4.0, 1e-12);
    ASSERT_NEAR(basis(Date_(2019, 1, 1), Date_(2008, 12, 31), nullptr), -(1.0 / 366.0 + 10.0), 1e-12);
    for (const auto& [from, to] : {std::pair{Date_(2023, 6, 15), Date_(2024, 6, 15)}, std::pair{Date_(2020, 2, 29), Date_(2026, 3, 1)}})
        ASSERT_DOUBLE_EQ(basis(to, from, nullptr), -basis(from, to, nullptr));
}

#include <gtest/gtest.h>
#include <dal/platform/strict.hpp>
#include <dal/time/daybasis.hpp>

using Dal::Date_;
using Dal::DayBasis_;

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

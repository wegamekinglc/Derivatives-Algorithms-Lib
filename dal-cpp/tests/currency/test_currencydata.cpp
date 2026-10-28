//
// Created by wegam on 2022/2/10.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>

#include <dal/currency/currencydata.hpp>
#include <dal/protocol/collateraltype.hpp>
#include <dal/protocol/rateconvention.hpp>
#include <dal/time/daybasis.hpp>
#include <dal/time/holidays.hpp>
#include <dal/time/periodlength.hpp>
#include <dal/time/schedules.hpp>
#include <dal/utilities/factsinternal.hpp>

using namespace Dal;

namespace {
    void CheckLiborIndexDefault(const RateIndexConvention_& c) {
        ASSERT_EQ(c.spotLag_, 2);
        ASSERT_EQ(c.fixingLag_, 2);
        ASSERT_TRUE(c.useProjectionCurve_);
        ASSERT_TRUE(c.forecastTenor_ == PeriodLength_("3M"));
        ASSERT_TRUE(c.dayBasis_ == DayBasis_("ACT_360"));
        ASSERT_TRUE(c.businessDayConvention_ == BizDayConvention_("Following"));
        ASSERT_TRUE(c.collateral_ == CollateralType_(CollateralType_::Value_::OIS));
    }

    void CheckFloatLegDefault(const RateLegConvention_& c) {
        ASSERT_TRUE(c.paymentFrequency_ == PeriodLength_("3M"));
        ASSERT_TRUE(c.dayBasis_ == DayBasis_("ACT_360"));
        ASSERT_TRUE(c.businessDayConvention_ == BizDayConvention_("ModifiedFollowing"));
        ASSERT_TRUE(c.paymentConvention_ == BizDayConvention_("ModifiedFollowing"));
    }
} // namespace

TEST(CurrencyDataTest, TestFactWrite) {
    FactInternal::StoredFact_<Ccy_, int> fact;
    fact.XWrite().SetDefault(2);
    fact.XWrite()(Ccy_("EUR"), 1);
    ASSERT_EQ(fact(Ccy_("EUR")), 1);
    ASSERT_EQ(fact(Ccy_("USD")), 2);
}

TEST(CurrencyDataTest, TestXcsDefaultConventionHasNoNotionalModeBooleans) {
    const CrossCurrencyConvention_& xcs = Ccy::Conventions::Xcs()(Ccy_("USD"));

    ASSERT_TRUE(xcs.initialNotionalExchange_);
    ASSERT_TRUE(xcs.finalNotionalExchange_);
    ASSERT_TRUE(xcs.spreadOnForeignLeg_);

    CheckLiborIndexDefault(xcs.domesticIndex_);
    CheckLiborIndexDefault(xcs.foreignIndex_);
    CheckFloatLegDefault(xcs.domesticLeg_);
    CheckFloatLegDefault(xcs.foreignLeg_);
}

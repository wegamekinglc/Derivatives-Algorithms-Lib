//
// Created by wegam on 2026/5/17.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/currency/currency.hpp>
#include <dal/currency/currencydata.hpp>
#include <dal/protocol/collateraltype.hpp>
#include <dal/time/daybasis.hpp>
#include <dal/time/holidays.hpp>
#include <dal/time/periodlength.hpp>

using namespace Dal;

//  Ccy_ enum tests

TEST(CcyTest, TestDefaultConstructor) {
    Ccy_ ccy;
    ASSERT_EQ(ccy.Switch(), Ccy_::Value_::_NOT_SET);
}

TEST(CcyTest, TestConstructionFromValue) {
    Ccy_ ccy(Ccy_::Value_::USD);
    ASSERT_EQ(ccy.Switch(), Ccy_::Value_::USD);
}

TEST(CcyTest, TestConstructionFromString) {
    Ccy_ ccy("USD");
    ASSERT_EQ(ccy.Switch(), Ccy_::Value_::USD);
    ASSERT_STREQ(ccy.String(), "USD");
}

TEST(CcyTest, TestConstructionFromStringCaseInsensitive) {
    Ccy_ ccy("usd");
    ASSERT_EQ(ccy.Switch(), Ccy_::Value_::USD);
}

TEST(CcyTest, TestConstructionFromInvalidString) {
    ASSERT_THROW(Ccy_("XXX"), Dal::Exception_);
}

TEST(CcyTest, TestStringForAllCurrencies) {
    ASSERT_STREQ(Ccy_(Ccy_::Value_::USD).String(), "USD");
    ASSERT_STREQ(Ccy_(Ccy_::Value_::EUR).String(), "EUR");
    ASSERT_STREQ(Ccy_(Ccy_::Value_::GBP).String(), "GBP");
    ASSERT_STREQ(Ccy_(Ccy_::Value_::JPY).String(), "JPY");
    ASSERT_STREQ(Ccy_(Ccy_::Value_::AUD).String(), "AUD");
    ASSERT_STREQ(Ccy_(Ccy_::Value_::CHF).String(), "CHF");
    ASSERT_STREQ(Ccy_(Ccy_::Value_::CAD).String(), "CAD");
    ASSERT_STREQ(Ccy_(Ccy_::Value_::CNY).String(), "CNY");
}

TEST(CcyTest, TestListAll) {
    auto all = CcyListAll();
    ASSERT_EQ(all.size(), 8);
}

TEST(CcyTest, TestEquality) {
    Ccy_ usd("USD");
    Ccy_ eur("EUR");
    ASSERT_TRUE(usd == usd);
    ASSERT_TRUE(usd != eur);
    ASSERT_TRUE(usd == Ccy_::Value_::USD);
    ASSERT_TRUE(usd != Ccy_::Value_::EUR);
}

TEST(CcyTest, TestComparison) {
    Ccy_ usd("USD");
    Ccy_ eur("EUR");
    ASSERT_TRUE(usd < eur);
}

//  Read-only tests of facts initialized by CcyFacts_::Init()

TEST(CurrencyFactTest, TestLiborFixDaysPreInitialized) {
    ASSERT_EQ(Ccy::Conventions::LiborFixDays()(Ccy_("CNY")), 1);
    ASSERT_EQ(Ccy::Conventions::LiborFixDays()(Ccy_("USD")), 2);
}

TEST(CurrencyFactTest, TestLiborFixHolidaysPreInitialized) {
    ASSERT_EQ(Ccy::Conventions::LiborFixHolidays()(Ccy_("CNY")).String(), "CN.IB");
}

//  Write/read tests on facts that are not pre-initialized

TEST(CurrencyFactTest, TestSwapPayHolidaysWriteRead) {
    Ccy::Conventions::SwapPayHolidays().XWrite()(Ccy_("CNY"), Holidays_("CN.IB"));
    ASSERT_EQ(Ccy::Conventions::SwapPayHolidays()(Ccy_("CNY")).String(), "CN.IB");
}

TEST(CurrencyFactTest, TestSwapFixedDayBasisSetDefault) {
    Ccy::Conventions::SwapFixedDayBasis().XWrite().SetDefault(DayBasis_("ACT_360"));
    ASSERT_TRUE(Ccy::Conventions::SwapFixedDayBasis()(Ccy_("USD")) == DayBasis_("ACT_360"));
}

TEST(CurrencyFactTest, TestSwapFixedDayBasisSpecificOverride) {
    Ccy::Conventions::SwapFixedDayBasis().XWrite().SetDefault(DayBasis_("ACT_365F"));
    Ccy::Conventions::SwapFixedDayBasis().XWrite()(Ccy_("GBP"), DayBasis_("ACT_360"));
    ASSERT_TRUE(Ccy::Conventions::SwapFixedDayBasis()(Ccy_("USD")) == DayBasis_("ACT_365F"));
    ASSERT_TRUE(Ccy::Conventions::SwapFixedDayBasis()(Ccy_("GBP")) == DayBasis_("ACT_360"));
}

TEST(CurrencyFactTest, TestLiborDayBasisSetDefault) {
    Ccy::Conventions::LiborDayBasis().XWrite().SetDefault(DayBasis_("ACT_365F"));
    ASSERT_TRUE(Ccy::Conventions::LiborDayBasis()(Ccy_("USD")) == DayBasis_("ACT_365F"));
}

TEST(CurrencyFactTest, TestDefaultRateConventionsAreInitialized) {
    ASSERT_EQ(Ccy::Conventions::SwapPayHolidays()(Ccy_("USD")).String(), "");
    ASSERT_TRUE(Ccy::Conventions::OisIndex()(Ccy_("USD")).collateral_ == CollateralType_::Value_::OIS);
    ASSERT_TRUE(Ccy::Conventions::LiborIndex()(Ccy_("USD")).useProjectionCurve_);
    ASSERT_EQ(Ccy::Conventions::LiborIndex()(Ccy_("USD")).forecastTenor_, PeriodLength_("3M"));
    ASSERT_EQ(Ccy::Conventions::SwapFixedLeg()(Ccy_("USD")).paymentFrequency_, PeriodLength_("6M"));
    ASSERT_EQ(Ccy::Conventions::SwapFloatLeg()(Ccy_("USD")).paymentFrequency_, PeriodLength_("3M"));
}

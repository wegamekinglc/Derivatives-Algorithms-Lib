//
// Created by wegamekinglc on 2026/6/20.
//

#include <gtest/gtest.h>

#include <dal-public/src/curvedata.hpp>
#include <dal-public/src/curveprotocol.hpp>

using Dal::CollateralType_OIS;
using Dal::CurveBlockNew;
using Dal::Date_;
using Dal::DayBasis_New;
using Dal::DiscountPWLFNew;
using Dal::Handle_;
using Dal::PeriodLength_New;
using Dal::String_;
using Dal::Vector_;

namespace {

Date_ Today() { return Date_(2025, 6, 20); }
Date_ Spot() { return Today().AddDays(2); }

} // namespace

// ============================================================================
// DiscountPWLFNew
// ============================================================================

TEST(CurveDataTest, TestDiscountPWLFNew) {
    Vector_<Date_> knotDates;
    knotDates.push_back(Spot());
    knotDates.push_back(Spot().AddDays(1825)); // 5 years
    Vector_<> fwdRates;
    fwdRates.push_back(0.04);
    fwdRates.push_back(0.04);
    auto curve = DiscountPWLFNew(String_("flat"), String_("USD"), knotDates, fwdRates);
    ASSERT_TRUE(curve != nullptr);
}

TEST(CurveDataTest, TestDiscountPWLFNewMultipleKnots) {
    Vector_<Date_> knotDates;
    Vector_<> fwdRates;
    for (int y : {0, 1, 2, 5, 10}) {
        knotDates.push_back(Spot().AddDays(y * 365));
        fwdRates.push_back(0.05);
    }
    auto curve = DiscountPWLFNew(String_("multi"), String_("USD"), knotDates, fwdRates);
    ASSERT_TRUE(curve != nullptr);
}

TEST(CurveDataTest, TestDiscountPWLFNewWithBase) {
    Vector_<Date_> knotDates;
    knotDates.push_back(Spot());
    knotDates.push_back(Spot().AddDays(365));
    Vector_<> fwdRates;
    fwdRates.push_back(0.03);
    fwdRates.push_back(0.03);
    auto base = DiscountPWLFNew(String_("base"), String_("USD"), knotDates, fwdRates);
    ASSERT_TRUE(base != nullptr);

    Vector_<Date_> knotDates2;
    knotDates2.push_back(Spot());
    knotDates2.push_back(Spot().AddDays(1825));
    Vector_<> fwdRates2;
    fwdRates2.push_back(0.04);
    fwdRates2.push_back(0.04);
    auto curve = DiscountPWLFNew(String_("bootstrapped"), String_("USD"), knotDates2, fwdRates2, base);
    ASSERT_TRUE(curve != nullptr);
}

// ============================================================================
// CurveBlockNew (simple overload)
// ============================================================================

TEST(CurveDataTest, TestCurveBlockNewSimple) {
    Vector_<Date_> knotDates;
    knotDates.push_back(Spot());
    knotDates.push_back(Spot().AddDays(1825));
    Vector_<> fwdRates;
    fwdRates.push_back(0.04);
    fwdRates.push_back(0.04);
    auto dc = DiscountPWLFNew(String_("ois"), String_("USD"), knotDates, fwdRates);

    auto block = CurveBlockNew(dc);
    ASSERT_TRUE(block != nullptr);
}

TEST(CurveDataTest, TestCurveBlockNewSimpleWithBasis) {
    Vector_<Date_> knotDates;
    knotDates.push_back(Spot());
    knotDates.push_back(Spot().AddDays(1825));
    Vector_<> fwdRates;
    fwdRates.push_back(0.04);
    fwdRates.push_back(0.04);
    auto dc = DiscountPWLFNew(String_("ois"), String_("USD"), knotDates, fwdRates);

    auto block = CurveBlockNew(dc, DayBasis_New("ACT_360"));
    ASSERT_TRUE(block != nullptr);
}

// ============================================================================
// CurveBlockNew (full overload)
// ============================================================================

TEST(CurveDataTest, TestCurveBlockNewFull) {
    // Build two discount curves: OIS and Libor 3M
    Vector_<Date_> knotDates;
    knotDates.push_back(Spot());
    knotDates.push_back(Spot().AddDays(1825));
    Vector_<> oisRates;
    oisRates.push_back(0.04);
    oisRates.push_back(0.04);
    auto oisCurve = DiscountPWLFNew(String_("ois"), String_("USD"), knotDates, oisRates);

    Vector_<> liborRates;
    liborRates.push_back(0.045);
    liborRates.push_back(0.045);
    auto liborCurve = DiscountPWLFNew(String_("libor"), String_("USD"), knotDates, liborRates);

    std::map<Dal::CollateralType_, Handle_<Dal::DiscountCurve_>> discounts;
    discounts[CollateralType_OIS()] = oisCurve;

    std::map<Dal::PeriodLength_, Handle_<Dal::DiscountCurve_>> forwards;
    forwards[PeriodLength_New("3M")] = liborCurve;

    auto block = CurveBlockNew(String_("usd"), String_("USD"), discounts, forwards,
                                DayBasis_New("ACT_365F"));
    ASSERT_TRUE(block != nullptr);
}

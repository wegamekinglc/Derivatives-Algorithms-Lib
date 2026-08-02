//
// Created by wegamekinglc on 2026/6/20.
//

#include <gtest/gtest.h>

#include <dal-public/src/curvedata.hpp>
#include <dal-public/src/curveprotocol.hpp>
#include <dal/curve/ycconst.hpp>
#include <dal/curve/yclogdf.hpp>
#include <dal/curve/ycpwlf.hpp>

using Dal::CollateralType_OIS;
using Dal::CurveBlockNew;
using Dal::Date_;
using Dal::DayBasis_New;
using Dal::DiscountLogDFNew;
using Dal::DiscountPWCNew;
using Dal::DiscountPWLFNew;
using Dal::DiscountZeroRate_;
using Dal::DiscountZeroRateNew;
using Dal::Handle_;
using Dal::LogDfScheme_;
using Dal::PeriodLength_New;
using Dal::String_;
using Dal::Vector_;

namespace {

    Date_ Today() { return Date_(2025, 6, 20); }
    Date_ Spot() { return Today().AddDays(2); }

} // namespace

// DiscountPWLFNew

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

TEST(CurveDataTest, TestCompleteCurveFactoriesExposeExactReconstructionState) {
    const Vector_<Date_> knotDates{
        Today().AddDays(30),
        Today().AddDays(365),
        Today().AddDays(730),
        Today().AddDays(1095),
    };
    const Vector_<> right{0.01, 0.02, 0.03, 0.04};
    const Vector_<> left{0.011, 0.021, 0.031, 0.041};
    const auto base = DiscountPWCNew("base", "USD", knotDates, right);

    const auto pwlf = DiscountPWLFNew("pwlf", "USD", knotDates, left, right, base);
    const auto* typedPwlf = dynamic_cast<const Dal::Tape::DiscountPWLF_<double>*>(pwlf.get());
    ASSERT_NE(typedPwlf, nullptr);
    ASSERT_EQ(typedPwlf->KnotDates(), knotDates);
    ASSERT_EQ(typedPwlf->FLeft(), left);
    ASSERT_EQ(typedPwlf->FRight(), right);
    ASSERT_EQ(typedPwlf->Base().get(), base.get());

    const Vector_<Date_> logDates = Dal::Vector::Join(Vector_<Date_>{Today()}, knotDates);
    const Vector_<> logDf{0.0, -0.001, -0.02, -0.05, -0.09};
    Dal::MappedDiscountCurveOptions_ options;
    options.dayCount_ = DayBasis_New("ACT_360");
    options.logDfScheme_ = LogDfScheme_::Value_::MIXED;
    options.base_ = base;
    const auto mapped = DiscountLogDFNew("log", "USD", logDates, logDf, options);
    const auto* typedMapped = dynamic_cast<const Dal::DiscountLogDF_*>(mapped.get());
    ASSERT_NE(typedMapped, nullptr);
    ASSERT_EQ(typedMapped->NodeDates(), logDates);
    ASSERT_EQ(typedMapped->NodeLogDF(), logDf);
    ASSERT_EQ(typedMapped->DayCount().String(), String_("ACT_360"));
    ASSERT_EQ(typedMapped->Scheme(), LogDfScheme_::Value_::MIXED);
    ASSERT_EQ(typedMapped->Base().get(), base.get());
}

// DiscountZeroRateNew

TEST(CurveDataTest, TestDiscountZeroRateNewDefaults) {
    const Vector_<Date_> nodeDates{Today().AddDays(365), Today().AddDays(730), Today().AddDays(1095)};
    const Vector_<> zeroRates{0.02, 0.022, 0.024};

    const auto curve = DiscountZeroRateNew("zero", "USD", Today(), nodeDates, zeroRates);
    const auto* typed = dynamic_cast<const DiscountZeroRate_*>(curve.get());

    ASSERT_NE(typed, nullptr);
    ASSERT_EQ(typed->AnchorDate(), Today());
    ASSERT_EQ(typed->NodeDates(), nodeDates);
    ASSERT_EQ(typed->NodeZeroRates(), zeroRates);
    ASSERT_EQ(typed->DayCount().String(), String_("ACT_365F"));
    ASSERT_EQ(typed->Scheme(), LogDfScheme_::Value_::LOG_LINEAR);
}

TEST(CurveDataTest, TestDiscountZeroRateNewOptionsAndBaseMatchCoreFactory) {
    const Vector_<Date_> nodeDates{Today().AddDays(365), Today().AddDays(730), Today().AddDays(1095)};
    const Vector_<> baseRates{0.01, 0.011, 0.012};
    const Vector_<> spreadRates{0.004, 0.005, 0.006};
    const auto base = DiscountZeroRateNew("base", "USD", Today(), nodeDates, baseRates);
    const Date_ query = Today().AddDays(600);

    for (const auto scheme : {LogDfScheme_::Value_::LOG_LINEAR, LogDfScheme_::Value_::LOG_CUBIC_NATURAL, LogDfScheme_::Value_::MIXED}) {
        const auto curve = DiscountZeroRateNew("spread", "USD", Today(), nodeDates, spreadRates, DayBasis_New("ACT_360"), scheme, base);
        const Handle_<Dal::DiscountCurve_> core(
            Dal::NewDiscountZeroRate("core", "USD", Today(), nodeDates, spreadRates, DayBasis_New("ACT_360"), scheme, base));
        const auto* typed = dynamic_cast<const DiscountZeroRate_*>(curve.get());

        ASSERT_NE(typed, nullptr);
        ASSERT_EQ(typed->DayCount().String(), String_("ACT_360"));
        ASSERT_EQ(typed->Scheme(), scheme);
        ASSERT_NEAR((*curve)(Today(), query), (*core)(Today(), query), 1e-14);
    }
}

// CurveBlockNew (simple overload)

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

// CurveBlockNew (full overload)

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

    auto block = CurveBlockNew(String_("usd"), String_("USD"), discounts, forwards, DayBasis_New("ACT_365F"));
    ASSERT_TRUE(block != nullptr);
    ASSERT_EQ(block->DiscountCurves().size(), 1);
    ASSERT_EQ(block->ForwardCurves().size(), 1);
    ASSERT_EQ(block->LiborBasis().String(), String_("ACT_365F"));
}

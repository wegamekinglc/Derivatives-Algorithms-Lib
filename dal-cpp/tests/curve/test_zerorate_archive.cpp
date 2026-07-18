//
// Created by dal-tester on 2026/7/12.
//

#include <gtest/gtest.h>

#include <cmath>
#include <memory>

#include <dal/curve/yclogdf.hpp>
#include <dal/curve/yczerorate.hpp>
#include <dal/math/aad/aad.hpp>
#include <dal/platform/platform.hpp>
#include <dal/storage/json.hpp>
#include <dal/time/daybasis.hpp>

using namespace Dal;

namespace {
    const Date_ ANCHOR(2024, 1, 15);
    const DayBasis_ ACT_360("ACT_360");
    const Vector_<Date_> NODE_DATES{Date_(2024, 4, 15), Date_(2024, 7, 15), Date_(2025, 1, 15), Date_(2026, 1, 15), Date_(2028, 1, 15)};
    const Vector_<> ZERO_RATES{0.012, 0.015, 0.019, 0.023, 0.027};
    const Vector_<Date_> QUERY_DATES{Date_(2023, 10, 15), ANCHOR, NODE_DATES.front(), Date_(2024, 10, 15), NODE_DATES.back(), Date_(2030, 1, 15)};
    const Vector_<LogDfScheme_> SCHEMES{LogDfScheme_::Value_::LOG_LINEAR, LogDfScheme_::Value_::LOG_CUBIC_NATURAL, LogDfScheme_::Value_::MIXED};

    Handle_<DiscountCurve_> MakeSerializableBase(const String_& name, const Vector_<>& logDf) {
        Vector_<Date_> dates{ANCHOR, Date_(2025, 1, 15), Date_(2027, 1, 15), Date_(2030, 1, 15)};
        return Handle_<DiscountCurve_>(NewDiscountLogDF(name, "USD", dates, logDf, DayBasis_("ACT_365F"), LogDfScheme_::Value_::LOG_LINEAR));
    }

    void AssertFieldsAndValues(const DiscountZeroRate_& actual,
                               const DiscountZeroRate_& expected,
                               const String_& expectedName,
                               LogDfScheme_ expectedScheme) {
        ASSERT_EQ(actual.Name(), expectedName);
        ASSERT_EQ(actual.ccy_.String(), String_("USD"));
        ASSERT_EQ(actual.AnchorDate(), ANCHOR);
        ASSERT_EQ(actual.NodeDates(), NODE_DATES);
        ASSERT_EQ(actual.NodeZeroRates(), expected.NodeZeroRates());
        ASSERT_EQ(actual.DayCount().String(), String_("ACT_360"));
        ASSERT_EQ(actual.Scheme(), expectedScheme);
        ASSERT_EQ(actual.NX(), static_cast<int>(ZERO_RATES.size()));

        for (const auto& from : QUERY_DATES) {
            for (const auto& to : QUERY_DATES)
                ASSERT_NEAR(actual(from, to), expected(from, to), 1.0e-13)
                    << "scheme=" << expectedScheme.String() << " from=" << Date::ToString(from) << " to=" << Date::ToString(to);
        }
    }
} // namespace

TEST(ZeroRateArchiveTest, TestJsonRoundTripPreservesAllSchemesAndZeroRateBumps) {
    const Vector_<> bump{0.001, -0.002, 0.003, -0.004, 0.005};

    for (const auto& scheme : SCHEMES) {
        DiscountZeroRate_ original("zero_archive", "USD", ANCHOR, NODE_DATES, ZERO_RATES, ACT_360, scheme);
        const String_ blob = JSON::WriteString(original);
        ASSERT_FALSE(blob.empty());

        const auto restoredObject = JSON::ReadString(blob, false);
        const auto restored = handle_cast<DiscountZeroRate_>(restoredObject);
        ASSERT_NE(restored, nullptr);
        AssertFieldsAndValues(*restored, original, "zero_archive", scheme);

        std::unique_ptr<DiscountZeroRate_> mutableRestored(dynamic_cast<DiscountZeroRate_*>(restored->Clone("zero_archive", {}).release()));
        ASSERT_NE(mutableRestored, nullptr);
        original.ApplyDX(bump.begin(), 1.0);
        mutableRestored->ApplyDX(bump.begin(), 1.0);
        AssertFieldsAndValues(*mutableRestored, original, "zero_archive", scheme);

        original.ApplyDX(bump.begin(), -1.0);
        mutableRestored->ApplyDX(bump.begin(), -1.0);
        ASSERT_EQ(mutableRestored->NodeZeroRates(), ZERO_RATES);
        AssertFieldsAndValues(*mutableRestored, original, "zero_archive", scheme);
    }
}

TEST(ZeroRateArchiveTest, TestJsonRoundTripAndClonePreserveSerializableBase) {
    const auto base = MakeSerializableBase("base", Vector_<>{0.0, -0.01, -0.03, -0.065});
    const auto replacementBase = MakeSerializableBase("replacement", Vector_<>{0.0, -0.018, -0.04, -0.09});

    for (const auto& scheme : SCHEMES) {
        DiscountZeroRate_ original("zero_with_base", "USD", ANCHOR, NODE_DATES, ZERO_RATES, ACT_360, scheme, base);
        const String_ blob = JSON::WriteString(original);
        const auto restored = handle_cast<DiscountZeroRate_>(JSON::ReadString(blob, false));
        ASSERT_NE(restored, nullptr);
        AssertFieldsAndValues(*restored, original, "zero_with_base", scheme);

        const DiscountZeroRate_ zeroOnly("zero_only", "USD", ANCHOR, NODE_DATES, ZERO_RATES, ACT_360, scheme);
        for (const auto& query : QUERY_DATES)
            ASSERT_NEAR((*restored)(ANCHOR, query), zeroOnly(ANCHOR, query) * (*base)(ANCHOR, query), 1.0e-13);

        YCComponent_::substitutions_t substitutions;
        substitutions[base.get()] = handle_cast<YCComponent_>(replacementBase);
        std::unique_ptr<YCComponent_> cloned(original.Clone("zero_clone", substitutions));
        const auto* clonedZero = dynamic_cast<const DiscountZeroRate_*>(cloned.get());
        ASSERT_NE(clonedZero, nullptr);
        const DiscountZeroRate_ expectedClone("expected", "USD", ANCHOR, NODE_DATES, ZERO_RATES, ACT_360, scheme, replacementBase);
        AssertFieldsAndValues(*clonedZero, expectedClone, "zero_clone", scheme);
    }
}

TEST(ZeroRateArchiveTest, TestActiveAadCurveSerializationIsRejected) {
    auto* tape = AAD::Tape();
    AAD::Clear(*tape);

    Vector_<AAD::Number_> activeZeroRates(ZERO_RATES.size());
    for (int i = 0; i < static_cast<int>(ZERO_RATES.size()); ++i)
        activeZeroRates[i] = ZERO_RATES[i];

    const Tape::DiscountZeroRate_<AAD::Number_> active("active_zero", "USD", ANCHOR, NODE_DATES, activeZeroRates, ACT_360,
                                                       LogDfScheme_::Value_::LOG_LINEAR);
    ASSERT_THROW(JSON::WriteString(active), Dal::Exception_);

    AAD::Clear(*tape);
}

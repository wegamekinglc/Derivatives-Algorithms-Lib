//
// Created by dal-implementer on 2026/7/13.
//

#include <dal/platform/platform.hpp>

#include <gtest/gtest.h>
#include <limits>
#include <dal/curve/xccyinstrument.hpp>
#include <dal/indice/fixingsnapshot.hpp>
#include <dal/storage/globals.hpp>

using namespace Dal;

namespace {
    const DateTime_ kFixing(Date_(2024, 1, 2), 11, 0);

    MarketFixingSnapshot_::values_t SingleValue(const String_& indexName, double value) { return {{indexName, {{kFixing, value}}}}; }
} // namespace

TEST(FixingSnapshotTest, TestCanonicalFxNames) {
    const CurrencyPair_ pair(Ccy_("USD"), Ccy_("EUR"));
    ASSERT_EQ(FxIndexName(Ccy_("USD"), Ccy_("EUR")), "FX[EUR/USD]");
    ASSERT_EQ(FxIndexName(pair), "FX[EUR/USD]");
    ASSERT_EQ(ReverseFxIndexName(pair), "FX[USD/EUR]");
}

TEST(FixingSnapshotTest, TestFindDirectFixing) {
    const MarketFixingSnapshot_ snapshot(SingleValue("FX[EUR/USD]", 1.10));
    const std::optional<double> value = snapshot.Find("FX[EUR/USD]", kFixing);
    ASSERT_TRUE(value.has_value());
    ASSERT_NEAR(*value, 1.10, 1.0e-12);
}

TEST(FixingSnapshotTest, TestFindUsesReverseFxReciprocal) {
    const MarketFixingSnapshot_ snapshot(SingleValue("FX[USD/EUR]", 1.0 / 1.10));
    const std::optional<double> value = snapshot.Find("FX[EUR/USD]", kFixing);
    ASSERT_TRUE(value.has_value());
    ASSERT_NEAR(*value, 1.10, 1.0e-12);
}

TEST(FixingSnapshotTest, TestConsistentTwoWayFxFixingsAreAccepted) {
    const MarketFixingSnapshot_ snapshot({
        {"FX[EUR/USD]", {{kFixing, 1.10}}},
        {"FX[USD/EUR]", {{kFixing, 1.0 / 1.10}}},
    });
    ASSERT_NEAR(snapshot.Require("FX[EUR/USD]", kFixing, "consistent test"), 1.10, 1.0e-12);
}

TEST(FixingSnapshotTest, TestInconsistentTwoWayFxFixingsAreRejected) {
    const MarketFixingSnapshot_::values_t values = {
        {"FX[EUR/USD]", {{kFixing, 1.10}}},
        {"FX[USD/EUR]", {{kFixing, 0.95}}},
    };
    ASSERT_THROW(static_cast<void>(MarketFixingSnapshot_(values)), Dal::Exception_);
}

TEST(FixingSnapshotTest, TestInvalidFixingValuesAreRejected) {
    ASSERT_THROW(static_cast<void>(MarketFixingSnapshot_(SingleValue("USD-SOFR", 0.0))), Dal::Exception_);
    ASSERT_THROW(static_cast<void>(MarketFixingSnapshot_(SingleValue("USD-SOFR", -0.01))), Dal::Exception_);
    ASSERT_THROW(static_cast<void>(MarketFixingSnapshot_(SingleValue("USD-SOFR", std::numeric_limits<double>::infinity()))), Dal::Exception_);
    ASSERT_THROW(static_cast<void>(MarketFixingSnapshot_(SingleValue("USD-SOFR", std::numeric_limits<double>::quiet_NaN()))), Dal::Exception_);
}

TEST(FixingSnapshotTest, TestRequireRejectsMissingFixing) {
    const MarketFixingSnapshot_ snapshot;
    ASSERT_THROW(static_cast<void>(snapshot.Require("USD-SOFR", kFixing, "coupon 3")), Dal::Exception_);
}

TEST(FixingSnapshotTest, TestSnapshotStoresOnlyRequestedTimestamp) {
    const DateTime_ other(Date_(2024, 1, 3), 11, 0);
    FixHistory_ history;
    history.vals_ = {{kFixing, 1.10}, {other, 1.20}};
    XGLOBAL::StoreFixings("FX[EUR/USD]", history, false);

    const auto snapshot = SnapshotGlobalFixings({{"FX[EUR/USD]", kFixing}});
    ASSERT_NEAR(snapshot->Require("FX[EUR/USD]", kFixing, "requested"), 1.10, 1.0e-12);
    ASSERT_FALSE(snapshot->Find("FX[EUR/USD]", other).has_value());
}

TEST(FixingSnapshotTest, TestSnapshotResolvesReverseGlobalFxFixing) {
    FixHistory_ reverse;
    reverse.vals_ = {{kFixing, 1.0 / 1.25}};
    XGLOBAL::StoreFixings("FX[USD/GBP]", reverse, false);

    const auto snapshot = SnapshotGlobalFixings({{"FX[GBP/USD]", kFixing}});
    ASSERT_NEAR(snapshot->Require("FX[GBP/USD]", kFixing, "reverse"), 1.25, 1.0e-12);
}

TEST(FixingSnapshotTest, TestSnapshotDoesNotObserveLaterGlobalMutation) {
    FixHistory_ first;
    first.vals_ = {{kFixing, 1.10}};
    XGLOBAL::StoreFixings("FX[EUR/USD]", first, false);

    const auto snapshot = SnapshotGlobalFixings({{"FX[EUR/USD]", kFixing}});

    FixHistory_ replacement;
    replacement.vals_ = {{kFixing, 1.20}};
    XGLOBAL::StoreFixings("FX[EUR/USD]", replacement, false);
    ASSERT_NEAR(snapshot->Require("FX[EUR/USD]", kFixing, "test"), 1.10, 1.0e-12);
}

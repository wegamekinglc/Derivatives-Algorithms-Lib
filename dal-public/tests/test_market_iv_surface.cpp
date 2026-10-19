#include <gtest/gtest.h>

#include <dal-public/src/marketivsurface.hpp>

using namespace Dal;

TEST(PublicMarketIvSurfaceTest, TestOwningSnapshotAndLegacyBoundary) {
    const Date_ d(2026, 10, 19), expiry(2027, 10, 19);
    const auto market = NewFlatMarketIvMarket(d, 100., -.01, .02, {expiry});
    MarketIvFitRequest_ request{market, {{"atm", expiry, market.Terms()[0].forward_, OptionType_("CALL"), MarketIvQuoteType_("DECIMAL_IV"), .2}}, {}};
    const auto snapshot = FitMarketIvSurface(request).Snapshot();
    request.quotes_.clear();
    const auto ivs = MarketIvSurfaceAsLegacyIVS(snapshot);
    ASSERT_NEAR(ivs->ImpliedVol(100., .7), .2, 1e-10);
    ASSERT_EQ(snapshot.Request().quotes_.size(), 1);
    const MarketIvMarket_ nonflat(d, 100., market.Terms());
    request = {nonflat, snapshot.Request().quotes_, {}};
    const auto curve = FitMarketIvSurface(request).Snapshot();
    ASSERT_THROW((void)MarketIvSurfaceAsLegacyIVS(curve), Exception_);
    const auto point = EvaluateMarketIvSurface(curve, {100., .7});
    ASSERT_NEAR(*point.LocalVol(), .2, 1e-10);
}

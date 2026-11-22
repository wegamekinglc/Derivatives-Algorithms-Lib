#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>

#include "../../dal-public/tests/stagedquoteriskfixture.hpp"
#include <dal-public/src/global.hpp>

using namespace Dal;
using namespace Dal::StagedQuoteRiskTest;

namespace {
    void CheckPortfolio(const RatePortfolioQuoteRisk_& risk) {
        REQUIRE(risk.provenanceFailures_.empty(), "Unexpected source failure");
        REQUIRE(risk.meta_.size() == 2 && risk.meta_[0].eligible_ && risk.meta_[1].eligible_, "Trade risk failed");
        REQUIRE(risk.buckets_.size() == PORTFOLIO_DV01.size(), "Expected four EUR quote buckets");
        std::cout << std::setprecision(std::numeric_limits<double>::max_digits10);
        for (int i = 0; i < 4; ++i) {
            const auto& bucket = risk.buckets_[i];
            const auto key = "stage:" + String::FromInt(i / 2) + ":" + String::FromInt(i % 2);
            REQUIRE(bucket.quoteKey_ == key && bucket.actualPvCcy_ == Ccy_("EUR"), "Unexpected quote key or PV currency");
            REQUIRE(std::abs(bucket.dPvDDecimalQuote_ - PORTFOLIO_DV01[i] * 1.e4) < 1.e-4, "Quote sensitivity differs from full-refit fixture");
            REQUIRE(bucket.dv01_ == bucket.dPvDDecimalQuote_ * 1.e-4, "Incorrect DV01 units");
            std::cout << key << " EUR " << bucket.dPvDDecimalQuote_ << " " << bucket.dv01_ << '\n';
        }
    }
} // namespace

int main() {
    InitGlobalData(1);
    const auto fit = CalibrateMultiCurveBundle(Spec(), true);
    const auto market = Market(fit);
    const auto source = BuildStagedMultiCurveQuoteRiskProvenance(fit, market, CONFIG);
    const Vector_<RateTradeDefinition_> trades{SwapTrade(true), SwapTrade(false)};
    CheckPortfolio(AggregateRatePortfolioQuoteRisk(trades, market, {source}));
    auto stale = market;
    stale.valuationTime_ = DateTime_(TODAY, 10, 1);
    const auto rejected = AggregateRatePortfolioQuoteRisk(trades, stale, {source});
    REQUIRE(rejected.buckets_.empty() && rejected.provenanceFailures_.size() == 1, "Expected one stale-source failure");
    const auto& failure = rejected.provenanceFailures_.front();
    REQUIRE(failure.calibrationId_ == CONFIG.calibrationId_ && failure.reason_ == "QUOTE_RISK_CALIBRATION_STATE_MISMATCH", "Wrong stale-source row");
    const auto plain = CalibrateMultiCurveBundle(Spec(), false);
    const auto unavailable = BuildStagedMultiCurveQuoteRiskProvenance(plain, Market(plain), CONFIG);
    REQUIRE(!plain.riskRecord_ && !unavailable.Available() && unavailable.Reason() == "bundle QUOTE_RISK_STAGED_RECORD_NOT_REQUESTED",
            "Unrecorded calibration must explain risk unavailability");
    return 0;
}

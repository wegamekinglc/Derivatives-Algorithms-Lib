//
// Created by Codex on 2026/10/10.
//

#include <gtest/gtest.h>

#include <future>
#include <limits>

#include <dal/curve/ratetradeobjective_internal.hpp>
#include <dal/indice/detail/fixingobserver.hpp>
#include <dal/platform/platform.hpp>
#include <dal/storage/_repository.hpp>
#include <dal/storage/globals.hpp>

#include "ratejacobianfixtures.hpp"

namespace {
    class TradeFixingScope_ {
        Dal::String_ name_ = "USD-AAD-TRADE-OBJECTIVE";
        Dal::FixHistory_ saved_ = Dal::Global::Fixings_().History(name_);

    public:
        ~TradeFixingScope_() {
            if (saved_.vals_.empty())
                (void)Dal::ObjectAccess_::Erase(Dal::String_("##GLOBAL##FixingsFor:") + name_ + "~");
            else
                Dal::XGLOBAL::StoreFixings(name_, saved_, false);
        }
        [[nodiscard]] const Dal::String_& Name() const { return name_; }
        void Store(const Dal::DateTime_& time, double value) {
            Dal::FixHistory_ history;
            history.vals_.push_back({time, value});
            Dal::XGLOBAL::StoreFixings(name_, history, false);
        }
    };

    struct HistoryReads_ : Dal::Detail::FixingReadObserver_ {
        int histories_ = 0;
        void BeforeHistory(const Dal::String_&) override { ++histories_; }
    };

    Dal::AAD::BumpOverAADRequest_ ObjectiveDirection(int count) {
        Dal::AAD::BumpOverAADRequest_ request;
        request.directions_ = Dal::Matrix_<>(1, count, 0.0);
        for (int column = 0; column < count; ++column)
            request.directions_(0, column) = 1.0 / (column + 1);
        request.steps_ = {1.0e-4};
        return request;
    }
} // namespace

TEST(RateParameterJacobianTest, TestTradeObjectiveCapturesFixingsAndOwners) {
    TradeFixingScope_ globals;
    HistoryReads_ reads;
    Dal::Detail::ScopedFixingReadObserver_ observation(&reads);
    Dal::RateCashflowPricingInternal::RateTradeObjective_ captured;
    double expected = 0.0;
    double discountTime = 0.0;
    Dal::DateTime_ fixing;
    {
        auto market = RateJacobianFixtures::ComponentMarket();
        auto trade = RateJacobianFixtures::Fra();
        const auto today = market.valuationTime_.Date();
        trade.startDate_ = Dal::Date::AddMonths(today, -6);
        trade.tradeDate_ = trade.startDate_;
        trade.maturityDate_ = Dal::Date::AddMonths(today, 6);
        auto& terms = std::get<Dal::FraTradeTerms_>(trade.terms_);
        terms.notional_ = 1.0;
        terms.fixingIdentity_ = {globals.Name(), 10, 30};
        const auto plan = Dal::BuildRateCashflowPlan(trade, market);
        ASSERT_EQ(plan.requiredHistoricalFixings_.size(), 1U);
        fixing = plan.requiredHistoricalFixings_.front().fixingTime_;
        globals.Store(fixing, 0.031);
        Dal::MarketFixingSnapshot_::values_t values;
        values[globals.Name()][fixing] = 0.031;
        auto reference = market;
        reference.fixings_ = Dal::Handle_<Dal::MarketFixingSnapshot_>(new Dal::MarketFixingSnapshot_(values));
        const auto price = Dal::PriceRateTrade(trade, reference);
        ASSERT_TRUE(price.succeeded_) << price.error_;
        expected = price.pv_;
        discountTime = Dal::DayBasis::Act365F()(today, trade.maturityDate_, nullptr);
        Dal::RateCashflowPricingInternal::RateTradeObjectiveSettings_ settings;
        settings.trailingInputs_ = 2;
        settings.fixings_ = Dal::Handle_<Dal::MarketFixingSnapshot_>(new Dal::MarketFixingSnapshot_());
        const Dal::Vector_<Dal::RateCurveParameterCoordinate_> axis{{"A", 0}, {"E", 0}};
        ASSERT_THROW((void)Dal::RateCashflowPricingInternal::NewRateTradeObjective({trade}, market, axis, settings), Dal::Exception_);
        ASSERT_EQ(reads.histories_, 0);
        settings.fixings_.reset();
        captured = Dal::RateCashflowPricingInternal::NewRateTradeObjective({trade}, market, axis, settings);
        ASSERT_EQ(reads.histories_, 1);
        terms.contractRate_ = 0.9;
        trade.maturityDate_ = Dal::Date::AddMonths(today, 48);
        market.curveComponents_.clear();
    }
    globals.Store(fixing, 0.7);
    const Dal::Vector_<> point{0.020, 0.004, 0.025, 0.03};
    const auto result = Dal::AAD::EvaluateBumpOverAAD(captured.objective_, point, ObjectiveDirection(4));
    ASSERT_EQ(captured.currency_, Dal::Ccy_("USD"));
    ASSERT_NEAR(result.Value(), expected, 1.0e-10);
    ASSERT_NEAR(result.Gradient()[0], -discountTime * expected, 1.0e-10);
    for (int column = 1; column < 4; ++column)
        ASSERT_EQ(result.Gradient()[column], 0.0);
    ASSERT_EQ(reads.histories_, 1);
}

TEST(RateParameterJacobianTest, TestTradeObjectiveRejectsMalformedCaptureAndTail) {
    const auto market = RateJacobianFixtures::ComponentMarket();
    const auto trade = RateJacobianFixtures::Deposit("deposit", "A");
    const Dal::Vector_<Dal::RateCurveParameterCoordinate_> axis{{"A", 0}};
    ASSERT_THROW((void)Dal::RateCashflowPricingInternal::NewRateTradeObjective({}, market, axis), Dal::Exception_);
    for (const Dal::Vector_<Dal::RateCurveParameterCoordinate_>& bad :
         {Dal::Vector_<Dal::RateCurveParameterCoordinate_>{{"missing", 0}}, Dal::Vector_<Dal::RateCurveParameterCoordinate_>{{"A", 5}},
          Dal::Vector_<Dal::RateCurveParameterCoordinate_>{{"A", 0}, {"A", 0}}})
        ASSERT_THROW((void)Dal::RateCashflowPricingInternal::NewRateTradeObjective({trade}, market, bad), Dal::Exception_);
    Dal::RateCashflowPricingInternal::RateTradeObjectiveSettings_ settings;
    settings.trailingInputs_ = std::numeric_limits<size_t>::max();
    ASSERT_THROW((void)Dal::RateCashflowPricingInternal::NewRateTradeObjective({trade}, market, axis, settings), Dal::Exception_);
    settings.trailingInputs_ = 1;
    const auto captured = Dal::RateCashflowPricingInternal::NewRateTradeObjective({trade}, market, axis, settings);
    ASSERT_THROW((void)Dal::AAD::EvaluateBumpOverAAD(captured.objective_, {0.02}, ObjectiveDirection(1)), Dal::Exception_);
    const auto result = Dal::AAD::EvaluateBumpOverAAD(captured.objective_, {0.02, 0.03}, ObjectiveDirection(2));
    ASSERT_EQ(result.Gradient()[1], 0.0);
}

TEST(RateParameterJacobianTest, TestTradeObjectiveConcurrentPreparedRequests) {
    const auto market = RateJacobianFixtures::ComponentMarket();
    const Dal::Vector_<Dal::RateTradeDefinition_> trades{RateJacobianFixtures::Deposit("deposit", "A"), RateJacobianFixtures::Irs()};
    const Dal::Vector_<Dal::RateCurveParameterCoordinate_> axis{{"A", 0}, {"B", 0}, {"C", 0}};
    Dal::RateCashflowPricingInternal::RateTradeObjectiveSettings_ settings;
    settings.weights_ = {0.75, -0.5};
    const auto captured = Dal::RateCashflowPricingInternal::NewRateTradeObjective(trades, market, axis, settings);
    const Dal::Vector_<> point{0.020, 0.005, 0.010};
    const auto request = ObjectiveDirection(3);
    const auto expected = Dal::AAD::EvaluateBumpOverAAD(captured.objective_, point, request);
    auto first = std::async(std::launch::async, [&] { return Dal::AAD::EvaluateBumpOverAAD(captured.objective_, point, request); });
    auto second = std::async(std::launch::async, [&] { return Dal::AAD::EvaluateBumpOverAAD(captured.objective_, point, request); });
    for (auto* future : {&first, &second}) {
        const auto actual = future->get();
        ASSERT_EQ(actual.Value(), expected.Value());
        ASSERT_EQ(actual.Gradient(), expected.Gradient());
        for (int column = 0; column < 3; ++column)
            ASSERT_EQ(actual.HessianProducts()(0, column), expected.HessianProducts()(0, column));
    }
}

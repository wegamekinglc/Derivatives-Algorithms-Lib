//
// Created by Codex on 2026/10/09.
//

#include <gtest/gtest.h>

#include <cmath>
#include <future>
#include <utility>

#include <dal/curve/ratecashflowpricing_internal.hpp>
#include <dal/curve/rateparameterjacobian.hpp>
#include <dal/curve/yclogdf.hpp>
#include <dal/curve/ycpwlf.hpp>
#include <dal/curve/yczerorate.hpp>
#include <dal/math/aad/native.hpp>

#include "ratejacobianfixtures.hpp"

using namespace Dal;
using namespace RateJacobianFixtures;

namespace {
    struct GroupedDeposits_ {
        RatePricingMarket_ market_;
        Vector_<RateCurveParameterCoordinate_> axis_;
        Vector_<RateTradeDefinition_> trades_;
    };

    GroupedDeposits_ GroupedDeposits(size_t curves) {
        GroupedDeposits_ result;
        result.market_.valuationTime_ = DateTime_(Date_(2026, 10, 12));
        result.market_.resultCurrency_ = Ccy_("USD");
        for (size_t column = 0; column < curves; ++column) {
            const size_t curve = (3 * column) % curves;
            const String_ key = "K" + String_(std::to_string(curve));
            result.market_.curveComponents_[key] = FlatCurve(key, 0.01 + 0.002 * curve);
            result.axis_.push_back({key, 0});
        }
        for (size_t row = 0; row < 64; ++row) {
            auto trade = Deposit("deposit-" + String_(std::to_string(row)), "K" + String_(std::to_string(row % curves)));
            auto& terms = std::get<DepositTradeTerms_>(trade.terms_);
            terms.notional_ = 100000.0 * (1.0 + row % 7);
            terms.contractRate_ = 0.025 + 0.0001 * (row % 5);
            terms.lend_ = row % 2 == 0;
            result.trades_.push_back(std::move(trade));
        }
        return result;
    }

    void CheckGroupedDeposits(const RateTradeParameterJacobianResult_& result, size_t curves) {
        ASSERT_EQ(result.prices_.size(), 64);
        ASSERT_EQ(result.jacobian_.Rows(), 64);
        ASSERT_EQ(result.jacobian_.Cols(), static_cast<int>(curves));
        for (size_t row = 0; row < 64; ++row) {
            SCOPED_TRACE(row);
            const double rate = 0.01 + 0.002 * (row % curves);
            const double signedNotional = (row % 2 == 0 ? 1.0 : -1.0) * 100000.0 * (1.0 + row % 7);
            const double start = std::exp(-rate / 365.0);
            const double maturity = (1.025 + 0.0001 * (row % 5)) * std::exp(-rate * 366.0 / 365.0);
            const double derivative = signedNotional * (start / 365.0 - maturity * 366.0 / 365.0);
            ASSERT_NEAR(result.prices_[row].pv_, signedNotional * (maturity - start), 1e-8);
            for (size_t column = 0; column < curves; ++column) {
                const double expected = row % curves == (3 * column) % curves ? derivative : 0.0;
                ASSERT_NEAR(result.jacobian_(static_cast<int>(row), static_cast<int>(column)), expected, 1e-8);
            }
        }
    }

    void CheckEquivalentMatrices(const RateTradeParameterJacobianResult_& actual, const RateTradeParameterJacobianResult_& expected) {
        ASSERT_EQ(actual.outputAxis_, expected.outputAxis_);
        ASSERT_EQ(actual.prices_.size(), expected.prices_.size());
        ASSERT_EQ(actual.inputAxis_.size(), expected.inputAxis_.size());
        ASSERT_EQ(actual.jacobian_.Rows(), expected.jacobian_.Rows());
        ASSERT_EQ(actual.jacobian_.Cols(), expected.jacobian_.Cols());
        for (size_t column = 0; column < actual.inputAxis_.size(); ++column) {
            ASSERT_EQ(actual.inputAxis_[column].componentKey_, expected.inputAxis_[column].componentKey_);
            ASSERT_EQ(actual.inputAxis_[column].parameterOrdinal_, expected.inputAxis_[column].parameterOrdinal_);
        }
        for (size_t row = 0; row < actual.prices_.size(); ++row) {
            ASSERT_EQ(actual.prices_[row].currency_, expected.prices_[row].currency_);
            ASSERT_TRUE(actual.prices_[row].succeeded_);
            ASSERT_NEAR(actual.prices_[row].pv_, expected.prices_[row].pv_, 1e-8);
            for (int column = 0; column < actual.jacobian_.Cols(); ++column)
                ASSERT_NEAR(actual.jacobian_(static_cast<int>(row), column), expected.jacobian_(static_cast<int>(row), column), 1e-8);
        }
    }
} // namespace

TEST(RateParameterJacobianTest, TestCachedPlanUsesFreshRecordingAndFallsBackForChangedRowsAxesAndTerms) {
    const auto points = ReferencePoints();
    const auto market = MarketAt(points[2].parameters_);
    const Vector_<RateTradeDefinition_> trades = {Irs(0.0), Deposit("deposit-D", "D"), Fra()};
    const Vector_<RateCurveParameterCoordinate_> axis = {{"C", 0}, {"D", 0}, {"A", 0}, {"E", 0}, {"B", 0}};
    const auto plan = PlanRateStructuralJacobian(CaptureRateStructuralJacobian(trades, MarketAt(points[1].parameters_), axis));
    const auto dense = RateTradeParameterJacobian(trades, market, axis);
    for (const auto [multi, width] : {std::pair{false, size_t{1}}, std::pair{true, size_t{2}}}) {
        const auto reused = RateTradeParameterJacobian(trades, market, axis, plan, {multi, width, {}});
        ASSERT_EQ(reused.reverseDirections_, 2);
        ASSERT_EQ(reused.reverseSweeps_, 2 / width);
        ASSERT_NO_FATAL_FAILURE(CheckEquivalentMatrices(reused, dense));
    }
    auto changed = trades;
    std::get<IrsTradeTerms_>(changed[0].terms_).value_.floatLeg_.paymentLag_ = 3;
    auto duplicated = trades;
    duplicated.push_back(trades[0]);
    for (const auto& currentTrades : {changed, duplicated}) {
        const auto fallback = RateTradeParameterJacobian(currentTrades, market, axis, plan);
        ASSERT_EQ(fallback.reverseDirections_, currentTrades.size());
        ASSERT_EQ(fallback.reverseSweeps_, currentTrades.size());
        ASSERT_NO_FATAL_FAILURE(CheckEquivalentMatrices(fallback, RateTradeParameterJacobian(currentTrades, market, axis)));
    }
    const Vector_<RateCurveParameterCoordinate_> reordered = {axis[4], axis[3], axis[2], axis[1], axis[0]};
    const auto reorderedResult = RateTradeParameterJacobian(trades, market, reordered, plan);
    ASSERT_EQ(reorderedResult.reverseDirections_, 3);
    ASSERT_NO_FATAL_FAILURE(CheckEquivalentMatrices(reorderedResult, RateTradeParameterJacobian(trades, market, reordered)));
}

TEST(RateParameterJacobianTest, TestCachedPlanBudgetAppliesToActualStrategyAndFailureDoesNotPoisonReuse) {
    const auto market = ComponentMarket();
    const Vector_<RateTradeDefinition_> trades = {Irs(0.0), Deposit("deposit-D", "D"), Fra()};
    const Vector_<RateCurveParameterCoordinate_> axis = {{"C", 0}, {"D", 0}, {"A", 0}, {"E", 0}, {"B", 0}};
    const auto plan = PlanRateStructuralJacobian(CaptureRateStructuralJacobian(trades, market, axis));
    const RateJacobianExecutionSettings_ settings{true, 2, 200};
    const auto compressed = RateTradeParameterJacobian(trades, market, axis, plan, settings);
    ASSERT_EQ(compressed.reverseDirections_, 2);
    auto changed = trades;
    std::get<IrsTradeTerms_>(changed[0].terms_).value_.floatLeg_.paymentLag_ = 3;
    ASSERT_THROW(static_cast<void>(RateTradeParameterJacobian(changed, market, axis, plan, settings)), Exception_);
    ASSERT_THROW(static_cast<void>(RateTradeParameterJacobian(trades, market, {{"missing", 0}}, plan)), Exception_);
    ASSERT_THROW(static_cast<void>(RateTradeParameterJacobian(trades, market, axis, plan, {false, 2, {}})), Exception_);
    ASSERT_NO_FATAL_FAILURE(CheckEquivalentMatrices(RateTradeParameterJacobian(trades, market, axis, plan, settings), compressed));
    const auto fallback = RateTradeParameterJacobian(changed, market, axis, plan, {true, 2, 240});
    ASSERT_EQ(fallback.reverseDirections_, 3);
    ASSERT_EQ(fallback.reverseSweeps_, 2);
    ASSERT_NO_FATAL_FAILURE(CheckEquivalentMatrices(fallback, RateTradeParameterJacobian(changed, market, axis)));
}

TEST(RateParameterJacobianTest, TestGroupedOutputRichDepositsAgainstIndependentCashflowDerivatives) {
    for (const size_t curves : {size_t{2}, size_t{8}}) {
        const auto fixture = GroupedDeposits(curves);
        const auto plan = PlanRateStructuralJacobian(CaptureRateStructuralJacobian(fixture.trades_, fixture.market_, fixture.axis_));
        ASSERT_EQ(plan.NumericPlan().ColorCount(), 64 / curves);
        for (const auto [multi, width] : {std::pair{false, size_t{1}}, std::pair{true, size_t{4}}}) {
            const RateJacobianExecutionSettings_ settings{multi, width, {}};
            const auto dense = RateTradeParameterJacobian(fixture.trades_, fixture.market_, fixture.axis_, settings);
            const auto compressed = ExecuteRateStructuralJacobian(fixture.trades_, fixture.market_, plan, settings);
            ASSERT_EQ(dense.reverseDirections_, 64);
            ASSERT_EQ(dense.reverseSweeps_, 64 / width);
            ASSERT_EQ(compressed.reverseDirections_, 64 / curves);
            ASSERT_EQ(compressed.reverseSweeps_, (64 / curves) / width);
            ASSERT_NO_FATAL_FAILURE(CheckGroupedDeposits(dense, curves));
            ASSERT_NO_FATAL_FAILURE(CheckGroupedDeposits(compressed, curves));
        }
    }
}

TEST(RateParameterJacobianTest, TestLayeredCompleteMatricesAgainstFrozenReferenceAcrossNativeWidths) {
    const Vector_<RateCurveParameterCoordinate_> axis = {{"C", 0}, {"D", 0}, {"A", 0}, {"E", 0}, {"B", 0}};
    const auto points = ReferencePoints();
    for (size_t point = 0; point < points.size(); ++point) {
        SCOPED_TRACE(point);
        const auto& oracle = points[point];
        const auto market = MarketAt(oracle.parameters_);
        auto deposit = Deposit("deposit-D", "D");
        std::get<DepositTradeTerms_>(deposit.terms_).notional_ = 500000.0;
        const Vector_<RateTradeDefinition_> trades = {Irs(oracle.swapRate_), deposit, Fra()};
        const auto descriptor = CaptureRateStructuralJacobian(trades, market, axis);
        const auto plan = PlanRateStructuralJacobian(descriptor);
        ASSERT_EQ(plan.NumericPlan().ColorCount(), 2);
        for (const auto [multi, width] :
             {std::pair{false, size_t{1}}, std::pair{true, size_t{1}}, std::pair{true, size_t{2}}, std::pair{true, size_t{4}}}) {
            SCOPED_TRACE(width);
            const RateJacobianExecutionSettings_ settings{multi, width, {}};
            const auto dense = RateTradeParameterJacobian(trades, market, axis, settings);
            const auto compressed = ExecuteRateStructuralJacobian(trades, market, plan, settings);
            ASSERT_EQ(dense.reverseDirections_, 3);
            ASSERT_EQ(compressed.reverseDirections_, 2);
            ASSERT_EQ(dense.reverseSweeps_, (size_t{3} + width - 1) / width);
            ASSERT_EQ(compressed.reverseSweeps_, (size_t{2} + width - 1) / width);
            for (const auto* result : {&dense, &compressed}) {
                ASSERT_EQ(result->jacobian_.Rows(), 3);
                ASSERT_EQ(result->jacobian_.Cols(), 5);
                ASSERT_EQ(result->prices_.size(), 3);
                ASSERT_EQ(result->inputAxis_.size(), 5);
                ASSERT_EQ(result->outputAxis_, descriptor.OutputAxis());
                for (size_t column = 0; column < axis.size(); ++column) {
                    ASSERT_EQ(result->inputAxis_[column].componentKey_, axis[column].componentKey_);
                    ASSERT_EQ(result->inputAxis_[column].parameterOrdinal_, axis[column].parameterOrdinal_);
                }
                for (int row = 0; row < 3; ++row) {
                    SCOPED_TRACE(row);
                    ASSERT_TRUE(result->prices_[row].succeeded_) << result->prices_[row].error_;
                    ASSERT_EQ(result->prices_[row].currency_, Ccy_("USD"));
                    ASSERT_NEAR(result->prices_[row].pv_, oracle.pv_[row], 1e-8);
                    for (int column = 0; column < 5; ++column) {
                        SCOPED_TRACE(column);
                        ASSERT_NEAR(result->jacobian_(row, column), oracle.jacobian_[row][column], 1e-8);
                    }
                }
            }
        }
    }
}

TEST(RateParameterJacobianTest, TestSevenFamiliesAgainstExistingJointNativeMatrix) {
    const auto market = XccyMarket();
    const auto trades = ClosedFamilyTrades();
    const Vector_<RateCurveParameterCoordinate_> axis = {{"C", 0}, {"D", 0}, {"A", 0}, {"E", 0}, {"B", 0}, {"F", 0}};
    const Vector_<String_> keys = {"C", "D", "A", "E", "B", "F"};
    const auto reference = RateCashflowPricingInternal::JointNodeSensitivitiesBatch(trades, market, keys);
    const auto plan = PlanRateStructuralJacobian(CaptureRateStructuralJacobian(trades, market, axis));
    ASSERT_EQ(reference.size(), 42);
    for (const auto [multi, width] : {std::pair{false, size_t{1}}, std::pair{true, size_t{2}}}) {
        const RateJacobianExecutionSettings_ settings{multi, width, {}};
        const auto dense = RateTradeParameterJacobian(trades, market, axis, settings);
        const auto compressed = ExecuteRateStructuralJacobian(trades, market, plan, settings);
        for (int row = 0; row < 7; ++row) {
            SCOPED_TRACE(row);
            ASSERT_NEAR(dense.prices_[row].pv_, compressed.prices_[row].pv_, 1e-8);
            for (int column = 0; column < 6; ++column) {
                SCOPED_TRACE(column);
                const auto& cell = reference[row * 6 + column].result_;
                const double expected = cell.eligible_ ? cell.gradient_[0] : 0.0;
                if (!cell.eligible_) {
                    ASSERT_EQ(cell.reason_, "TRADE_DOES_NOT_DEPEND_ON_COMPONENT");
                }
                ASSERT_NEAR(dense.jacobian_(row, column), expected, 1e-8);
                ASSERT_NEAR(compressed.jacobian_(row, column), expected, 1e-8);
            }
        }
    }
}

TEST(RateParameterJacobianTest, TestActualPvCurrenciesDifferFromReportingLabelWithoutConversion) {
    auto market = XccyMarket();
    auto eur = Deposit("eur", "F");
    eur.currencyOrPair_ = Ccy_("EUR");
    const Vector_<RateTradeDefinition_> trades = {Deposit("usd", "A"), eur, Xccy()};
    const Vector_<RateCurveParameterCoordinate_> axis = {{"A", 0}, {"F", 0}};
    const Vector_<Ccy_> currencies = {Ccy_("USD"), Ccy_("EUR"), Ccy_("USD")};
    const auto reference = RateTradeParameterJacobian(trades, market, axis);
    market.resultCurrency_ = Ccy_("JPY");
    const auto plan = PlanRateStructuralJacobian(CaptureRateStructuralJacobian(trades, market, axis));
    const auto dense = RateTradeParameterJacobian(trades, market, axis);
    const auto compressed = ExecuteRateStructuralJacobian(trades, market, plan);
    for (const auto* result : {&dense, &compressed})
        for (int row = 0; row < 3; ++row) {
            SCOPED_TRACE(row);
            ASSERT_EQ(result->prices_[row].currency_, currencies[row]);
            ASSERT_NEAR(result->prices_[row].pv_, reference.prices_[row].pv_, 1e-8);
            for (int column = 0; column < 2; ++column)
                ASSERT_NEAR(result->jacobian_(row, column), reference.jacobian_(row, column), 1e-8);
        }
}

TEST(RateParameterJacobianTest, TestFreshNumericPointReusesFullPlanAndRejectsChangedTerms) {
    const auto points = ReferencePoints();
    auto deposit = Deposit("deposit-D", "D");
    std::get<DepositTradeTerms_>(deposit.terms_).notional_ = 500000.0;
    const Vector_<RateTradeDefinition_> trades = {Irs(0.0), deposit, Fra()};
    const Vector_<RateCurveParameterCoordinate_> axis = {{"C", 0}, {"D", 0}, {"A", 0}, {"E", 0}, {"B", 0}};
    const auto plan = PlanRateStructuralJacobian(CaptureRateStructuralJacobian(trades, MarketAt(points[1].parameters_), axis));
    const auto zero = ExecuteRateStructuralJacobian(trades, MarketAt(points[1].parameters_), plan);
    ASSERT_NEAR(zero.jacobian_(0, 2), points[1].jacobian_[0][2], 1e-8);
    ASSERT_EQ(plan.Descriptor().RowSupport(0), (Vector_<size_t>{0, 2, 4}));
    const auto fresh = ExecuteRateStructuralJacobian(trades, MarketAt(points[2].parameters_), plan);
    ASSERT_NEAR(fresh.jacobian_(0, 2), points[2].jacobian_[0][2], 1e-8);
    auto changed = trades;
    std::get<IrsTradeTerms_>(changed[0].terms_).value_.floatLeg_.paymentLag_ = 3;
    ASSERT_THROW(static_cast<void>(ExecuteRateStructuralJacobian(changed, MarketAt(points[2].parameters_), plan)), Exception_);
    ASSERT_NO_THROW(static_cast<void>(ExecuteRateStructuralJacobian(trades, MarketAt(points[2].parameters_), plan)));
}

TEST(RateParameterJacobianTest, TestEmptyShapesAndProvenZeroRowsAvoidReverse) {
    const auto market = ComponentMarket();
    const auto empty = RateTradeParameterJacobian({}, market, {});
    ASSERT_EQ(empty.jacobian_.Rows(), 0);
    ASSERT_EQ(empty.jacobian_.Cols(), 0);
    ASSERT_EQ(empty.reverseSweeps_, 0);
    const auto noOutputs = RateTradeParameterJacobian({}, market, {{"A", 0}});
    ASSERT_EQ(noOutputs.jacobian_.Cols(), 1);
    ASSERT_EQ(noOutputs.jacobian_.Rows(), 0);
    ASSERT_EQ(noOutputs.reverseSweeps_, 0);
    const Vector_<RateTradeDefinition_> trades = {Irs()};
    const auto noInputs = RateTradeParameterJacobian(trades, market, {});
    ASSERT_EQ(noInputs.jacobian_.Rows(), 1);
    ASSERT_EQ(noInputs.jacobian_.Cols(), 0);
    ASSERT_EQ(noInputs.reverseSweeps_, 0);
    const auto plan = PlanRateStructuralJacobian(CaptureRateStructuralJacobian(trades, market, {{"D", 0}}));
    const auto zero = ExecuteRateStructuralJacobian(trades, market, plan);
    ASSERT_EQ(zero.reverseDirections_, 0);
    ASSERT_EQ(zero.reverseSweeps_, 0);
    ASSERT_DOUBLE_EQ(zero.jacobian_(0, 0), 0.0);
    ASSERT_NEAR(zero.prices_[0].pv_, -9689.568010118917, 1e-8);
}

TEST(RateParameterJacobianTest, TestRepeatedPositionalRowsAndExpiredConstantsRetainCompleteResults) {
    auto market = ComponentMarket();
    const auto trade = Deposit("same-id", "A");
    const Vector_<RateTradeDefinition_> trades = {trade, trade};
    const Vector_<RateCurveParameterCoordinate_> axis = {{"A", 0}};
    const auto reference = RateTradeParameterJacobian({trade}, market, axis);
    const auto plan = PlanRateStructuralJacobian(CaptureRateStructuralJacobian(trades, market, axis));
    const auto repeated = ExecuteRateStructuralJacobian(trades, market, plan, {true, 2, {}});
    ASSERT_EQ(repeated.jacobian_.Rows(), 2);
    ASSERT_EQ(repeated.jacobian_.Cols(), 1);
    ASSERT_EQ(repeated.outputAxis_, (Vector_<String_>{"same-id", "same-id"}));
    ASSERT_EQ(repeated.reverseDirections_, 2);
    ASSERT_EQ(repeated.reverseSweeps_, 1);
    for (int row = 0; row < 2; ++row) {
        ASSERT_NEAR(repeated.prices_[row].pv_, reference.prices_[0].pv_, 1e-8);
        ASSERT_NEAR(repeated.jacobian_(row, 0), reference.jacobian_(0, 0), 1e-8);
    }
    market.valuationTime_ = DateTime_(Date_(2028, 10, 12));
    ASSERT_THROW(static_cast<void>(ExecuteRateStructuralJacobian(trades, market, plan)), Exception_);
    const auto expiredPlan = PlanRateStructuralJacobian(CaptureRateStructuralJacobian(trades, market, axis));
    const auto expired = ExecuteRateStructuralJacobian(trades, market, expiredPlan, {true, 2, 32});
    ASSERT_EQ(expired.jacobian_.Rows(), 2);
    ASSERT_EQ(expired.jacobian_.Cols(), 1);
    ASSERT_EQ(expired.reverseDirections_, 2);
    ASSERT_EQ(expired.reverseSweeps_, 1);
    for (int row = 0; row < 2; ++row) {
        ASSERT_TRUE(expired.prices_[row].succeeded_);
        ASSERT_DOUBLE_EQ(expired.prices_[row].pv_, 0.0);
        ASSERT_DOUBLE_EQ(expired.jacobian_(row, 0), 0.0);
    }
}

TEST(RateParameterJacobianTest, TestBudgetInvalidAxesAndFailureCleanupRestoreCallerMode) {
    const auto market = ComponentMarket();
    const Vector_<RateTradeDefinition_> trades = {Deposit("deposit-A", "A")};
    const Vector_<RateCurveParameterCoordinate_> axis = {{"A", 0}};
    const auto plan = PlanRateStructuralJacobian(CaptureRateStructuralJacobian(trades, market, axis));
    const auto mode = AAD::SetNumResultsForAAD(true, 2);
    ASSERT_THROW(static_cast<void>(RateTradeParameterJacobian(trades, market, axis, {false, 1, 15})), Exception_);
    ASSERT_THROW(static_cast<void>(ExecuteRateStructuralJacobian(trades, market, plan, {false, 1, 15})), Exception_);
    ASSERT_NO_THROW(static_cast<void>(RateTradeParameterJacobian(trades, market, axis, {false, 1, 16})));
    ASSERT_THROW(static_cast<void>(RateTradeParameterJacobian(trades, market, {{"missing", 0}})), Exception_);
    ASSERT_THROW(static_cast<void>(RateTradeParameterJacobian(trades, market, {{"A", 1}})), Exception_);
    auto alias = market;
    alias.curveComponents_["alias"] = market.curveComponents_.at("A");
    ASSERT_THROW(static_cast<void>(RateTradeParameterJacobian(trades, alias, {{"A", 0}, {"alias", 0}})), Exception_);
    ASSERT_THROW(static_cast<void>(RateTradeParameterJacobian(trades, market, axis, {false, 2, {}})), Exception_);
    auto failing = trades;
    std::get<DepositTradeTerms_>(failing[0].terms_).notional_ = -1.0;
    ASSERT_THROW(static_cast<void>(RateTradeParameterJacobian(failing, market, axis)), Exception_);
    auto overflow = trades;
    overflow[0].maturityDate_ = Date_(2029, 10, 13);
    auto& terms = std::get<DepositTradeTerms_>(overflow[0].terms_);
    terms.notional_ = 1e308;
    terms.contractRate_ = 0.0;
    auto zeroMarket = market;
    zeroMarket.curveComponents_["A"] = FlatCurve("A", 0.0);
    ASSERT_TRUE(PriceRateTrade(overflow[0], zeroMarket).succeeded_);
    ASSERT_THROW(static_cast<void>(RateTradeParameterJacobian(overflow, zeroMarket, axis)), Exception_);
    ASSERT_NO_THROW(static_cast<void>(ExecuteRateStructuralJacobian(trades, market, plan)));
    ASSERT_TRUE(AAD::Tape()->multi_);
    ASSERT_EQ(AAD::Tape()->numAdj_, 2);
}

TEST(RateParameterJacobianTest, TestIndependentConcurrentRequestsAndNestedScopeRejection) {
    const auto market = ComponentMarket();
    const Vector_<RateTradeDefinition_> trades = {Deposit("deposit-A", "A")};
    const Vector_<RateCurveParameterCoordinate_> axis = {{"A", 0}};
    const auto plan = PlanRateStructuralJacobian(CaptureRateStructuralJacobian(trades, market, axis));
    const auto expected = ExecuteRateStructuralJacobian(trades, market, plan);
    std::array<std::future<double>, 4> futures;
    for (auto& future : futures)
        future = std::async(std::launch::async, [&] { return ExecuteRateStructuralJacobian(trades, market, plan).jacobian_(0, 0); });
    for (auto& future : futures)
        ASSERT_NEAR(future.get(), expected.jacobian_(0, 0), 1e-8);
    AAD::RecordingScope_ recording;
    AAD::Number_ input;
    recording.RegisterInput(input, 2.0);
    recording.StartRecording();
    const auto nodes = AAD::Tape()->nodes_.OccupiedSlots();
    ASSERT_THROW(static_cast<void>(RateTradeParameterJacobian(trades, market, axis)), Exception_);
    ASSERT_EQ(AAD::Tape()->nodes_.OccupiedSlots(), nodes);
    const AAD::Number_ output = input * input;
    recording.FinishRecording();
    recording.ClearAdjoints();
    auto seed = output;
    AAD::NativeOperations_::SetSeed(seed, 1.0);
    recording.Reverse();
    ASSERT_DOUBLE_EQ(AAD::AdjointValue(input), 4.0);
}

TEST(RateParameterJacobianTest, TestAllCurveFamiliesAndInterleavedFreeParameterOrdinals) {
    auto market = ComponentMarket();
    const Vector_<Date_> dates = {Date_(2026, 10, 12), Date_(2028, 10, 13)};
    market.curveComponents_["L"] =
        Handle_<DiscountCurve_>(new Tape::DiscountLogDF_<double>("L", "USD", dates, {0.0, -0.04}, DayBasis_("ACT_365F"), LogDfScheme_("LOG_LINEAR")));
    market.curveComponents_["Z"] = Handle_<DiscountCurve_>(
        new Tape::DiscountZeroRate_<double>("Z", "USD", dates[0], {dates[1]}, {0.02}, DayBasis_("ACT_365F"), LogDfScheme_("LOG_LINEAR")));
    market.curveComponents_["W"] = Handle_<DiscountCurve_>(new Tape::DiscountPWLF_<double>("W", "USD", {dates[1]}, {0.01}, {0.02}));
    const Vector_<RateTradeDefinition_> trades = {Deposit("logdf", "L"), Deposit("zero-rate", "Z"), Deposit("pwlf", "W"), Deposit("pwc", "D")};
    const Vector_<RateCurveParameterCoordinate_> axis = {{"W", 1}, {"L", 0}, {"D", 0}, {"W", 0}, {"Z", 0}};
    const Vector_<String_> keys = {"W", "L", "D", "W", "Z"};
    const auto reference = RateCashflowPricingInternal::JointNodeSensitivitiesBatch(trades, market, keys);
    const auto plan = PlanRateStructuralJacobian(CaptureRateStructuralJacobian(trades, market, axis));
    ASSERT_EQ(plan.NumericPlan().ColorCount(), 1);
    const auto dense = RateTradeParameterJacobian(trades, market, axis);
    const auto compressed = ExecuteRateStructuralJacobian(trades, market, plan, {true, 2, {}});
    for (int row = 0; row < 4; ++row)
        for (int column = 0; column < 5; ++column) {
            SCOPED_TRACE(row);
            SCOPED_TRACE(column);
            const auto& cell = reference[row * 5 + column].result_;
            if (!cell.eligible_) {
                ASSERT_EQ(cell.reason_, "TRADE_DOES_NOT_DEPEND_ON_COMPONENT");
            }
            const auto expected = cell.eligible_ ? cell.gradient_[axis[column].parameterOrdinal_] : 0.0;
            ASSERT_NEAR(dense.jacobian_(row, column), expected, 1e-8);
            ASSERT_NEAR(compressed.jacobian_(row, column), expected, 1e-8);
        }
}

TEST(RateParameterJacobianTest, TestSevenFamiliesAgainstIndependentPassiveDifferencesWithBaseRebuild) {
    const auto trades = ClosedFamilyTrades();
    const Vector_<RateCurveParameterCoordinate_> axis = {{"C", 0}, {"D", 0}, {"A", 0}, {"E", 0}, {"B", 0}, {"F", 0}};
    const std::array<double, 6> values = {0.010, 0.030, 0.020, 0.004, 0.005, 0.010};
    const auto marketAt = [](const std::array<double, 6>& rates) {
        auto market = MarketAt({rates[0], rates[1], rates[2], rates[3], rates[4]});
        market.curveComponents_["F"] = Handle_<DiscountCurve_>(NewDiscountPWC("F", "EUR", PiecewiseConstant_({Date_(2028, 10, 13)}, {rates[5]})));
        const auto domestic = Handle_<CurveBlock_>(new CurveBlock_(market.curveComponents_.at("A")));
        const auto foreign = Handle_<CurveBlock_>(new CurveBlock_(market.curveComponents_.at("F")));
        auto native = std::make_shared<CrossCurrencyMarket_>(domestic, foreign, 1.2, market.valuationTime_, Ccy_("USD"));
        native->SetBasisCurve(market.curveComponents_.at("B"));
        market.xccyMarket_ = native;
        return market;
    };
    const auto market = marketAt(values);
    const auto plan = PlanRateStructuralJacobian(CaptureRateStructuralJacobian(trades, market, axis));
    const auto result = ExecuteRateStructuralJacobian(trades, market, plan);
    for (const double step : {1e-5, 5e-6})
        for (int column = 0; column < 6; ++column) {
            SCOPED_TRACE(step);
            SCOPED_TRACE(column);
            auto up = values, down = values;
            up[column] += step;
            down[column] -= step;
            const auto high = PriceRateTrades(trades, marketAt(up));
            const auto low = PriceRateTrades(trades, marketAt(down));
            for (int row = 0; row < 7; ++row) {
                SCOPED_TRACE(row);
                ASSERT_TRUE(high[row].succeeded_) << high[row].error_;
                ASSERT_TRUE(low[row].succeeded_) << low[row].error_;
                const double derivative = (high[row].pv_ - low[row].pv_) / (2.0 * step);
                ASSERT_NEAR(result.jacobian_(row, column) / 1e6, derivative / 1e6, 1e-8);
            }
        }
}

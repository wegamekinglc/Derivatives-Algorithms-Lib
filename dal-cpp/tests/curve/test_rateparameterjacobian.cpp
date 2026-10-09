//
// Created by Codex on 2026/10/09.
//

#include <gtest/gtest.h>

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

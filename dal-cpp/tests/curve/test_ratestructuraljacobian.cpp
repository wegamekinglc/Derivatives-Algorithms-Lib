//
// Created by Codex on 2026/10/09.
//

#include <gtest/gtest.h>

#include <array>
#include <future>

#include <dal/curve/calibration_internal.hpp>
#include <dal/curve/curveblock.hpp>
#include <dal/curve/piecewiseconstant.hpp>
#include <dal/curve/ratecashflowpricing_internal.hpp>
#include <dal/curve/ratestructuraljacobian.hpp>
#include <dal/curve/xccypricing.hpp>
#include <dal/curve/ycconst.hpp>
#include <dal/curve/ycpwlf.hpp>

#include "ratejacobianfixtures.hpp"

using namespace Dal;

using namespace RateJacobianFixtures;

TEST(RateStructuralJacobianTest, TestIndependentComponentBlocksWithInterleavedAxisAndBaseClosure) {
    const auto market = ComponentMarket();
    const Vector_<RateTradeDefinition_> trades = {Deposit("deposit-A", "A"), Deposit("deposit-D", "D"), Deposit("deposit-C", "C")};
    const Vector_<RateCurveParameterCoordinate_> axis = {{"C", 0}, {"D", 0}, {"A", 0}, {"E", 0}, {"B", 0}};
    const auto descriptor = CaptureRateStructuralJacobian(trades, market, axis);
    ASSERT_TRUE(descriptor.Available());
    ASSERT_TRUE(descriptor.Reason().empty());
    ASSERT_EQ(descriptor.Inputs(), 5);
    ASSERT_EQ(descriptor.Outputs(), 3);
    ASSERT_EQ(descriptor.RowSupport(0), (Vector_<size_t>{2}));
    ASSERT_EQ(descriptor.RowSupport(1), (Vector_<size_t>{1}));
    ASSERT_EQ(descriptor.RowSupport(2), (Vector_<size_t>{0, 4}));
    const auto plan = PlanRateStructuralJacobian(descriptor);
    ASSERT_EQ(plan.NumericPlan().ColorCount(), 1);
    ASSERT_EQ(plan.NumericPlan().Inputs(), 5);
    ASSERT_EQ(plan.NumericPlan().Outputs(), 3);
    ASSERT_TRUE(SameRateStructuralJacobianStructure(plan.Descriptor(), descriptor));
}

TEST(RateStructuralJacobianTest, TestCompleteIdentityRejectsSameShapeChangesAndAllowsFreshNumericPoint) {
    const auto market = ComponentMarket();
    const Vector_<RateTradeDefinition_> original = {Deposit("deposit-A", "A")};
    const Vector_<RateCurveParameterCoordinate_> axis = {{"A", 0}};
    const auto baseline = CaptureRateStructuralJacobian(original, market, axis);
    {
        auto trades = original;
        std::get<DepositTradeTerms_>(trades[0].terms_).contractRate_ = 0.026;
        const auto changed = CaptureRateStructuralJacobian(trades, market, axis);
        ASSERT_EQ(changed.RowSupport(0), baseline.RowSupport(0));
        ASSERT_FALSE(SameRateStructuralJacobianStructure(baseline, changed));
    }
    {
        auto trades = original;
        trades[0].maturityDate_ = trades[0].maturityDate_.AddDays(1);
        ASSERT_FALSE(SameRateStructuralJacobianStructure(baseline, CaptureRateStructuralJacobian(trades, market, axis)));
    }
    {
        auto changed = market;
        changed.valuationTime_ = DateTime_(market.valuationTime_.Date(), 12);
        ASSERT_FALSE(SameRateStructuralJacobianStructure(baseline, CaptureRateStructuralJacobian(original, changed, axis)));
    }
    {
        auto changed = market;
        changed.curveComponents_["A"] = Handle_<DiscountCurve_>(NewDiscountPWC("A", "USD", PiecewiseConstant_({Date_(2028, 10, 14)}, {0.020})));
        ASSERT_FALSE(SameRateStructuralJacobianStructure(baseline, CaptureRateStructuralJacobian(original, changed, axis)));
    }
    {
        auto changed = market;
        changed.curveComponents_["A"] = Handle_<DiscountCurve_>(new Tape::DiscountPWLF_<double>("A", "USD", {Date_(2028, 10, 13)}, {0.020}, {0.020}));
        ASSERT_FALSE(SameRateStructuralJacobianStructure(baseline, CaptureRateStructuralJacobian(original, changed, axis)));
    }
    {
        auto changed = market;
        changed.curveComponents_["A"] = FlatCurve("A", 0.031);
        const auto fresh = CaptureRateStructuralJacobian(original, changed, axis);
        ASSERT_TRUE(SameRateStructuralJacobianStructure(baseline, fresh));
        ASSERT_EQ(fresh.RowSupport(0), baseline.RowSupport(0));
    }
}

TEST(RateStructuralJacobianTest, TestFixingPresenceAtValuationInvalidatesEvenWithoutHistoricalRequest) {
    auto market = ComponentMarket();
    const auto trade = Fra();
    market.valuationTime_ = DateTime_(trade.startDate_, 10, 30);
    const Vector_<RateTradeDefinition_> trades = {trade};
    const Vector_<RateCurveParameterCoordinate_> axis = {{"E", 0}, {"A", 0}};
    ASSERT_TRUE(BuildRateCashflowPlan(trade, market).requiredHistoricalFixings_.empty());
    const auto projected = CaptureRateStructuralJacobian(trades, market, axis);
    market.fixings_ = Handle_<MarketFixingSnapshot_>(new MarketFixingSnapshot_({{"USD-12M", {{market.valuationTime_, 0.024}}}}));
    const auto fixed = CaptureRateStructuralJacobian(trades, market, axis);
    ASSERT_EQ(projected.RowSupport(0), fixed.RowSupport(0));
    ASSERT_FALSE(SameRateStructuralJacobianStructure(projected, fixed));
}

TEST(RateStructuralJacobianTest, TestOrderedMetadataSurvivesMarketAndTradeCleanup) {
    const auto descriptor = [] {
        auto market = ComponentMarket();
        Vector_<RateTradeDefinition_> trades = {Deposit("deposit-C", "C"), Deposit("deposit-D", "D")};
        Vector_<RateCurveParameterCoordinate_> axis = {{"B", 0}, {"C", 0}, {"D", 0}};
        return CaptureRateStructuralJacobian(trades, market, axis);
    }();
    const auto copy = descriptor;
    ASSERT_EQ(copy.InputAxis().size(), 3);
    ASSERT_EQ(copy.InputAxis()[0].componentKey_, "B");
    ASSERT_EQ(copy.InputAxis()[1].componentKey_, "C");
    ASSERT_EQ(copy.InputAxis()[1].parameterOrdinal_, 0);
    ASSERT_EQ(copy.OutputAxis(), (Vector_<String_>{"deposit-C", "deposit-D"}));
    ASSERT_EQ(copy.RowSupport(0), (Vector_<size_t>{0, 1}));
    ASSERT_EQ(copy.RowSupport(1), (Vector_<size_t>{2}));
    ASSERT_TRUE(SameRateStructuralJacobianStructure(descriptor, copy));
}

TEST(RateStructuralJacobianTest, TestLayeredFinancialSupportsAgainstAnalyticFullMatrixAndDenseNativeReference) {
    const auto points = ReferencePoints();
    const Vector_<RateCurveParameterCoordinate_> axis = {{"C", 0}, {"D", 0}, {"A", 0}, {"E", 0}, {"B", 0}};
    const Vector_<String_> keys = {"C", "D", "A", "E", "B"};
    const Vector_<Vector_<size_t>> expectedSupports = {{0, 2, 4}, {1}, {2, 3}};
    std::optional<RateStructuralJacobianDescriptor_> zeroPoint;
    for (size_t point = 0; point < points.size(); ++point) {
        SCOPED_TRACE(point);
        const auto& oracle = points[point];
        const auto market = MarketAt(oracle.parameters_);
        auto deposit = Deposit("deposit-D", "D");
        std::get<DepositTradeTerms_>(deposit.terms_).notional_ = 500000.0;
        const Vector_<RateTradeDefinition_> trades = {Irs(oracle.swapRate_), deposit, Fra()};
        const auto& swap = std::get<IrsTradeTerms_>(trades[0].terms_).value_;
        const auto periods = BuildLegPeriods<XccyCouponPeriod_>(trades[0].startDate_, trades[0].maturityDate_, swap.floatLeg_,
                                                                swap.floatIndex_.fixingLag_, swap.floatIndex_.fixingHolidays_);
        ASSERT_EQ(periods.size(), 1);
        ASSERT_EQ(periods[0].schedule_.accrualStart_, Date_(2026, 10, 13));
        ASSERT_EQ(periods[0].schedule_.accrualEnd_, Date_(2027, 10, 13));
        ASSERT_EQ(periods[0].schedule_.paymentDate_, Date_(2027, 10, 15));
        ASSERT_DOUBLE_EQ(periods[0].accrual_.dcf_, 1.0);
        const auto descriptor = CaptureRateStructuralJacobian(trades, market, axis);
        ASSERT_TRUE(descriptor.Available());
        ASSERT_EQ(PlanRateStructuralJacobian(descriptor).NumericPlan().ColorCount(), 2);
        const auto passive = PriceRateTrades(trades, market);
        const auto dense = RateCashflowPricingInternal::JointNodeSensitivitiesBatch(trades, market, keys);
        ASSERT_EQ(passive.size(), 3);
        ASSERT_EQ(dense.size(), 15);
        for (size_t row = 0; row < trades.size(); ++row) {
            SCOPED_TRACE(row);
            ASSERT_EQ(descriptor.RowSupport(row), expectedSupports[row]);
            ASSERT_TRUE(passive[row].succeeded_) << passive[row].error_;
            ASSERT_NEAR(passive[row].pv_, oracle.pv_[row], 1e-8);
            for (size_t column = 0; column < axis.size(); ++column) {
                SCOPED_TRACE(column);
                const auto& result = dense[row * axis.size() + column].result_;
                if (result.eligible_) {
                    ASSERT_EQ(result.gradient_.size(), 1);
                    ASSERT_NEAR(result.pv_, oracle.pv_[row], 1e-8);
                    ASSERT_NEAR(result.gradient_[0], oracle.jacobian_[row][column], 1e-8);
                } else {
                    ASSERT_EQ(result.reason_, "TRADE_DOES_NOT_DEPEND_ON_COMPONENT");
                    ASSERT_DOUBLE_EQ(oracle.jacobian_[row][column], 0.0);
                }
            }
        }
        if (point == 1)
            zeroPoint = descriptor;
        if (point == 2) {
            ASSERT_TRUE(SameRateStructuralJacobianStructure(*zeroPoint, descriptor));
        }
    }
}

TEST(RateStructuralJacobianTest, TestUnregisteredConsumedXccyCurveCannotBecomeAnAvailableZeroRow) {
    auto market = XccyMarket();
    const Vector_<RateTradeDefinition_> trades = {Xccy()};
    const Vector_<RateCurveParameterCoordinate_> axis = {{"D", 0}};
    const auto available = CaptureRateStructuralJacobian(trades, market, axis);
    ASSERT_TRUE(available.Available());
    ASSERT_TRUE(available.RowSupport(0).empty());
    market.curveComponents_.erase("A");
    const auto unregistered = CaptureRateStructuralJacobian(trades, market, axis);
    ASSERT_FALSE(unregistered.Available());
    ASSERT_EQ(unregistered.Reason(), "RATE_STRUCTURAL_UNREGISTERED_CONSUMED_CURVE");
    ASSERT_THROW(static_cast<void>(unregistered.RowSupport(0)), Exception_);
    ASSERT_THROW(static_cast<void>(PlanRateStructuralJacobian(unregistered)), Exception_);
    ASSERT_FALSE(SameRateStructuralJacobianStructure(available, unregistered));
}

TEST(RateStructuralJacobianTest, TestInvalidAxisAndPhysicalAliasesAreRejected) {
    auto market = ComponentMarket();
    const Vector_<RateTradeDefinition_> trades = {Deposit("deposit-A", "A")};
    ASSERT_THROW(static_cast<void>(CaptureRateStructuralJacobian(trades, market, {{"missing", 0}})), Exception_);
    ASSERT_THROW(static_cast<void>(CaptureRateStructuralJacobian(trades, market, {{"A", 1}})), Exception_);
    market.curveComponents_["alias-A"] = market.curveComponents_.at("A");
    ASSERT_THROW(static_cast<void>(CaptureRateStructuralJacobian(trades, market, {{"A", 0}, {"alias-A", 0}})), Exception_);
    const auto alias = CaptureRateStructuralJacobian({Deposit("alias-deposit", "alias-A")}, market, {{"A", 0}});
    ASSERT_TRUE(alias.Available());
    ASSERT_EQ(alias.RowSupport(0), (Vector_<size_t>{0}));
    ASSERT_THROW(static_cast<void>(alias.RowSupport(1)), Exception_);
}

TEST(RateStructuralJacobianTest, TestUnknownAndIncompleteGraphsCannotProveEmptySupport) {
    const Vector_<RateTradeDefinition_> trades = {Deposit("deposit-C", "C")};
    {
        auto market = ComponentMarket();
        market.curveComponents_.erase("C");
        const auto descriptor = CaptureRateStructuralJacobian(trades, market, {{"D", 0}});
        ASSERT_FALSE(descriptor.Available());
        ASSERT_EQ(descriptor.Reason(), "RATE_STRUCTURAL_INCOMPLETE_CURVE_GRAPH");
    }
    {
        auto market = ComponentMarket();
        market.curveComponents_["C"] = Handle_<DiscountCurve_>(new DerivedCurve_("custom", "USD", {Date_(2028, 10, 13)}, {0.010}));
        const auto descriptor = CaptureRateStructuralJacobian(trades, market, {{"D", 0}});
        ASSERT_FALSE(descriptor.Available());
        ASSERT_EQ(descriptor.Reason(), "RATE_STRUCTURAL_INCOMPLETE_CURVE_GRAPH");
        const auto unsupportedInput = CaptureRateStructuralJacobian({}, market, {{"C", 0}});
        ASSERT_FALSE(unsupportedInput.Available());
        ASSERT_EQ(unsupportedInput.Reason(), "RATE_STRUCTURAL_UNSUPPORTED_INPUT_CURVE");
        ASSERT_EQ(unsupportedInput.InputAxis()[0].componentKey_, "C");
    }
    {
        auto market = ComponentMarket();
        const auto custom = Handle_<DiscountCurve_>(new DerivedCurve_("custom-base", "USD", {Date_(2028, 10, 13)}, {0.010}));
        market.curveComponents_["C"] = FlatCurve("C", 0.010, custom);
        const auto descriptor = CaptureRateStructuralJacobian(trades, market, {{"D", 0}});
        ASSERT_FALSE(descriptor.Available());
        const auto unrelatedInput = CaptureRateStructuralJacobian({}, market, {{"C", 0}});
        ASSERT_FALSE(unrelatedInput.Available());
        ASSERT_EQ(unrelatedInput.Reason(), "RATE_STRUCTURAL_INCOMPLETE_INPUT_GRAPH");
    }
}

TEST(RateStructuralJacobianTest, TestAxisOutputBaseAndAliasChangesInvalidateSameSizedPlans) {
    auto market = ComponentMarket();
    const Vector_<RateTradeDefinition_> trades = {Deposit("deposit-E", "E"), Deposit("deposit-D", "D")};
    const Vector_<RateCurveParameterCoordinate_> axis = {{"E", 0}, {"D", 0}};
    const auto baseline = CaptureRateStructuralJacobian(trades, market, axis);
    ASSERT_FALSE(SameRateStructuralJacobianStructure(baseline, CaptureRateStructuralJacobian(trades, market, {{"D", 0}, {"E", 0}})));
    ASSERT_FALSE(SameRateStructuralJacobianStructure(baseline, CaptureRateStructuralJacobian({trades[1], trades[0]}, market, axis)));
    {
        auto changed = market;
        changed.curveComponents_["E"] = FlatCurve("E", 0.004, changed.curveComponents_.at("B"));
        const auto descriptor = CaptureRateStructuralJacobian(trades, changed, axis);
        ASSERT_EQ(descriptor.RowSupport(0), baseline.RowSupport(0));
        ASSERT_FALSE(SameRateStructuralJacobianStructure(baseline, descriptor));
    }
    {
        market.curveComponents_["alias-E"] = market.curveComponents_.at("E");
        const auto aliased = CaptureRateStructuralJacobian(trades, market, axis);
        market.curveComponents_["alias-E"] = FlatCurve("E", 0.004, market.curveComponents_.at("A"));
        ASSERT_FALSE(SameRateStructuralJacobianStructure(aliased, CaptureRateStructuralJacobian(trades, market, axis)));
    }
}

TEST(RateStructuralJacobianTest, TestPaymentFixingAndValuationChangesInvalidateFinancialIdentity) {
    const auto market = ComponentMarket();
    const Vector_<RateTradeDefinition_> trades = {Irs()};
    const Vector_<RateCurveParameterCoordinate_> axis = {{"C", 0}, {"A", 0}, {"B", 0}};
    const auto baseline = CaptureRateStructuralJacobian(trades, market, axis);
    {
        auto changed = trades;
        std::get<IrsTradeTerms_>(changed[0].terms_).value_.floatLeg_.paymentLag_ = 3;
        ASSERT_FALSE(SameRateStructuralJacobianStructure(baseline, CaptureRateStructuralJacobian(changed, market, axis)));
    }
    {
        auto changed = trades;
        std::get<IrsTradeTerms_>(changed[0].terms_).value_.fixingIdentity_.fixingMinute_ = 31;
        ASSERT_FALSE(SameRateStructuralJacobianStructure(baseline, CaptureRateStructuralJacobian(changed, market, axis)));
    }
    {
        auto changed = market;
        changed.valuationTime_ = DateTime_(Date_(2027, 10, 16));
        const auto expired = CaptureRateStructuralJacobian(trades, changed, axis);
        ASSERT_EQ(expired.RowSupport(0), baseline.RowSupport(0));
        ASSERT_FALSE(SameRateStructuralJacobianStructure(baseline, expired));
    }
}

TEST(RateStructuralJacobianTest, TestEmptyShapesAndIndependentConcurrentCaptures) {
    const auto market = ComponentMarket();
    const auto empty = CaptureRateStructuralJacobian({}, market, {});
    ASSERT_TRUE(empty.Available());
    ASSERT_EQ(empty.Inputs(), 0);
    ASSERT_EQ(empty.Outputs(), 0);
    ASSERT_EQ(PlanRateStructuralJacobian(empty).NumericPlan().ColorCount(), 0);
    const auto noInputs = CaptureRateStructuralJacobian({Deposit("deposit-A", "A")}, market, {});
    ASSERT_EQ(noInputs.Outputs(), 1);
    ASSERT_TRUE(noInputs.RowSupport(0).empty());
    const auto noOutputs = CaptureRateStructuralJacobian({}, market, {{"A", 0}});
    ASSERT_EQ(noOutputs.Inputs(), 1);
    ASSERT_EQ(noOutputs.Outputs(), 0);
    const Vector_<RateTradeDefinition_> trades = {Irs(), Fra()};
    const Vector_<RateCurveParameterCoordinate_> axis = {{"A", 0}, {"B", 0}, {"C", 0}, {"E", 0}};
    const auto baseline = CaptureRateStructuralJacobian(trades, market, axis);
    std::array<std::future<bool>, 4> requests;
    for (auto& request : requests)
        request = std::async(std::launch::async, [&] {
            const auto current = CaptureRateStructuralJacobian(trades, ComponentMarket(), axis);
            const auto plan = PlanRateStructuralJacobian(current);
            return SameRateStructuralJacobianStructure(baseline, plan.Descriptor()) && plan.NumericPlan().ColorCount() == 2;
        });
    for (auto& request : requests)
        ASSERT_TRUE(request.get());
}

TEST(RateStructuralJacobianTest, TestAllClosedFamiliesRetainFullConsumedCurveBlocks) {
    const auto market = XccyMarket();
    const auto trades = ClosedFamilyTrades();
    const Vector_<RateCurveParameterCoordinate_> axis = {{"C", 0}, {"D", 0}, {"A", 0}, {"E", 0}, {"B", 0}, {"F", 0}};
    const Vector_<Vector_<size_t>> expected = {{1}, {2, 3}, {2, 3}, {0, 2, 4}, {0, 2, 4}, {0, 2, 3, 4}, {2, 4, 5}};
    const auto descriptor = CaptureRateStructuralJacobian(trades, market, axis);
    ASSERT_TRUE(descriptor.Available());
    const auto passive = PriceRateTrades(trades, market);
    for (size_t row = 0; row < trades.size(); ++row) {
        SCOPED_TRACE(row);
        ASSERT_EQ(descriptor.RowSupport(row), expected[row]);
        ASSERT_TRUE(passive[row].succeeded_) << passive[row].error_;
    }
}

TEST(RateStructuralJacobianTest, TestUnresolvedXccyAndChangedConsumedBasisInvalidate) {
    const Vector_<RateTradeDefinition_> trades = {Xccy()};
    const Vector_<RateCurveParameterCoordinate_> axis = {{"A", 0}, {"F", 0}, {"B", 0}, {"D", 0}};
    const auto market = XccyMarket();
    const auto baseline = CaptureRateStructuralJacobian(trades, market, axis);
    ASSERT_EQ(baseline.RowSupport(0), (Vector_<size_t>{0, 1, 2}));
    {
        auto changed = market;
        changed.xccyMarket_.reset();
        const auto descriptor = CaptureRateStructuralJacobian(trades, changed, axis);
        ASSERT_FALSE(descriptor.Available());
        ASSERT_EQ(descriptor.Reason(), "RATE_STRUCTURAL_UNRESOLVED_ROUTE");
    }
    {
        auto changed = market;
        const auto domestic = Handle_<CurveBlock_>(new CurveBlock_(changed.curveComponents_.at("A")));
        const auto foreign = Handle_<CurveBlock_>(new CurveBlock_(changed.curveComponents_.at("F")));
        auto native = std::make_shared<CrossCurrencyMarket_>(domestic, foreign, 1.2, changed.valuationTime_, Ccy_("USD"));
        native->SetBasisCurve(changed.curveComponents_.at("D"));
        changed.xccyMarket_ = native;
        const auto descriptor = CaptureRateStructuralJacobian(trades, changed, axis);
        ASSERT_EQ(descriptor.RowSupport(0), (Vector_<size_t>{0, 1, 3}));
        ASSERT_FALSE(SameRateStructuralJacobianStructure(baseline, descriptor));
    }
    {
        auto changed = market;
        const auto domestic = Handle_<CurveBlock_>(new CurveBlock_("GC-only", "USD", {{CollateralType_("GC"), changed.curveComponents_.at("A")}}));
        const auto foreign = Handle_<CurveBlock_>(new CurveBlock_(changed.curveComponents_.at("F")));
        changed.xccyMarket_ = std::make_shared<CrossCurrencyMarket_>(domestic, foreign, 1.2, changed.valuationTime_, Ccy_("USD"));
        const auto descriptor = CaptureRateStructuralJacobian(trades, changed, axis);
        ASSERT_FALSE(descriptor.Available());
        ASSERT_EQ(descriptor.Reason(), "RATE_STRUCTURAL_UNRESOLVED_ROUTE");
    }
}

TEST(RateStructuralJacobianTest, TestFreeParameterLayoutsAndMovedFromMetadata) {
    auto market = ComponentMarket();
    const Vector_<Date_> dates = {Date_(2026, 10, 12), Date_(2028, 10, 13)};
    market.curveComponents_["L"] =
        Handle_<DiscountCurve_>(new Tape::DiscountLogDF_<double>("L", "USD", dates, {0.0, -0.04}, DayBasis_("ACT_365F"), LogDfScheme_("LOG_LINEAR")));
    market.curveComponents_["Z"] = Handle_<DiscountCurve_>(
        new Tape::DiscountZeroRate_<double>("Z", "USD", dates[0], {dates[1]}, {0.02}, DayBasis_("ACT_365F"), LogDfScheme_("LOG_LINEAR")));
    market.curveComponents_["W"] = Handle_<DiscountCurve_>(new Tape::DiscountPWLF_<double>("W", "USD", {dates[1]}, {0.01}, {0.02}));
    const Vector_<RateTradeDefinition_> trades = {Deposit("logdf", "L"), Deposit("zero-rate", "Z"), Deposit("pwlf", "W")};
    const Vector_<RateCurveParameterCoordinate_> axis = {{"W", 1}, {"L", 0}, {"W", 0}, {"Z", 0}};
    auto descriptor = CaptureRateStructuralJacobian(trades, market, axis);
    ASSERT_TRUE(descriptor.Available());
    ASSERT_EQ(descriptor.RowSupport(0), (Vector_<size_t>{1}));
    ASSERT_EQ(descriptor.RowSupport(1), (Vector_<size_t>{3}));
    ASSERT_EQ(descriptor.RowSupport(2), (Vector_<size_t>{0, 2}));
    ASSERT_THROW(static_cast<void>(CaptureRateStructuralJacobian(trades, market, {{"L", 1}})), Exception_);
    const auto moved = std::move(descriptor);
    ASSERT_THROW(static_cast<void>(descriptor.Available()), Exception_);
    descriptor = moved;
    ASSERT_TRUE(SameRateStructuralJacobianStructure(descriptor, moved));
    auto changed = market;
    changed.curveComponents_["Z"] = Handle_<DiscountCurve_>(
        new Tape::DiscountZeroRate_<double>("Z", "USD", dates[0], {dates[1]}, {0.02}, DayBasis_("ACT_360"), LogDfScheme_("LOG_LINEAR")));
    ASSERT_FALSE(SameRateStructuralJacobianStructure(moved, CaptureRateStructuralJacobian(trades, changed, axis)));
    changed = market;
    const Vector_<Date_> cubicDates = {dates[0], Date_(2027, 10, 13), dates[1]};
    changed.curveComponents_["L"] = Handle_<DiscountCurve_>(
        new Tape::DiscountLogDF_<double>("L", "USD", cubicDates, {0.0, -0.02, -0.04}, DayBasis_("ACT_365F"), LogDfScheme_("LOG_LINEAR")));
    const auto linear = CaptureRateStructuralJacobian(trades, changed, axis);
    changed.curveComponents_["L"] = Handle_<DiscountCurve_>(
        new Tape::DiscountLogDF_<double>("L", "USD", cubicDates, {0.0, -0.02, -0.04}, DayBasis_("ACT_365F"), LogDfScheme_("LOG_CUBIC_NATURAL")));
    ASSERT_FALSE(SameRateStructuralJacobianStructure(linear, CaptureRateStructuralJacobian(trades, changed, axis)));
}

TEST(RateStructuralJacobianTest, TestUnsupportedInputDoesNotHideMalformedLaterCoordinates) {
    auto market = ComponentMarket();
    market.curveComponents_["custom"] = Handle_<DiscountCurve_>(new DerivedCurve_("custom", "USD", {Date_(2028, 10, 13)}, {0.01}));
    ASSERT_THROW(static_cast<void>(CaptureRateStructuralJacobian({}, market, {{"custom", 0}, {"missing", 0}})), Exception_);
    ASSERT_THROW(static_cast<void>(CaptureRateStructuralJacobian({}, market, {{"custom", 0}, {"A", 1}})), Exception_);
    ASSERT_THROW(static_cast<void>(CaptureRateStructuralJacobian({}, market, {{"custom", 0}, {"custom", 0}})), Exception_);
}

TEST(RateStructuralJacobianTest, TestUnrepresentableParametersReportUnavailableForInputsAndConsumedGraph) {
    auto market = ComponentMarket();
    market.valuationTime_ = DateTime_(Date_(2029, 10, 12));
    const Vector_<RateTradeDefinition_> trades = {Deposit("expired-deposit", "A")};
    const auto input = CaptureRateStructuralJacobian(trades, market, {{"A", 0}});
    ASSERT_FALSE(input.Available());
    ASSERT_EQ(input.Reason(), "RATE_STRUCTURAL_UNREPRESENTABLE_CURVE_PARAMETERS");
    market.curveComponents_["future-axis"] =
        Handle_<DiscountCurve_>(NewDiscountPWC("future-axis", "USD", PiecewiseConstant_({Date_(2031, 10, 13)}, {0.02})));
    const auto consumed = CaptureRateStructuralJacobian(trades, market, {{"future-axis", 0}});
    ASSERT_FALSE(consumed.Available());
    ASSERT_EQ(consumed.Reason(), "RATE_STRUCTURAL_UNREPRESENTABLE_CURVE_PARAMETERS");
    ASSERT_EQ(consumed.InputAxis()[0].componentKey_, "future-axis");
    ASSERT_THROW(static_cast<void>(consumed.RowSupport(0)), Exception_);
}

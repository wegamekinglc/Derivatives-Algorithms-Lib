//
// Created by Codex on 2026/7/13.
//

#include <gtest/gtest.h>

#include <dal/curve/discount.hpp>
#include <dal/curve/jointycctx.hpp>
#include <dal/curve/xccypricing.hpp>
#include <dal/storage/archive.hpp>

using namespace Dal;

namespace {
    CrossCurrencySwapConfig_ MakeQuarterlyConfig(XccyNotionalMode_ mode) {
        CrossCurrencySwapConfig_ config;
        config.pair_ = CurrencyPair_(Ccy_("USD"), Ccy_("EUR"));
        config.domesticNotional_ = 110.0;
        config.foreignNotional_ = 100.0;
        config.notionalMode_ = mode;

        config.convention_.domesticLeg_.paymentFrequency_ = PeriodLength_("3M");
        config.convention_.domesticLeg_.dayBasis_ = DayBasis_("ACT_365F");
        config.convention_.domesticLeg_.accrualHolidays_ = Holidays::None();
        config.convention_.domesticLeg_.paymentHolidays_ = Holidays::None();
        config.convention_.domesticLeg_.businessDayConvention_ = BizDayConvention_("Unadjusted");
        config.convention_.domesticLeg_.paymentConvention_ = BizDayConvention_("Unadjusted");
        config.convention_.foreignLeg_ = config.convention_.domesticLeg_;

        config.convention_.domesticIndex_.fixingLag_ = 2;
        config.convention_.domesticIndex_.fixingHolidays_ = Holidays::None();
        config.convention_.domesticIndex_.forecastTenor_ = PeriodLength_("3M");
        config.convention_.foreignIndex_ = config.convention_.domesticIndex_;

        config.domesticRateFixing_ = {"USD-SOFR-3M", 11, 0};
        config.foreignRateFixing_ = {"EUR-ESTR-3M", 11, 0};
        config.fxReset_.fixingLag_ = 2;
        config.fxReset_.fixingHolidays_ = Holidays::None();
        config.fxReset_.fixingConvention_ = BizDayConvention_("Preceding");
        config.fxReset_.fixingHour_ = 10;
        config.fxReset_.fixingMinute_ = 30;
        return config;
    }

    bool HasRequest(const Vector_<FixingRequest_>& requests, const String_& indexName, const DateTime_& fixingTime) {
        for (const auto& request : requests) {
            if (request.indexName_ == indexName && request.fixingTime_ == fixingTime)
                return true;
        }
        return false;
    }
    class ConstantDiscountCurve_ : public DiscountCurve_ {
        double value_;

    public:
        ConstantDiscountCurve_(const String_& name, const String_& ccy, double value) : DiscountCurve_(name, ccy), value_(value) {}

        double operator()(const Date_&, const Date_&) const override { return value_; }
        void Poll(Vector_<const YCComponent_*>* all) const override { all->push_back(this); }
        void Poll(std::map<const YCComponent_*, Handle_<YCComponent_>>*) const override {}
        [[nodiscard]] ConstantDiscountCurve_* Clone(const String_& newName, const YCComponent_::substitutions_t&) const override {
            return new ConstantDiscountCurve_(newName, ccy_.String(), value_);
        }
        void Write(Archive::Store_&) const override {}
    };

    class MappedDiscountCurve_ : public DiscountCurve_ {
        Date_ valuationDate_;
        std::map<Date_, double> valuationDfs_;
        std::map<pair<Date_, Date_>, double> forwardDfs_;

        MappedDiscountCurve_(const String_& name,
                             const String_& ccy,
                             const Date_& valuationDate,
                             const std::map<Date_, double>& valuationDfs,
                             const std::map<pair<Date_, Date_>, double>& forwardDfs)
            : DiscountCurve_(name, ccy), valuationDate_(valuationDate), valuationDfs_(valuationDfs), forwardDfs_(forwardDfs) {}

    public:
        MappedDiscountCurve_(const String_& name, const String_& ccy, const Date_& valuationDate)
            : DiscountCurve_(name, ccy), valuationDate_(valuationDate) {}

        void SetValuationDf(const Date_& to, double value) { valuationDfs_[to] = value; }
        void SetForwardDf(const Date_& from, const Date_& to, double value) { forwardDfs_[{from, to}] = value; }

        double operator()(const Date_& from, const Date_& to) const override {
            const auto forward = forwardDfs_.find({from, to});
            if (forward != forwardDfs_.end())
                return forward->second;
            if (from == valuationDate_) {
                const auto valuation = valuationDfs_.find(to);
                if (valuation != valuationDfs_.end())
                    return valuation->second;
            }
            return 1.0;
        }
        void Poll(Vector_<const YCComponent_*>* all) const override { all->push_back(this); }
        void Poll(std::map<const YCComponent_*, Handle_<YCComponent_>>*) const override {}
        [[nodiscard]] MappedDiscountCurve_* Clone(const String_& newName, const YCComponent_::substitutions_t&) const override {
            return new MappedDiscountCurve_(newName, ccy_.String(), valuationDate_, valuationDfs_, forwardDfs_);
        }
        void Write(Archive::Store_&) const override {}
    };

    struct PricingMarket_ {
        ConstantDiscountCurve_ domestic_{"domestic", "USD", 1.0};
        ConstantDiscountCurve_ foreign_{"foreign", "EUR", 1.0};
        ConstantDiscountCurve_ basis_{"basis", "USD", 1.0};
        Tape::JointCurveBlock_<double> domesticBlock_;
        Tape::JointCurveBlock_<double> foreignBlock_;

        PricingMarket_() {
            const CollateralType_ ois(CollateralType_::Value_::OIS);
            domesticBlock_.discountCurves[ois] = &domestic_;
            domesticBlock_.forwardCurves[PeriodLength_("3M")] = &domestic_;
            foreignBlock_.discountCurves[ois] = &foreign_;
            foreignBlock_.forwardCurves[PeriodLength_("3M")] = &foreign_;
        }

        XccyMarketView_<double> View(const DateTime_& valuationTime) const {
            XccyMarketView_<double> result;
            result.valuationTime_ = valuationTime;
            result.pair_ = CurrencyPair_(Ccy_("USD"), Ccy_("EUR"));
            result.collateralCurrency_ = Ccy_("USD");
            result.fxSpot_ = 1.10;
            result.domestic_ = &domesticBlock_;
            result.foreign_ = &foreignBlock_;
            result.basis_ = &basis_;
            return result;
        }
    };

} // namespace

TEST(XccyPricingTest, TestQuarterlyMtmPlanResetsFromSecondPeriod) {
    const auto plan = BuildXccyCashflowPlan(Date_(2024, 1, 4), Date_(2025, 1, 4), MakeQuarterlyConfig(XccyNotionalMode_::Value_::MARK_TO_MARKET));

    ASSERT_EQ(plan.domesticPeriods_.size(), 4U);
    ASSERT_EQ(plan.foreignPeriods_.size(), 4U);
    ASSERT_EQ(plan.resets_.size(), 3U);
    ASSERT_EQ(plan.resets_[0].effectiveDate_, plan.domesticPeriods_[1].schedule_.accrualStart_);
    ASSERT_EQ(plan.resets_[0].domesticPeriodIndex_, 1);
    ASSERT_EQ(plan.resets_[1].domesticPeriodIndex_, 2);
    ASSERT_EQ(plan.resets_[2].domesticPeriodIndex_, 3);
}

TEST(XccyPricingTest, TestResettableAndMtmPlansUseSameResetDates) {
    const auto resettable = BuildXccyCashflowPlan(Date_(2024, 1, 4), Date_(2025, 1, 4), MakeQuarterlyConfig(XccyNotionalMode_::Value_::RESETTABLE));
    const auto mtm = BuildXccyCashflowPlan(Date_(2024, 1, 4), Date_(2025, 1, 4), MakeQuarterlyConfig(XccyNotionalMode_::Value_::MARK_TO_MARKET));

    ASSERT_EQ(resettable.resets_.size(), mtm.resets_.size());
    for (int i = 0; i < static_cast<int>(resettable.resets_.size()); ++i) {
        ASSERT_EQ(resettable.resets_[i].effectiveDate_, mtm.resets_[i].effectiveDate_);
        ASSERT_EQ(resettable.resets_[i].fxFixingTime_, mtm.resets_[i].fxFixingTime_);
    }
}

TEST(XccyPricingTest, TestFixedPlanHasNoResets) {
    const auto plan = BuildXccyCashflowPlan(Date_(2024, 1, 4), Date_(2025, 1, 4), MakeQuarterlyConfig(XccyNotionalMode_::Value_::FIXED));
    ASSERT_TRUE(plan.resets_.empty());
}

TEST(XccyPricingTest, TestFixedPlanLeavesUnconfiguredRateFixingMetadataEmpty) {
    auto config = MakeQuarterlyConfig(XccyNotionalMode_::Value_::FIXED);
    config.domesticRateFixing_ = FixingIdentity_();
    config.foreignRateFixing_ = FixingIdentity_();

    const auto plan = BuildXccyCashflowPlan(Date_(2024, 1, 4), Date_(2025, 1, 4), config);

    ASSERT_FALSE(plan.domesticPeriods_.empty());
    ASSERT_FALSE(plan.foreignPeriods_.empty());
    for (const auto& period : plan.domesticPeriods_) {
        ASSERT_TRUE(period.rateIndexName_.empty());
        ASSERT_FALSE(period.rateFixingTime_.IsValid());
    }
    for (const auto& period : plan.foreignPeriods_) {
        ASSERT_TRUE(period.rateIndexName_.empty());
        ASSERT_FALSE(period.rateFixingTime_.IsValid());
    }

    const PricingMarket_ curves;
    const MarketFixingSnapshot_ fixings;
    ASSERT_TRUE(std::isfinite(PriceXccyParSpread<double>(plan, curves.View(DateTime_(Date_(2024, 1, 4))), fixings)));
}

TEST(XccyPricingTest, TestPlanPreservesShortFinalStub) {
    const auto plan = BuildXccyCashflowPlan(Date_(2024, 1, 4), Date_(2025, 2, 15), MakeQuarterlyConfig(XccyNotionalMode_::Value_::MARK_TO_MARKET));

    ASSERT_EQ(plan.domesticPeriods_.size(), 5U);
    ASSERT_TRUE(plan.domesticPeriods_.back().schedule_.isStub_);
    ASSERT_EQ(plan.domesticPeriods_.back().schedule_.accrualEnd_, Date_(2025, 2, 15));
    ASSERT_EQ(plan.resets_.size(), 4U);
}

TEST(XccyPricingTest, TestPlanAppliesRateAndFxFixingLagAndTime) {
    const auto plan = BuildXccyCashflowPlan(Date_(2024, 1, 8), Date_(2024, 7, 8), MakeQuarterlyConfig(XccyNotionalMode_::Value_::MARK_TO_MARKET));

    ASSERT_EQ(plan.domesticPeriods_[0].rateFixingTime_, DateTime_(Date_(2024, 1, 4), 11, 0));
    ASSERT_EQ(plan.foreignPeriods_[0].rateFixingTime_, DateTime_(Date_(2024, 1, 4), 11, 0));
    ASSERT_EQ(plan.resets_[0].effectiveDate_, Date_(2024, 4, 8));
    ASSERT_EQ(plan.resets_[0].fxFixingTime_, DateTime_(Date_(2024, 4, 4), 10, 30));
}

TEST(XccyPricingTest, TestRequiredHistoricalFixingsExcludePaidCoupons) {
    const auto plan = BuildXccyCashflowPlan(Date_(2024, 1, 8), Date_(2025, 1, 8), MakeQuarterlyConfig(XccyNotionalMode_::Value_::MARK_TO_MARKET));
    const auto requests = RequiredHistoricalFixings(plan, DateTime_(Date_(2024, 4, 9), 12, 0));

    ASSERT_EQ(requests.size(), 3U);
    ASSERT_TRUE(HasRequest(requests, "USD-SOFR-3M", DateTime_(Date_(2024, 4, 4), 11, 0)));
    ASSERT_TRUE(HasRequest(requests, "EUR-ESTR-3M", DateTime_(Date_(2024, 4, 4), 11, 0)));
    ASSERT_TRUE(HasRequest(requests, "FX[EUR/USD]", DateTime_(Date_(2024, 4, 4), 10, 30)));
}

TEST(XccyPricingTest, TestRequiredHistoricalFixingsIncludeValuationDatePayments) {
    const auto plan = BuildXccyCashflowPlan(Date_(2024, 1, 4), Date_(2025, 1, 4), MakeQuarterlyConfig(XccyNotionalMode_::Value_::MARK_TO_MARKET));
    const auto requests = RequiredHistoricalFixings(plan, DateTime_(Date_(2024, 4, 4), 12, 0));

    ASSERT_EQ(requests.size(), 5U);
    ASSERT_TRUE(HasRequest(requests, "USD-SOFR-3M", DateTime_(Date_(2024, 1, 2), 11, 0)));
    ASSERT_TRUE(HasRequest(requests, "EUR-ESTR-3M", DateTime_(Date_(2024, 1, 2), 11, 0)));
    ASSERT_TRUE(HasRequest(requests, "USD-SOFR-3M", DateTime_(Date_(2024, 4, 2), 11, 0)));
    ASSERT_TRUE(HasRequest(requests, "EUR-ESTR-3M", DateTime_(Date_(2024, 4, 2), 11, 0)));
    ASSERT_TRUE(HasRequest(requests, "FX[EUR/USD]", DateTime_(Date_(2024, 4, 2), 10, 30)));
    for (int i = 1; i < static_cast<int>(requests.size()); ++i) {
        ASSERT_TRUE(requests[i - 1].indexName_ < requests[i].indexName_ ||
                    (requests[i - 1].indexName_ == requests[i].indexName_ && requests[i - 1].fixingTime_ < requests[i].fixingTime_));
    }
}

TEST(XccyPricingTest, TestResetPlanRejectsNegativeFxFixingLag) {
    auto config = MakeQuarterlyConfig(XccyNotionalMode_::Value_::RESETTABLE);
    config.fxReset_.fixingLag_ = -1;

    ASSERT_THROW(static_cast<void>(BuildXccyCashflowPlan(Date_(2024, 1, 4), Date_(2025, 1, 4), config)), Dal::Exception_);
}

TEST(XccyPricingTest, TestResetPlanRequiresRateFixingIdentities) {
    auto config = MakeQuarterlyConfig(XccyNotionalMode_::Value_::MARK_TO_MARKET);
    config.domesticRateFixing_ = FixingIdentity_();

    ASSERT_THROW(static_cast<void>(BuildXccyCashflowPlan(Date_(2024, 1, 4), Date_(2025, 1, 4), config)), Dal::Exception_);
}
TEST(XccyPricingTest, TestPastFxResetRequiresHistoricalFixing) {
    const auto plan = BuildXccyCashflowPlan(Date_(2024, 1, 4), Date_(2024, 7, 4), MakeQuarterlyConfig(XccyNotionalMode_::Value_::MARK_TO_MARKET));
    const PricingMarket_ curves;
    const auto market = curves.View(DateTime_(Date_(2024, 4, 3), 12, 0));
    const MarketFixingSnapshot_ fixings;

    ASSERT_THROW(static_cast<void>(ResolveXccyNotionals<double>(plan, market, fixings)), Dal::Exception_);
}

TEST(XccyPricingTest, TestTwoPeriodMtmPrincipalCashflowsMatchHandCalculation) {
    auto config = MakeQuarterlyConfig(XccyNotionalMode_::Value_::MARK_TO_MARKET);
    config.convention_.initialNotionalExchange_ = true;
    config.convention_.finalNotionalExchange_ = true;
    config.convention_.spreadOnForeignLeg_ = true;
    config.domesticRateFixing_.fixingHour_ = 13;
    config.foreignRateFixing_.fixingHour_ = 13;
    config.fxReset_.fixingLag_ = 70;
    const auto plan = BuildXccyCashflowPlan(Date_(2024, 1, 8), Date_(2024, 7, 8), config);

    MarketFixingSnapshot_::values_t values;
    values[FxIndexName(config.pair_)][plan.resets_[0].fxFixingTime_] = 1.20;
    const MarketFixingSnapshot_ fixings(values);
    const PricingMarket_ curves;
    const auto market = curves.View(DateTime_(Date_(2024, 1, 4), 12, 0));

    const auto resolved = ResolveXccyNotionals<double>(plan, market, fixings);
    ASSERT_NEAR(resolved.domesticNotionals_[0], 110.0, 1.0e-12);
    ASSERT_NEAR(resolved.domesticNotionals_[1], 120.0, 1.0e-12);
    ASSERT_EQ(resolved.mtmDeltas_.size(), 1U);
    ASSERT_NEAR(resolved.mtmDeltas_[0], 10.0, 1.0e-12);

    const double explicitDomesticPrincipal = -110.0 - 10.0 + 120.0;
    const double explicitForeignPrincipal = (-100.0 + 100.0) * 1.10;
    const double explicitForeignAnnuity = 100.0 * (plan.foreignPeriods_[0].accrual_.dcf_ + plan.foreignPeriods_[1].accrual_.dcf_) * 1.10;
    const double expectedParSpread = (explicitDomesticPrincipal - explicitForeignPrincipal) / explicitForeignAnnuity;
    ASSERT_NEAR(PriceXccyParSpread<double>(plan, market, fixings), expectedParSpread, 1.0e-12);
}

TEST(XccyPricingTest, TestMultipleFutureMtmExchangesUpdateDomesticPvSequentially) {
    auto config = MakeQuarterlyConfig(XccyNotionalMode_::Value_::MARK_TO_MARKET);
    config.domesticNotional_ = 8.0;
    config.foreignNotional_ = 1.0;
    config.convention_.domesticLeg_.dayBasis_ = DayBasis_("30_360");
    config.convention_.foreignLeg_.dayBasis_ = DayBasis_("30_360");
    config.convention_.spreadOnForeignLeg_ = true;
    config.convention_.initialNotionalExchange_ = false;
    config.convention_.finalNotionalExchange_ = false;
    config.convention_.domesticIndex_.fixingLag_ = 0;
    config.convention_.foreignIndex_.fixingLag_ = 0;

    const Date_ valuationDate(2024, 1, 3);
    const auto plan = BuildXccyCashflowPlan(Date_(2024, 1, 4), Date_(2024, 10, 4), config);
    ASSERT_EQ(plan.resets_.size(), 2U);

    MappedDiscountCurve_ domestic("domestic_mapped", "USD", valuationDate);
    MappedDiscountCurve_ foreign("foreign_mapped", "EUR", valuationDate);
    ConstantDiscountCurve_ basis("basis_mapped", "USD", 1.0);
    domestic.SetForwardDf(plan.domesticPeriods_[0].schedule_.accrualStart_, plan.domesticPeriods_[0].schedule_.accrualEnd_, 0.5);
    domestic.SetValuationDf(plan.resets_[0].effectiveDate_, 0x1p50);
    foreign.SetValuationDf(plan.resets_[0].fxFixingTime_.Date(), 16.0);
    foreign.SetValuationDf(plan.resets_[1].fxFixingTime_.Date(), 17.0);

    const CollateralType_ ois(CollateralType_::Value_::OIS);
    Tape::JointCurveBlock_<double> domesticBlock;
    Tape::JointCurveBlock_<double> foreignBlock;
    domesticBlock.discountCurves[ois] = &domestic;
    foreignBlock.discountCurves[ois] = &foreign;
    XccyMarketView_<double> market;
    market.valuationTime_ = DateTime_(valuationDate, 12, 0);
    market.pair_ = config.pair_;
    market.collateralCurrency_ = config.pair_.domestic_;
    market.fxSpot_ = 1.0;
    market.domestic_ = &domesticBlock;
    market.foreign_ = &foreignBlock;
    market.basis_ = &basis;

    const MarketFixingSnapshot_ fixings;
    ASSERT_NEAR(PriceXccyParSpread<double>(plan, market, fixings), -4.0 / 3.0, 1.0e-12);
}

namespace {
    struct CustomPricingMarket_ {
        ConstantDiscountCurve_ domestic_;
        ConstantDiscountCurve_ foreign_;
        ConstantDiscountCurve_ basis_;
        Tape::JointCurveBlock_<double> domesticBlock_;
        Tape::JointCurveBlock_<double> foreignBlock_;
        double spot_;

        CustomPricingMarket_(double domesticDf, double foreignDf, double basisDf, double spot = 1.10)
            : domestic_("domestic_custom", "USD", domesticDf), foreign_("foreign_custom", "EUR", foreignDf), basis_("basis_custom", "USD", basisDf),
              spot_(spot) {
            const CollateralType_ ois(CollateralType_::Value_::OIS);
            domesticBlock_.discountCurves[ois] = &domestic_;
            domesticBlock_.forwardCurves[PeriodLength_("3M")] = &domestic_;
            foreignBlock_.discountCurves[ois] = &foreign_;
            foreignBlock_.forwardCurves[PeriodLength_("3M")] = &foreign_;
        }

        XccyMarketView_<double> View(const DateTime_& valuationTime) const {
            XccyMarketView_<double> result;
            result.valuationTime_ = valuationTime;
            result.pair_ = CurrencyPair_(Ccy_("USD"), Ccy_("EUR"));
            result.collateralCurrency_ = Ccy_("USD");
            result.fxSpot_ = spot_;
            result.domestic_ = &domesticBlock_;
            result.foreign_ = &foreignBlock_;
            result.basis_ = &basis_;
            return result;
        }
    };
} // namespace

TEST(XccyPricingTest, TestFxResetAtValuationUsesSuppliedFixing) {
    const auto plan = BuildXccyCashflowPlan(Date_(2024, 1, 4), Date_(2024, 7, 4), MakeQuarterlyConfig(XccyNotionalMode_::Value_::MARK_TO_MARKET));
    MarketFixingSnapshot_::values_t values;
    values[FxIndexName(plan.config_.pair_)][plan.resets_[0].fxFixingTime_] = 1.25;
    const MarketFixingSnapshot_ fixings(values);
    const PricingMarket_ curves;
    const auto market = curves.View(plan.resets_[0].fxFixingTime_);

    const auto resolved = ResolveXccyNotionals<double>(plan, market, fixings);
    ASSERT_NEAR(resolved.domesticNotionals_[1], 125.0, 1.0e-12);
}

TEST(XccyPricingTest, TestFutureFxResetUsesActiveForward) {
    const auto plan = BuildXccyCashflowPlan(Date_(2024, 1, 4), Date_(2024, 7, 4), MakeQuarterlyConfig(XccyNotionalMode_::Value_::MARK_TO_MARKET));
    const DateTime_ valuationTime(Date_(2024, 4, 1), 12, 0);
    const MarketFixingSnapshot_ fixings;
    const CustomPricingMarket_ first(0.99, 0.98, 0.97);
    const CustomPricingMarket_ second(0.99, 0.98, 0.90);

    const double firstNotional = ResolveXccyNotionals<double>(plan, first.View(valuationTime), fixings).domesticNotionals_[1];
    const double secondNotional = ResolveXccyNotionals<double>(plan, second.View(valuationTime), fixings).domesticNotionals_[1];
    ASSERT_NEAR(firstNotional, 100.0 * 1.10 * 0.98 / (0.99 * 0.97), 1.0e-12);
    ASSERT_NE(firstNotional, secondNotional);
}

TEST(XccyPricingTest, TestPastRateFixingRequiredForUnpaidCoupon) {
    const auto plan = BuildXccyCashflowPlan(Date_(2024, 1, 4), Date_(2024, 4, 4), MakeQuarterlyConfig(XccyNotionalMode_::Value_::FIXED));
    const PricingMarket_ curves;
    const auto market = curves.View(DateTime_(Date_(2024, 1, 3), 12, 0));
    const MarketFixingSnapshot_ fixings;

    ASSERT_THROW(static_cast<void>(PriceXccyParSpread<double>(plan, market, fixings)), Dal::Exception_);
}

TEST(XccyPricingTest, TestRateFixingAtValuationUsesSuppliedValue) {
    auto config = MakeQuarterlyConfig(XccyNotionalMode_::Value_::FIXED);
    config.convention_.domesticIndex_.fixingLag_ = 0;
    config.convention_.foreignIndex_.fixingLag_ = 0;
    const auto plan = BuildXccyCashflowPlan(Date_(2024, 1, 4), Date_(2024, 4, 4), config);
    const DateTime_ valuationTime(Date_(2024, 1, 4), 11, 0);
    MarketFixingSnapshot_::values_t values;
    values[config.domesticRateFixing_.indexName_][valuationTime] = 0.05;
    values[config.foreignRateFixing_.indexName_][valuationTime] = 0.02;
    const MarketFixingSnapshot_ fixings(values);
    const PricingMarket_ curves;

    ASSERT_NEAR(PriceXccyParSpread<double>(plan, curves.View(valuationTime), fixings), 0.03, 1.0e-12);
}

TEST(XccyPricingTest, TestFutureRateFixingUsesActiveCurve) {
    auto config = MakeQuarterlyConfig(XccyNotionalMode_::Value_::FIXED);
    config.convention_.domesticIndex_.fixingLag_ = 0;
    config.convention_.foreignIndex_.fixingLag_ = 0;
    config.domesticRateFixing_.fixingHour_ = 13;
    config.foreignRateFixing_.fixingHour_ = 13;
    const auto plan = BuildXccyCashflowPlan(Date_(2024, 1, 4), Date_(2024, 4, 4), config);
    const DateTime_ valuationTime(Date_(2024, 1, 4), 12, 0);
    const MarketFixingSnapshot_ fixings;
    const CustomPricingMarket_ first(0.99, 1.0, 1.0);
    const CustomPricingMarket_ second(0.98, 1.0, 1.0);

    const double firstSpread = PriceXccyParSpread<double>(plan, first.View(valuationTime), fixings);
    const double secondSpread = PriceXccyParSpread<double>(plan, second.View(valuationTime), fixings);
    ASSERT_NE(firstSpread, secondSpread);
}

TEST(XccyPricingTest, TestPaidCouponIsFilteredBeforeFixingLookup) {
    const auto plan = BuildXccyCashflowPlan(Date_(2024, 1, 4), Date_(2024, 7, 4), MakeQuarterlyConfig(XccyNotionalMode_::Value_::FIXED));
    const DateTime_ valuationTime(Date_(2024, 4, 5), 12, 0);
    MarketFixingSnapshot_::values_t values;
    values[plan.config_.domesticRateFixing_.indexName_][plan.domesticPeriods_[1].rateFixingTime_] = 0.04;
    values[plan.config_.foreignRateFixing_.indexName_][plan.foreignPeriods_[1].rateFixingTime_] = 0.03;
    const MarketFixingSnapshot_ fixings(values);
    const PricingMarket_ curves;

    ASSERT_NEAR(PriceXccyParSpread<double>(plan, curves.View(valuationTime), fixings), 0.01, 1.0e-12);
}

TEST(XccyPricingTest, TestValuationDatePaymentUsesUnitDiscountFactor) {
    const auto plan = BuildXccyCashflowPlan(Date_(2024, 1, 4), Date_(2024, 4, 4), MakeQuarterlyConfig(XccyNotionalMode_::Value_::FIXED));
    const DateTime_ valuationTime(Date_(2024, 4, 4), 12, 0);
    MarketFixingSnapshot_::values_t values;
    values[plan.config_.domesticRateFixing_.indexName_][plan.domesticPeriods_[0].rateFixingTime_] = 0.05;
    values[plan.config_.foreignRateFixing_.indexName_][plan.foreignPeriods_[0].rateFixingTime_] = 0.02;
    const MarketFixingSnapshot_ fixings(values);
    const CustomPricingMarket_ curves(0.50, 0.80, 0.90);

    ASSERT_NEAR(PriceXccyParSpread<double>(plan, curves.View(valuationTime), fixings), 0.03, 1.0e-12);
}

TEST(XccyPricingTest, TestMaturedTradeThrowsWithoutLookingUpPaidFixings) {
    const auto plan = BuildXccyCashflowPlan(Date_(2024, 1, 4), Date_(2024, 4, 4), MakeQuarterlyConfig(XccyNotionalMode_::Value_::FIXED));
    const PricingMarket_ curves;
    const MarketFixingSnapshot_ fixings;

    ASSERT_THROW(static_cast<void>(PriceXccyParSpread<double>(plan, curves.View(DateTime_(Date_(2024, 4, 5), 12, 0)), fixings)), Dal::Exception_);
}

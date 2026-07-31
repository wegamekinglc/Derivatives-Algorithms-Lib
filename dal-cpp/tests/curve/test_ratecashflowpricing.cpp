//
// Created by dal-implementer on 2026/7/28.
//

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <future>
#include <limits>
#include <stdexcept>
#include <thread>

#include <dal/curve/curveblock.hpp>
#include <dal/curve/curveparameterization.hpp>
#include <dal/curve/piecewiseconstant.hpp>
#include <dal/curve/piecewiselinear.hpp>
#include <dal/curve/ratecashflowpricing.hpp>
#include <dal/curve/ratecashflowpricing_internal.hpp>
#include <dal/curve/ycconst.hpp>
#include <dal/curve/ycimp.hpp>
#include <dal/curve/yclogdf.hpp>
#include <dal/curve/yczerorate.hpp>

namespace {
    Dal::RateIndexConvention_ QuarterlyIndex() {
        Dal::RateIndexConvention_ result;
        result.forecastTenor_ = Dal::PeriodLength_("3M");
        result.dayBasis_ = Dal::DayBasis_("ACT_365F");
        result.collateral_ = Dal::CollateralType_(Dal::CollateralType_::Value_::OIS);
        return result;
    }

    Dal::RateLegConvention_ AnnualLeg() {
        Dal::RateLegConvention_ result;
        result.paymentFrequency_ = Dal::PeriodLength_("12M");
        result.dayBasis_ = Dal::DayBasis_("ACT_365F");
        return result;
    }

    Dal::Handle_<Dal::DiscountCurve_> FlatCurve(const Dal::Date_& maturity, double rate = 0.04, const Dal::String_& ccy = "USD") {
        return Dal::Handle_<Dal::DiscountCurve_>(Dal::NewDiscountPWC("flat", ccy, Dal::PiecewiseConstant_({maturity}, {rate})));
    }

    Dal::RatePricingMarket_ Market(const Dal::Date_& today, const Dal::Handle_<Dal::DiscountCurve_>& curve) {
        Dal::RatePricingMarket_ result;
        result.valuationTime_ = Dal::DateTime_(today, 10, 30);
        result.resultCurrency_ = Dal::Ccy_("USD");
        result.curveComponents_["discount"] = curve;
        result.curveComponents_["forecast"] = curve;
        result.curveComponents_["reference"] = curve;
        result.fixings_ = Dal::Handle_<Dal::MarketFixingSnapshot_>(new Dal::MarketFixingSnapshot_());
        return result;
    }

    Dal::RateTradeDefinition_ Trade(const Dal::RateInstrumentType_& type,
                                    const Dal::Date_& today,
                                    const Dal::Date_& start,
                                    const Dal::Date_& maturity,
                                    const Dal::RateTradeTerms_& terms) {
        return {"trade-1", type, today, start, maturity, Dal::Ccy_("USD"), terms};
    }

    Dal::DepositTradeTerms_ DepositTerms() {
        Dal::DepositTradeTerms_ result;
        result.notional_ = 100.0;
        result.contractRate_ = 0.05;
        result.lend_ = true;
        result.index_ = QuarterlyIndex();
        result.discountComponentKey_ = "discount";
        return result;
    }

    class UnsupportedDiscountCurve_ : public Dal::DiscountCurve_ {
    public:
        UnsupportedDiscountCurve_() : DiscountCurve_("unsupported", "USD") {}

        double operator()(const Dal::Date_&, const Dal::Date_&) const override { return 1.0; }
        void Poll(Dal::Vector_<const Dal::YCComponent_*>* all) const override { all->push_back(this); }
        void Poll(std::map<const Dal::YCComponent_*, Dal::Handle_<Dal::YCComponent_>>*) const override {}
        [[nodiscard]] std::unique_ptr<Dal::YCComponent_> Clone(const Dal::String_&, const Dal::YCComponent_::substitutions_t&) const override {
            return std::make_unique<UnsupportedDiscountCurve_>();
        }
        void Write(Dal::Archive::Store_&) const override {}
    };

    void AssertCanonicalFailure(const Dal::RateTradeNodeSensitivityResult_& result, const Dal::String_& reason) {
        ASSERT_FALSE(result.eligible_);
        ASSERT_DOUBLE_EQ(result.pv_, 0.0);
        ASSERT_TRUE(result.gradient_.empty());
        ASSERT_EQ(result.reason_, reason);
    }

    void
    AssertTradeValidationFailure(const Dal::RateTradeDefinition_& trade, const Dal::RatePricingMarket_& market, const std::string& errorFragment) {
        const auto priced = Dal::PriceRateTrade(trade, market);
        Dal::RateTradeNodeSensitivityResult_ sensitivity;
        ASSERT_NO_THROW(sensitivity = Dal::RateTradeNodeSensitivities(trade, market, "discount"));
        ASSERT_FALSE(priced.succeeded_);
        ASSERT_NE(std::string(priced.error_.c_str()).find(errorFragment), std::string::npos) << priced.error_;
        AssertCanonicalFailure(sensitivity, "TRADE_VALIDATION_FAILED");
    }

    template <class Builder_>
    void AssertRawNodeGradientMatchesCentralBumps(const Builder_& buildCurve,
                                                  const Dal::Vector_<>& parameters,
                                                  int expectedParameterCount,
                                                  double bump,
                                                  double absoluteTolerance,
                                                  double relativeTolerance,
                                                  const Dal::Vector_<int>& structuralZeroColumns) {
        const Dal::Date_ today(2026, 1, 15);
        const Dal::Date_ start(2026, 10, 15);
        const Dal::Date_ maturity(2029, 1, 15);
        const auto terms = DepositTerms();
        const auto trade = Trade(Dal::RateInstrumentType_("DEPOSIT"), today, start, maturity, terms);
        const auto market = Market(today, buildCurve(parameters));
        const auto aad = Dal::RateTradeNodeSensitivities(trade, market, "discount");

        ASSERT_TRUE(aad.eligible_);
        ASSERT_EQ(static_cast<int>(aad.gradient_.size()), expectedParameterCount);
        ASSERT_TRUE(std::isfinite(aad.pv_));
        for (int column = 0; column < expectedParameterCount; ++column) {
            auto plusParameters = parameters;
            auto minusParameters = parameters;
            plusParameters[column] += bump;
            minusParameters[column] -= bump;
            const auto plus = Dal::PriceRateTrade(trade, Market(today, buildCurve(plusParameters)));
            const auto minus = Dal::PriceRateTrade(trade, Market(today, buildCurve(minusParameters)));
            ASSERT_TRUE(plus.succeeded_);
            ASSERT_TRUE(minus.succeeded_);
            const double finiteDifference = (plus.pv_ - minus.pv_) / (2.0 * bump);
            const double tolerance = absoluteTolerance + relativeTolerance * std::max(std::abs(aad.gradient_[column]), std::abs(finiteDifference));
            ASSERT_NEAR(aad.gradient_[column], finiteDifference, tolerance) << "raw native-parameter column " << column;
        }

        auto borrowTerms = terms;
        borrowTerms.lend_ = false;
        const auto borrowTrade = Trade(Dal::RateInstrumentType_("DEPOSIT"), today, start, maturity, borrowTerms);
        const auto borrow = Dal::RateTradeNodeSensitivities(borrowTrade, market, "discount");
        ASSERT_TRUE(borrow.eligible_);
        ASSERT_DOUBLE_EQ(borrow.pv_, -aad.pv_);
        ASSERT_EQ(borrow.gradient_.size(), aad.gradient_.size());
        for (int column = 0; column < expectedParameterCount; ++column)
            ASSERT_DOUBLE_EQ(borrow.gradient_[column], -aad.gradient_[column]);

        const auto shortTrade = Trade(Dal::RateInstrumentType_("DEPOSIT"), today, today, start, terms);
        const auto structural = Dal::RateTradeNodeSensitivities(shortTrade, market, "discount");
        ASSERT_TRUE(structural.eligible_);
        ASSERT_EQ(static_cast<int>(structural.gradient_.size()), expectedParameterCount);
        for (const int column : structuralZeroColumns)
            ASSERT_NEAR(structural.gradient_[column], 0.0, 1.0e-12) << "structural-zero raw column " << column;
    }
} // namespace

TEST(RateCashflowPricingTest, TestRegistryAndDepositCashflows) {
    const auto families = Dal::RateInstrumentTypeListAll();
    ASSERT_EQ(families.size(), 7);
    ASSERT_EQ(families[0].String(), "DEPOSIT");
    ASSERT_EQ(families[1].String(), "FRA");
    ASSERT_EQ(families[2].String(), "FUTURE");
    ASSERT_EQ(families[3].String(), "OIS");
    ASSERT_EQ(families[4].String(), "IRS");
    ASSERT_EQ(families[5].String(), "BASIS_SWAP");
    ASSERT_EQ(families[6].String(), "XCCY");

    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ maturity(2027, 1, 15);
    const auto curve = FlatCurve(maturity);
    const auto market = Market(today, curve);
    Dal::DepositTradeTerms_ terms;
    terms.notional_ = 100.0;
    terms.contractRate_ = 0.05;
    terms.lend_ = true;
    terms.index_ = QuarterlyIndex();
    terms.discountComponentKey_ = "discount";
    const auto definition = Trade(Dal::RateInstrumentType_("DEPOSIT"), today, today, maturity, terms);

    const auto result = Dal::PriceRateTrade(definition, market);
    const double accrual = terms.index_.dayBasis_(today, maturity, nullptr);
    const double expected = -100.0 + 100.0 * (1.0 + 0.05 * accrual) * (*curve)(today, maturity);

    ASSERT_TRUE(result.succeeded_);
    ASSERT_NEAR(result.pv_, expected, 1.0e-10);
    ASSERT_TRUE(result.requiredHistoricalFixings_.empty());
    ASSERT_EQ(result.dependencyComponentKeys_, Dal::Vector_<Dal::String_>({"discount"}));

    terms.lend_ = false;
    const auto borrow = Trade(Dal::RateInstrumentType_("DEPOSIT"), today, today, maturity, terms);
    ASSERT_NEAR(Dal::PriceRateTrade(borrow, market).pv_, -expected, 1.0e-10);
}

TEST(RateCashflowPricingTest, TestDepositNodeAADMatchesCentralNativeParameterBump) {
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ maturity(2027, 1, 15);
    const double rate = 0.04;
    const auto market = Market(today, FlatCurve(maturity, rate));
    Dal::DepositTradeTerms_ terms;
    terms.notional_ = 100.0;
    terms.contractRate_ = 0.05;
    terms.lend_ = true;
    terms.index_ = QuarterlyIndex();
    terms.discountComponentKey_ = "discount";
    const auto trade = Trade(Dal::RateInstrumentType_("DEPOSIT"), today, today, maturity, terms);

    const auto aad = Dal::RateTradeNodeSensitivities(trade, market, "discount");
    const double epsilon = 1.0e-6;
    const double plus = Dal::PriceRateTrade(trade, Market(today, FlatCurve(maturity, rate + epsilon))).pv_;
    const double minus = Dal::PriceRateTrade(trade, Market(today, FlatCurve(maturity, rate - epsilon))).pv_;

    ASSERT_TRUE(aad.eligible_);
    ASSERT_EQ(aad.gradient_.size(), 1);
    ASSERT_NEAR(aad.pv_, Dal::PriceRateTrade(trade, market).pv_, 1.0e-12);
    ASSERT_NEAR(aad.gradient_[0], (plus - minus) / (2.0 * epsilon), 1.0e-6);
}

TEST(RateCashflowPricingTest, TestDepositNodeAADRejectsInvalidTradeCanonically) {
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ maturity(2027, 1, 15);
    const auto market = Market(today, FlatCurve(maturity));
    Dal::DepositTradeTerms_ terms;
    terms.notional_ = std::numeric_limits<double>::quiet_NaN();
    terms.contractRate_ = 0.05;
    terms.lend_ = true;
    terms.index_ = QuarterlyIndex();
    terms.discountComponentKey_ = "discount";
    const auto trade = Trade(Dal::RateInstrumentType_("DEPOSIT"), today, today, maturity, terms);

    const auto priced = Dal::PriceRateTrade(trade, market);
    Dal::RateTradeNodeSensitivityResult_ sensitivity;
    ASSERT_NO_THROW(sensitivity = Dal::RateTradeNodeSensitivities(trade, market, "discount"));

    ASSERT_FALSE(priced.succeeded_);
    ASSERT_NE(priced.error_.find("Deposit notional must be positive and finite"), Dal::String_::npos);
    AssertCanonicalFailure(sensitivity, "TRADE_VALIDATION_FAILED");
}

TEST(RateCashflowPricingTest, TestNodeSensitivityFinalizerPublishesOnlyCompleteFiniteCandidates) {
    using Dal::RateCashflowPricingInternal::FinalizeNodeSensitivityCandidate;
    using Dal::RateCashflowPricingInternal::NodeSensitivityCandidate_;

    const auto success = FinalizeNodeSensitivityCandidate(NodeSensitivityCandidate_{1.25, {2.0, -3.0}}, 2);
    ASSERT_TRUE(success.eligible_);
    ASSERT_DOUBLE_EQ(success.pv_, 1.25);
    ASSERT_EQ(success.gradient_, Dal::Vector_<>({2.0, -3.0}));
    ASSERT_TRUE(success.reason_.empty());

    AssertCanonicalFailure(FinalizeNodeSensitivityCandidate(NodeSensitivityCandidate_{std::numeric_limits<double>::quiet_NaN(), {2.0, -3.0}}, 2),
                           "AAD_EVALUATION_FAILED");
    AssertCanonicalFailure(FinalizeNodeSensitivityCandidate(NodeSensitivityCandidate_{std::numeric_limits<double>::infinity(), {2.0, -3.0}}, 2),
                           "AAD_EVALUATION_FAILED");
    AssertCanonicalFailure(FinalizeNodeSensitivityCandidate(NodeSensitivityCandidate_{-std::numeric_limits<double>::infinity(), {2.0, -3.0}}, 2),
                           "AAD_EVALUATION_FAILED");
    AssertCanonicalFailure(FinalizeNodeSensitivityCandidate(NodeSensitivityCandidate_{1.25, {2.0}}, 2), "AAD_EVALUATION_FAILED");
    AssertCanonicalFailure(FinalizeNodeSensitivityCandidate(NodeSensitivityCandidate_{1.25, {2.0, -3.0, 4.0}}, 2), "AAD_EVALUATION_FAILED");

    for (const double invalid :
         {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()}) {
        AssertCanonicalFailure(FinalizeNodeSensitivityCandidate(NodeSensitivityCandidate_{1.25, {invalid, -3.0}}, 2), "AAD_EVALUATION_FAILED");
        AssertCanonicalFailure(FinalizeNodeSensitivityCandidate(NodeSensitivityCandidate_{1.25, {2.0, invalid}}, 2), "AAD_EVALUATION_FAILED");
    }
}

TEST(RateCashflowPricingTest, TestNodeSensitivityAADStageCanonicalizesExceptionAndRewindsTape) {
    const auto failed = Dal::RateCashflowPricingInternal::RunNodeSensitivityAADStage(1, []() {
        auto parameters = Dal::RegisterCurveParameters(Dal::Vector_<>{0.04});
        Dal::AAD::NewRecording(*Dal::AAD::Tape());
        Dal::AAD::Number_ node = parameters[0] * parameters[0];
        Dal::AAD::Adjoint(node) = 1.0;
        throw std::runtime_error("injected after recording");
        return Dal::RateCashflowPricingInternal::NodeSensitivityCandidate_{};
    });
    AssertCanonicalFailure(failed, "AAD_EVALUATION_FAILED");

    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ maturity(2027, 1, 15);
    Dal::DepositTradeTerms_ terms;
    terms.notional_ = 100.0;
    terms.contractRate_ = 0.05;
    terms.lend_ = true;
    terms.index_ = QuarterlyIndex();
    terms.discountComponentKey_ = "discount";
    const auto trade = Trade(Dal::RateInstrumentType_("DEPOSIT"), today, today, maturity, terms);
    const auto valid = Dal::RateTradeNodeSensitivities(trade, Market(today, FlatCurve(maturity)), "discount");

    ASSERT_TRUE(valid.eligible_);
    ASSERT_EQ(valid.gradient_.size(), 1);
    ASSERT_TRUE(std::isfinite(valid.pv_));
    ASSERT_TRUE(std::isfinite(valid.gradient_[0]));
}

TEST(RateCashflowPricingTest, TestNodeSensitivityCurveClassifierReturnsOnlyRepresentationTagAndBorrowedPointer) {
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Vector_<Dal::Date_> knots{Dal::Date_(2026, 7, 15), Dal::Date_(2027, 1, 15), Dal::Date_(2028, 1, 15)};
    const Dal::Vector_<> rates{0.01, 0.02, 0.03};
    const Dal::Vector_<> logDf{0.0, -0.005, -0.02, -0.06};
    const Dal::DayBasis_ dayCount("ACT_365F");
    const std::unique_ptr<Dal::DiscountCurve_> pwc(Dal::NewDiscountPWC("pwc", "USD", Dal::PiecewiseConstant_(knots, rates)));
    const std::unique_ptr<Dal::DiscountCurve_> pwlf(Dal::NewDiscountPWLF("pwlf", "USD", Dal::PiecewiseLinear_(knots, rates, rates)));
    const std::unique_ptr<Dal::DiscountCurve_> logDiscount(Dal::NewDiscountLogDF(
        "logdf", "USD", Dal::Vector_<Dal::Date_>{today, knots[0], knots[1], knots[2]}, logDf, dayCount, Dal::LogDfScheme_::Value_::LOG_LINEAR));
    const std::unique_ptr<Dal::DiscountCurve_> zeroRate(
        Dal::NewDiscountZeroRate("zero", "USD", today, knots, rates, dayCount, Dal::LogDfScheme_::Value_::LOG_LINEAR));
    const UnsupportedDiscountCurve_ unsupported;

    const auto classifiedPwc = Dal::RateCashflowPricingInternal::ClassifyNodeSensitivityCurve(*pwc);
    const auto classifiedPwlf = Dal::RateCashflowPricingInternal::ClassifyNodeSensitivityCurve(*pwlf);
    const auto classifiedLogDiscount = Dal::RateCashflowPricingInternal::ClassifyNodeSensitivityCurve(*logDiscount);
    const auto classifiedZeroRate = Dal::RateCashflowPricingInternal::ClassifyNodeSensitivityCurve(*zeroRate);
    ASSERT_EQ(classifiedPwc.index(), 1);
    ASSERT_EQ(std::get<1>(classifiedPwc), pwc.get());
    ASSERT_EQ(classifiedPwlf.index(), 2);
    ASSERT_EQ(std::get<2>(classifiedPwlf), pwlf.get());
    ASSERT_EQ(classifiedLogDiscount.index(), 3);
    ASSERT_EQ(std::get<3>(classifiedLogDiscount), logDiscount.get());
    ASSERT_EQ(classifiedZeroRate.index(), 4);
    ASSERT_EQ(std::get<4>(classifiedZeroRate), zeroRate.get());
    ASSERT_EQ(Dal::RateCashflowPricingInternal::ClassifyNodeSensitivityCurve(unsupported).index(), 0);
}

TEST(RateCashflowPricingTest, TestDepositNodeAADPassiveValidationBoundaryMatrix) {
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ maturity(2027, 1, 15);
    const auto market = Market(today, FlatCurve(maturity));

    for (const double invalid :
         {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(), 0.0, -1.0}) {
        auto terms = DepositTerms();
        terms.notional_ = invalid;
        AssertTradeValidationFailure(Trade(Dal::RateInstrumentType_("DEPOSIT"), today, today, maturity, terms), market,
                                     "Deposit notional must be positive and finite");
    }
    for (const double invalid :
         {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()}) {
        auto terms = DepositTerms();
        terms.contractRate_ = invalid;
        AssertTradeValidationFailure(Trade(Dal::RateInstrumentType_("DEPOSIT"), today, today, maturity, terms), market,
                                     "Deposit contract rate must be finite");
    }

    const auto terms = DepositTerms();
    AssertTradeValidationFailure(Trade(Dal::RateInstrumentType_("DEPOSIT"), Dal::Date_(), today, maturity, terms), market,
                                 "Rate trade dates must be valid and start before maturity");
    AssertTradeValidationFailure(Trade(Dal::RateInstrumentType_("DEPOSIT"), today, Dal::Date_(), maturity, terms), market,
                                 "Rate trade dates must be valid and start before maturity");
    AssertTradeValidationFailure(Trade(Dal::RateInstrumentType_("DEPOSIT"), today, today, Dal::Date_(), terms), market,
                                 "Rate trade dates must be valid and start before maturity");
    AssertTradeValidationFailure(Trade(Dal::RateInstrumentType_("DEPOSIT"), today, today, today, terms), market,
                                 "Rate trade dates must be valid and start before maturity");

    auto invalidMarket = market;
    invalidMarket.valuationTime_ = Dal::DateTime_();
    AssertTradeValidationFailure(Trade(Dal::RateInstrumentType_("DEPOSIT"), today, today, maturity, terms), invalidMarket,
                                 "Rate cashflow plan requires a valid valuation time");

    for (const double contractRate : {0.0, -0.01}) {
        auto validTerms = terms;
        validTerms.contractRate_ = contractRate;
        const auto trade = Trade(Dal::RateInstrumentType_("DEPOSIT"), today, today, maturity, validTerms);
        const auto priced = Dal::PriceRateTrade(trade, market);
        const auto sensitivity = Dal::RateTradeNodeSensitivities(trade, market, "discount");
        ASSERT_TRUE(priced.succeeded_);
        ASSERT_TRUE(std::isfinite(priced.pv_));
        ASSERT_TRUE(sensitivity.eligible_);
        ASSERT_TRUE(std::isfinite(sensitivity.pv_));
        ASSERT_EQ(sensitivity.gradient_.size(), 1);
        ASSERT_TRUE(std::isfinite(sensitivity.gradient_[0]));
        ASSERT_TRUE(sensitivity.reason_.empty());
    }

    auto zeroPvTerms = terms;
    zeroPvTerms.contractRate_ = 0.0;
    const auto zeroPvTrade = Trade(Dal::RateInstrumentType_("DEPOSIT"), today, today, maturity, zeroPvTerms);
    const auto zeroPvSensitivity = Dal::RateTradeNodeSensitivities(zeroPvTrade, Market(today, FlatCurve(maturity, 0.0)), "discount");
    ASSERT_TRUE(zeroPvSensitivity.eligible_);
    ASSERT_DOUBLE_EQ(zeroPvSensitivity.pv_, 0.0);
    ASSERT_EQ(zeroPvSensitivity.gradient_.size(), 1);
    ASSERT_TRUE(std::isfinite(zeroPvSensitivity.gradient_[0]));
    ASSERT_TRUE(zeroPvSensitivity.reason_.empty());
}

TEST(RateCashflowPricingTest, TestDepositNodeAADReasonPrecedenceForCombinedFailures) {
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ maturity(2027, 1, 15);
    const auto supportedMarket = Market(today, FlatCurve(maturity));
    auto invalidTerms = DepositTerms();
    invalidTerms.notional_ = std::numeric_limits<double>::quiet_NaN();
    const auto invalidTrade = Trade(Dal::RateInstrumentType_("DEPOSIT"), today, today, maturity, invalidTerms);

    Dal::FraTradeTerms_ fraTerms;
    AssertCanonicalFailure(
        Dal::RateTradeNodeSensitivities(Trade(Dal::RateInstrumentType_("DEPOSIT"), today, today, maturity, fraTerms), supportedMarket, "discount"),
        "TRADE_FAMILY_NOT_AAD_ENABLED");
    AssertCanonicalFailure(
        Dal::RateTradeNodeSensitivities(Trade(Dal::RateInstrumentType_("FRA"), today, today, maturity, DepositTerms()), supportedMarket, "discount"),
        "TRADE_FAMILY_NOT_AAD_ENABLED");
    AssertCanonicalFailure(Dal::RateTradeNodeSensitivities(invalidTrade, supportedMarket, "wrong"), "TRADE_DOES_NOT_DEPEND_ON_COMPONENT");

    auto missingMarket = supportedMarket;
    missingMarket.curveComponents_.erase("discount");
    AssertCanonicalFailure(Dal::RateTradeNodeSensitivities(invalidTrade, missingMarket, "discount"), "CURVE_COMPONENT_UNAVAILABLE");
    auto nullMarket = supportedMarket;
    nullMarket.curveComponents_["discount"] = Dal::Handle_<Dal::DiscountCurve_>();
    AssertCanonicalFailure(Dal::RateTradeNodeSensitivities(invalidTrade, nullMarket, "discount"), "CURVE_COMPONENT_UNAVAILABLE");

    auto unsupportedMarket = supportedMarket;
    unsupportedMarket.curveComponents_["discount"] = Dal::Handle_<Dal::DiscountCurve_>(std::make_shared<const UnsupportedDiscountCurve_>());
    AssertCanonicalFailure(Dal::RateTradeNodeSensitivities(invalidTrade, unsupportedMarket, "discount"), "CURVE_REPRESENTATION_NOT_AAD_ENABLED");
    AssertCanonicalFailure(Dal::RateTradeNodeSensitivities(invalidTrade, supportedMarket, "discount"), "TRADE_VALIDATION_FAILED");

    const auto validTrade = Trade(Dal::RateInstrumentType_("DEPOSIT"), today, today, maturity, DepositTerms());
    AssertCanonicalFailure(Dal::RateTradeNodeSensitivities(validTrade, supportedMarket, "wrong"), "TRADE_DOES_NOT_DEPEND_ON_COMPONENT");
    AssertCanonicalFailure(Dal::RateTradeNodeSensitivities(validTrade, missingMarket, "discount"), "CURVE_COMPONENT_UNAVAILABLE");
    AssertCanonicalFailure(Dal::RateTradeNodeSensitivities(validTrade, unsupportedMarket, "discount"), "CURVE_REPRESENTATION_NOT_AAD_ENABLED");
}

TEST(RateCashflowPricingTest, TestDepositNodeAADPwcRawColumnsMatchCentralBumps) {
    constexpr double PWC_NATIVE_PARAMETER_BUMP = 1.0e-6;
    constexpr double PWC_RAW_GRADIENT_ABSOLUTE_TOLERANCE = 1.0e-6;
    constexpr double PWC_RAW_GRADIENT_RELATIVE_TOLERANCE = 1.0e-8;
    const Dal::Vector_<Dal::Date_> knots{Dal::Date_(2026, 7, 15), Dal::Date_(2027, 1, 15), Dal::Date_(2028, 1, 15)};
    const auto buildCurve = [&](const Dal::Vector_<>& parameters) {
        return Dal::Handle_<Dal::DiscountCurve_>(Dal::NewDiscountPWC("pwc", "USD", Dal::PiecewiseConstant_(knots, parameters)));
    };
    AssertRawNodeGradientMatchesCentralBumps(buildCurve, {0.01, -0.005, 0.03}, 3, PWC_NATIVE_PARAMETER_BUMP, PWC_RAW_GRADIENT_ABSOLUTE_TOLERANCE,
                                             PWC_RAW_GRADIENT_RELATIVE_TOLERANCE, {1, 2});
}

TEST(RateCashflowPricingTest, TestDepositNodeAADPwlfRawColumnsMatchCentralBumps) {
    constexpr double PWLF_NATIVE_PARAMETER_BUMP = 1.0e-6;
    constexpr double PWLF_RAW_GRADIENT_ABSOLUTE_TOLERANCE = 1.0e-6;
    constexpr double PWLF_RAW_GRADIENT_RELATIVE_TOLERANCE = 1.0e-8;
    const Dal::Vector_<Dal::Date_> knots{Dal::Date_(2026, 7, 15), Dal::Date_(2027, 1, 15), Dal::Date_(2028, 1, 15)};
    const auto buildCurve = [&](const Dal::Vector_<>& parameters) {
        Dal::Vector_<> left(3);
        Dal::Vector_<> right(3);
        for (int node = 0; node < 3; ++node) {
            left[node] = parameters[2 * node];
            right[node] = parameters[2 * node + 1];
        }
        return Dal::Handle_<Dal::DiscountCurve_>(Dal::NewDiscountPWLF("pwlf", "USD", Dal::PiecewiseLinear_(knots, left, right)));
    };
    AssertRawNodeGradientMatchesCentralBumps(buildCurve, {0.01, 0.0, -0.005, 0.02, 0.03, -0.01}, 6, PWLF_NATIVE_PARAMETER_BUMP,
                                             PWLF_RAW_GRADIENT_ABSOLUTE_TOLERANCE, PWLF_RAW_GRADIENT_RELATIVE_TOLERANCE, {3, 4, 5});
}

TEST(RateCashflowPricingTest, TestDepositNodeAADLogDiscountRawColumnsMatchCentralBumps) {
    constexpr double LOG_DISCOUNT_NATIVE_PARAMETER_BUMP = 1.0e-6;
    constexpr double LOG_DISCOUNT_RAW_GRADIENT_ABSOLUTE_TOLERANCE = 1.0e-6;
    constexpr double LOG_DISCOUNT_RAW_GRADIENT_RELATIVE_TOLERANCE = 1.0e-8;
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Vector_<Dal::Date_> knots{Dal::Date_(2026, 7, 15), Dal::Date_(2027, 1, 15), Dal::Date_(2028, 1, 15)};
    const auto buildCurve = [&](const Dal::Vector_<>& parameters) {
        Dal::Vector_<> stored{0.0};
        stored.Append(parameters);
        return Dal::Handle_<Dal::DiscountCurve_>(Dal::NewDiscountLogDF("logdf", "USD", Dal::Vector_<Dal::Date_>{today, knots[0], knots[1], knots[2]},
                                                                       stored, Dal::DayBasis_("ACT_365F"), Dal::LogDfScheme_::Value_::LOG_LINEAR));
    };
    AssertRawNodeGradientMatchesCentralBumps(buildCurve, {-0.005, 0.0, -0.06}, 3, LOG_DISCOUNT_NATIVE_PARAMETER_BUMP,
                                             LOG_DISCOUNT_RAW_GRADIENT_ABSOLUTE_TOLERANCE, LOG_DISCOUNT_RAW_GRADIENT_RELATIVE_TOLERANCE, {2});
}

TEST(RateCashflowPricingTest, TestDepositNodeAADZeroRateRawColumnsMatchCentralBumps) {
    constexpr double ZERO_RATE_NATIVE_PARAMETER_BUMP = 1.0e-6;
    constexpr double ZERO_RATE_RAW_GRADIENT_ABSOLUTE_TOLERANCE = 1.0e-6;
    constexpr double ZERO_RATE_RAW_GRADIENT_RELATIVE_TOLERANCE = 1.0e-8;
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Vector_<Dal::Date_> knots{Dal::Date_(2026, 7, 15), Dal::Date_(2027, 1, 15), Dal::Date_(2028, 1, 15)};
    const auto buildCurve = [&](const Dal::Vector_<>& parameters) {
        return Dal::Handle_<Dal::DiscountCurve_>(
            Dal::NewDiscountZeroRate("zero", "USD", today, knots, parameters, Dal::DayBasis_("ACT_365F"), Dal::LogDfScheme_::Value_::LOG_LINEAR));
    };
    AssertRawNodeGradientMatchesCentralBumps(buildCurve, {0.01, 0.0, -0.005}, 3, ZERO_RATE_NATIVE_PARAMETER_BUMP,
                                             ZERO_RATE_RAW_GRADIENT_ABSOLUTE_TOLERANCE, ZERO_RATE_RAW_GRADIENT_RELATIVE_TOLERANCE, {2});
}

TEST(RateCashflowPricingTest, TestDepositNodeAADRewindsPerTradeAndIsThreadLocal) {
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ maturity(2027, 1, 15);
    Dal::DepositTradeTerms_ terms;
    terms.notional_ = 100.0;
    terms.contractRate_ = 0.05;
    terms.lend_ = true;
    terms.index_ = QuarterlyIndex();
    terms.discountComponentKey_ = "discount";
    const auto trade = Trade(Dal::RateInstrumentType_("DEPOSIT"), today, today, maturity, terms);
    const auto evaluate = [=](double rate) { return Dal::RateTradeNodeSensitivities(trade, Market(today, FlatCurve(maturity, rate)), "discount"); };

    const auto first = evaluate(0.04);
    auto invalidTerms = terms;
    invalidTerms.notional_ = std::numeric_limits<double>::quiet_NaN();
    const auto invalidTrade = Trade(Dal::RateInstrumentType_("DEPOSIT"), today, today, maturity, invalidTerms);
    const auto invalid = Dal::RateTradeNodeSensitivities(invalidTrade, Market(today, FlatCurve(maturity, 0.04)), "discount");
    const auto repeated = evaluate(0.04);
    const auto repeatedAgain = evaluate(0.04);
    std::atomic<int> waiting{0};
    std::promise<void> release;
    const auto startTogether = release.get_future().share();
    auto left = std::async(std::launch::async, [&]() {
        ++waiting;
        startTogether.wait();
        return Dal::RateTradeNodeSensitivities(invalidTrade, Market(today, FlatCurve(maturity, 0.03)), "discount");
    });
    auto right = std::async(std::launch::async, [&]() {
        ++waiting;
        startTogether.wait();
        return evaluate(0.06);
    });
    while (waiting.load() != 2)
        std::this_thread::yield();
    release.set_value();
    const auto leftResult = left.get();
    const auto rightResult = right.get();

    ASSERT_TRUE(first.eligible_);
    AssertCanonicalFailure(invalid, "TRADE_VALIDATION_FAILED");
    ASSERT_TRUE(repeated.eligible_);
    ASSERT_TRUE(repeatedAgain.eligible_);
    AssertCanonicalFailure(leftResult, "TRADE_VALIDATION_FAILED");
    ASSERT_TRUE(rightResult.eligible_);
    ASSERT_DOUBLE_EQ(first.pv_, repeated.pv_);
    ASSERT_DOUBLE_EQ(first.pv_, repeatedAgain.pv_);
    ASSERT_EQ(first.gradient_, repeated.gradient_);
    ASSERT_EQ(first.gradient_, repeatedAgain.gradient_);
    ASSERT_EQ(first.reason_, repeated.reason_);
    ASSERT_EQ(first.reason_, repeatedAgain.reason_);
    const auto rightControl = evaluate(0.06);
    ASSERT_EQ(rightResult.pv_, rightControl.pv_);
    ASSERT_EQ(rightResult.gradient_, rightControl.gradient_);
}

TEST(RateCashflowPricingTest, TestFraAndFutureFormulas) {
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ start(2026, 4, 15);
    const Dal::Date_ maturity(2026, 7, 15);
    const auto curve = FlatCurve(maturity);
    const auto market = Market(today, curve);
    const auto index = QuarterlyIndex();
    const double accrual = index.dayBasis_(start, maturity, nullptr);
    const double forward = (1.0 / (*curve)(start, maturity) - 1.0) / accrual;

    Dal::FraTradeTerms_ fra;
    fra.notional_ = 2'000'000.0;
    fra.contractRate_ = 0.03;
    fra.receiveFloating_ = true;
    fra.settleAtStart_ = true;
    fra.index_ = index;
    fra.fixingIdentity_ = {"USD-LIBOR-3M", 11, 0};
    fra.forecastComponentKey_ = "forecast";
    fra.discountComponentKey_ = "discount";
    const auto fraResult = Dal::PriceRateTrade(Trade(Dal::RateInstrumentType_("FRA"), today, start, maturity, fra), market);
    const double payoff = fra.notional_ * accrual * (forward - fra.contractRate_) / (1.0 + accrual * forward);
    ASSERT_NEAR(fraResult.pv_, payoff * (*curve)(today, start), 1.0e-8);

    Dal::FutureTradeTerms_ future;
    future.contractCount_ = 12.0;
    future.long_ = true;
    future.referencePrice_ = 95.0;
    future.contractValuePerPricePoint_ = 25.0;
    future.convexityAdjustment_ = 0.0005;
    future.index_ = index;
    future.fixingIdentity_ = {"USD-LIBOR-3M", 11, 0};
    future.forecastComponentKey_ = "forecast";
    const auto futureResult = Dal::PriceRateTrade(Trade(Dal::RateInstrumentType_("FUTURE"), today, start, maturity, future), market);
    const double modelPrice = 100.0 * (1.0 - forward + future.convexityAdjustment_);
    ASSERT_NEAR(futureResult.pv_, 12.0 * 25.0 * (modelPrice - 95.0), 1.0e-10);

    future.long_ = false;
    const auto shortResult = Dal::PriceRateTrade(Trade(Dal::RateInstrumentType_("FUTURE"), today, start, maturity, future), market);
    ASSERT_NEAR(shortResult.pv_, -futureResult.pv_, 1.0e-10);
}

TEST(RateCashflowPricingTest, TestFloatingFamilySidesAreExactOpposites) {
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ maturity(2028, 1, 15);
    const auto curve = FlatCurve(maturity);
    const auto market = Market(today, curve);

    Dal::FixedFloatTradeTerms_ fixedFloat;
    fixedFloat.notional_ = 1'000'000.0;
    fixedFloat.contractRate_ = 0.03;
    fixedFloat.payFixed_ = true;
    fixedFloat.fixedLeg_ = AnnualLeg();
    fixedFloat.floatLeg_ = AnnualLeg();
    fixedFloat.floatIndex_ = QuarterlyIndex();
    fixedFloat.fixingIdentity_ = {"USD-SOFR", 11, 0};
    fixedFloat.forecastComponentKey_ = "forecast";
    fixedFloat.discountComponentKey_ = "discount";
    for (const auto& family : {"OIS", "IRS"}) {
        const auto type = Dal::RateInstrumentType_(family);
        const Dal::RateTradeTerms_ first = family == std::string("OIS") ? Dal::RateTradeTerms_(Dal::OisTradeTerms_{fixedFloat})
                                                                        : Dal::RateTradeTerms_(Dal::IrsTradeTerms_{fixedFloat});
        const double receive = Dal::PriceRateTrade(Trade(type, today, today, maturity, first), market).pv_;
        fixedFloat.payFixed_ = false;
        const Dal::RateTradeTerms_ opposite = family == std::string("OIS") ? Dal::RateTradeTerms_(Dal::OisTradeTerms_{fixedFloat})
                                                                           : Dal::RateTradeTerms_(Dal::IrsTradeTerms_{fixedFloat});
        const double pay = Dal::PriceRateTrade(Trade(type, today, today, maturity, opposite), market).pv_;
        ASSERT_NEAR(receive, -pay, 1.0e-10);
        fixedFloat.payFixed_ = true;
    }

    Dal::BasisTradeTerms_ basis;
    basis.notional_ = 1'000'000.0;
    basis.contractSpread_ = 0.001;
    basis.receiveReferencePaySpread_ = true;
    basis.spreadLeg_ = AnnualLeg();
    basis.referenceLeg_ = AnnualLeg();
    basis.spreadIndex_ = QuarterlyIndex();
    basis.referenceIndex_ = QuarterlyIndex();
    basis.spreadFixingIdentity_ = {"USD-LIBOR-3M", 11, 0};
    basis.referenceFixingIdentity_ = {"USD-SOFR", 11, 0};
    basis.spreadForecastComponentKey_ = "forecast";
    basis.referenceForecastComponentKey_ = "reference";
    basis.discountComponentKey_ = "discount";
    const auto first = Dal::PriceRateTrade(Trade(Dal::RateInstrumentType_("BASIS_SWAP"), today, today, maturity, basis), market);
    basis.receiveReferencePaySpread_ = false;
    const auto opposite = Dal::PriceRateTrade(Trade(Dal::RateInstrumentType_("BASIS_SWAP"), today, today, maturity, basis), market);
    ASSERT_NEAR(first.pv_, -opposite.pv_, 1.0e-10);
}

TEST(RateCashflowPricingTest, TestXccySpreadLegSidesAndPositionCount) {
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ maturity(2028, 1, 15);
    const auto usd = FlatCurve(maturity, 0.04, "USD");
    const auto eur = FlatCurve(maturity, 0.04, "EUR");
    const auto collateral = Dal::CollateralType_(Dal::CollateralType_::Value_::OIS);
    const auto usdBlock = Dal::Handle_<Dal::CurveBlock_>(new Dal::CurveBlock_("usd", "USD", {{collateral, usd}}, {}, Dal::DayBasis_("ACT_365F")));
    const auto eurBlock = Dal::Handle_<Dal::CurveBlock_>(new Dal::CurveBlock_("eur", "EUR", {{collateral, eur}}, {}, Dal::DayBasis_("ACT_365F")));
    const auto fixings = Dal::Handle_<Dal::MarketFixingSnapshot_>(new Dal::MarketFixingSnapshot_());
    const auto xccyMarket =
        std::make_shared<Dal::CrossCurrencyMarket_>(usdBlock, eurBlock, 1.2, Dal::DateTime_(today, 10, 30), Dal::Ccy_("USD"), fixings);

    Dal::XccyTradeTerms_ terms;
    terms.positionCount_ = 2.0;
    terms.contractSpread_ = 0.001;
    terms.spreadOnForeignLeg_ = true;
    terms.receiveNonSpreadPaySpread_ = true;
    terms.config_.pair_ = Dal::CurrencyPair_(Dal::Ccy_("USD"), Dal::Ccy_("EUR"));
    terms.config_.domesticNotional_ = 120.0;
    terms.config_.foreignNotional_ = 100.0;
    terms.config_.convention_.domesticLeg_ = AnnualLeg();
    terms.config_.convention_.foreignLeg_ = AnnualLeg();
    terms.config_.convention_.domesticIndex_ = QuarterlyIndex();
    terms.config_.convention_.foreignIndex_ = QuarterlyIndex();
    terms.config_.convention_.spreadOnForeignLeg_ = true;
    terms.config_.domesticRateFixing_ = {"USD-SOFR", 11, 0};
    terms.config_.foreignRateFixing_ = {"EUR-ESTR", 11, 0};

    Dal::RatePricingMarket_ market;
    market.valuationTime_ = Dal::DateTime_(today, 10, 30);
    market.resultCurrency_ = Dal::Ccy_("USD");
    market.xccyMarket_ = xccyMarket;
    market.fixings_ = fixings;
    const auto trade = Trade(Dal::RateInstrumentType_("XCCY"), today, today, maturity, terms);
    const auto receive = Dal::PriceRateTrade(trade, market);
    ASSERT_TRUE(receive.succeeded_);

    terms.receiveNonSpreadPaySpread_ = false;
    const auto pay = Dal::PriceRateTrade(Trade(Dal::RateInstrumentType_("XCCY"), today, today, maturity, terms), market);
    ASSERT_TRUE(pay.succeeded_);
    ASSERT_NEAR(receive.pv_, -pay.pv_, 1.0e-10);

    terms.positionCount_ = 1.0;
    terms.receiveNonSpreadPaySpread_ = true;
    const auto single = Dal::PriceRateTrade(Trade(Dal::RateInstrumentType_("XCCY"), today, today, maturity, terms), market);
    ASSERT_TRUE(single.succeeded_);
    ASSERT_NEAR(receive.pv_, 2.0 * single.pv_, 1.0e-10);
}

TEST(RateCashflowPricingTest, TestFixingAndPaymentBoundaries) {
    const Dal::Date_ start(2026, 1, 15);
    const Dal::Date_ maturity(2026, 4, 15);
    const auto curve = FlatCurve(maturity);
    Dal::FraTradeTerms_ terms;
    terms.notional_ = 1'000'000.0;
    terms.contractRate_ = 0.03;
    terms.receiveFloating_ = true;
    terms.settleAtStart_ = false;
    terms.index_ = QuarterlyIndex();
    terms.fixingIdentity_ = {"USD-LIBOR-3M", 11, 0};
    terms.forecastComponentKey_ = "forecast";
    terms.discountComponentKey_ = "discount";
    const auto trade = Trade(Dal::RateInstrumentType_("FRA"), start, start, maturity, terms);

    auto missingMarket = Market(start, curve);
    missingMarket.valuationTime_ = Dal::DateTime_(start.AddDays(1), 10, 30);
    const auto missing = Dal::PriceRateTrade(trade, missingMarket);
    ASSERT_FALSE(missing.succeeded_);
    ASSERT_EQ(missing.requiredHistoricalFixings_.size(), 1);
    ASSERT_EQ(missing.missingHistoricalFixings_.size(), 1);

    auto atFixing = Market(start, curve);
    atFixing.valuationTime_ = Dal::DateTime_(start, 11, 0);
    const auto forecast = Dal::PriceRateTrade(trade, atFixing);
    ASSERT_TRUE(forecast.succeeded_);
    ASSERT_TRUE(forecast.requiredHistoricalFixings_.empty());

    Dal::MarketFixingSnapshot_::values_t history;
    history["USD-LIBOR-3M"][Dal::DateTime_(start, 11, 0)] = -0.001;
    auto atPayment = Market(start, curve);
    atPayment.valuationTime_ = Dal::DateTime_(maturity, 10, 30);
    atPayment.fixings_ = Dal::Handle_<Dal::MarketFixingSnapshot_>(new Dal::MarketFixingSnapshot_(history));
    const auto payment = Dal::PriceRateTrade(trade, atPayment);
    ASSERT_TRUE(payment.succeeded_);
    const double accrual = terms.index_.dayBasis_(start, maturity, nullptr);
    ASSERT_NEAR(payment.pv_, terms.notional_ * accrual * (-0.001 - terms.contractRate_), 1.0e-10);

    auto expired = Market(start, curve);
    expired.valuationTime_ = Dal::DateTime_(maturity.AddDays(1), 10, 30);
    const auto paid = Dal::PriceRateTrade(trade, expired);
    ASSERT_TRUE(paid.succeeded_);
    ASSERT_DOUBLE_EQ(paid.pv_, 0.0);
    ASSERT_TRUE(paid.requiredHistoricalFixings_.empty());
}

TEST(RateCashflowPricingTest, TestOisPlanRecordsEachDailyHistoricalObservation) {
    const Dal::Date_ start(2026, 1, 15);
    const Dal::Date_ maturity(2026, 4, 15);
    Dal::FixedFloatTradeTerms_ fixedFloat;
    fixedFloat.notional_ = 1'000'000.0;
    fixedFloat.contractRate_ = 0.03;
    fixedFloat.payFixed_ = true;
    fixedFloat.fixedLeg_ = AnnualLeg();
    fixedFloat.floatLeg_ = AnnualLeg();
    fixedFloat.floatIndex_ = QuarterlyIndex();
    fixedFloat.fixingIdentity_ = {"USD-SOFR", 11, 0};
    fixedFloat.forecastComponentKey_ = "forecast";
    fixedFloat.discountComponentKey_ = "discount";
    const auto trade = Trade(Dal::RateInstrumentType_("OIS"), start, start, maturity, Dal::OisTradeTerms_{fixedFloat});

    const auto plan = Dal::BuildRateCashflowPlan(trade, Dal::DateTime_(start.AddDays(2), 10, 30));

    ASSERT_EQ(plan.requiredHistoricalFixings_.size(), 2);
    ASSERT_EQ(plan.requiredHistoricalFixings_[0].fixingTime_, Dal::DateTime_(start, 11, 0));
    ASSERT_EQ(plan.requiredHistoricalFixings_[1].fixingTime_, Dal::DateTime_(start.AddDays(1), 11, 0));
}

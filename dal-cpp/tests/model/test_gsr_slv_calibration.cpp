//
// Created by Codex on 2026/10/2.
//

#include <gtest/gtest.h>

#include <dal/platform/platform.hpp>
#include <dal/model/gsrslvpricing.hpp>
#include <dal/model/gsrslvcalibration.hpp>
#include <dal/model/gsrmarketcalibration.hpp>
#include <dal/math/specialfunctions.hpp>
#include <dal/math/distribution/black.hpp>
#include <dal/model/gsrslvpricinginternal.hpp>
#include <dal/model/gsrslv.hpp>

using namespace Dal;

namespace {
    const Date_ TODAY(2026, 10, 2);

    GSRSLVModelData_ Model(double leverage = 1.0, double eta = 0.0, int factors = 1) {
        const Handle_<GSRCurveData_> curve(new GSRCurveData_("curve", TODAY, "USD", {TODAY, TODAY.AddDays(1825)}, {0.0, -0.15}, {}, Matrix_<>(0, 0)));
        MultiFactorGSRVolSettings_ vol;
        vol.gKnotDates_ = vol.hKnotDates_ = {TODAY};
        vol.gValues_ = Matrix_<>(factors, 1, 0.02 / factors);
        vol.hValues_ = Matrix_<>(factors, 1, 1.0);
        vol.correlations_ = Matrix_<>(factors, factors, 0.25);
        for (int i = 0; i < factors; ++i) {
            vol.factorNames_.push_back("factor" + String::FromInt(i));
            vol.correlations_(i, i) = 1.0;
        }
        const Handle_<MultiFactorGSRModelData_> gaussian(
            new MultiFactorGSRModelData_("rates", curve, Handle_<MultiFactorGSRVolData_>(new MultiFactorGSRVolData_("vol", vol))));
        GSRSLVSettings_ settings;
        settings.volOfVol_ = eta;
        settings.maxStep_ = 0.25;
        return GSRSLVModelData_("smile", gaussian,
                                Handle_<GSRLeverageData_>(new GSRLeverageData_("leverage", {0.0}, {0.0}, Matrix_<>(1, 1, leverage))), settings);
    }

    EuropeanRateOption_ Bond(double strike = 0.97) { return BondOption_{TODAY.AddDays(365), TODAY.AddDays(730), strike, OptionType_("CALL")}; }
} // namespace

TEST(GSRSLVCalibrationTest, TestMonteCarloMatchesIndependentGaussianPrices) {
    for (int factors = 1; factors <= 3; ++factors) {
        const auto model = Model(1.0, 0.0, factors);
        const Vector_<EuropeanRateOption_> options{Bond(0.94), Bond(0.97), Bond(1.0)};
        const auto prices = PriceGSRSLVEuropeanOptions(model, options, {20000, 1729});
        ASSERT_EQ(prices.size(), options.size());
        for (size_t i = 0; i < options.size(); ++i) {
            const double analytic = PriceGSREuropeanOption(*model.gaussian_, options[i]).price_;
            ASSERT_NEAR(prices[i].price_, analytic, 5.0 * prices[i].standardError_ + 1e-6);
            ASSERT_GT(prices[i].standardError_, 0.0);
        }
    }
}

TEST(GSRSLVCalibrationTest, TestCapletAndOnePeriodSwaptionSharePaths) {
    const auto expiry = TODAY.AddDays(365), end = TODAY.AddDays(730);
    const Caplet_ caplet{expiry, expiry, end, end, 1.0, 1.0, "12M", 0.03, OptionType_("CALL")};
    const Swaption_ swaption{expiry, {{end, 1.0}}, {{expiry, expiry, end, end, 1.0, 1.0, "12M"}}, 0.03, OptionType_("CALL")};
    const auto result = PriceGSRSLVEuropeanOptions(Model(1.0, 0.6), {caplet, swaption}, {1024, 1729});
    ASSERT_NEAR(result[0].price_, result[1].price_, 1e-14);
    ASSERT_NEAR(result[0].standardError_, result[1].standardError_, 1e-14);
}

TEST(GSRSLVCalibrationTest, TestPaymentLagAndFutureCouponsMatchGaussianLimit) {
    const auto expiry = TODAY.AddDays(365), middle = TODAY.AddDays(730), end = TODAY.AddDays(1095);
    const Caplet_ lagged{expiry, expiry, middle, middle.AddDays(90), 1.0, 0.75, "12M", 0.03, OptionType_("CALL")};
    const Swaption_ swaption{expiry,
                             {{middle, 1.0}, {end, 1.0}},
                             {{expiry, expiry, middle, middle, 1.0, 1.0, "12M"}, {middle, middle, end, end, 1.0, 1.0, "12M"}},
                             0.03,
                             OptionType_("CALL")};
    const auto model = Model();
    const Vector_<EuropeanRateOption_> options{lagged, swaption};
    const auto prices = PriceGSRSLVEuropeanOptions(model, options, {20000, 1729});
    for (size_t i = 0; i < options.size(); ++i)
        ASSERT_NEAR(prices[i].price_, PriceGSREuropeanOption(*model.gaussian_, options[i]).price_, 5.0 * prices[i].standardError_ + 1e-6);
    const BondOption_ immediate{TODAY, middle, 0.9, OptionType_("CALL")};
    const auto result = PriceGSRSLVEuropeanOptions(model, {immediate}, {4, 1729})[0];
    ASSERT_NEAR(result.price_, std::exp(-0.06) - 0.9, 1e-14);
    ASSERT_DOUBLE_EQ(result.standardError_, 0.0);
}

TEST(GSRSLVCalibrationTest, TestRejectsHistoricalCouponAndMalformedInputs) {
    const auto fixing = TODAY.AddDays(730), end = TODAY.AddDays(1095);
    const Swaption_ lagged{
        TODAY.AddDays(365), {{end, 1.0}}, {{TODAY.AddDays(-2), fixing, end, end.AddDays(2), 1.0, 1.0, "12M"}}, 0.03, OptionType_("CALL")};
    ASSERT_THROW(PriceGSRSLVEuropeanOptions(Model(), {lagged}), Exception_);
    ASSERT_THROW(PriceGSRSLVEuropeanOptions(Model(), {Bond()}, {3, 1729}), Exception_);
    ASSERT_THROW(PriceGSRSLVEuropeanOptions(Model(), {Bond()}, {1024, -1}), Exception_);
    ASSERT_THROW(PriceGSRSLVEuropeanOptions(Model(), {}), Exception_);
    const Caplet_ overflowing{TODAY.AddDays(365), TODAY.AddDays(365), TODAY.AddDays(730), TODAY.AddDays(730), 1e-308, 1e308, "12M", 0.03,
                              OptionType_("CALL")};
    ASSERT_THROW(PriceGSRSLVEuropeanOptions(Model(), {overflowing}, {4, 1729}), Exception_);
}

TEST(GSRSLVCalibrationTest, TestConditionalLaggedSwaptionMatchesGaussianAndParity) {
    const auto exercise = TODAY.AddDays(365), fixing = TODAY.AddDays(540), start = fixing.AddDays(2), end = start.AddDays(365), pay = end.AddDays(2);
    Swaption_ payer{exercise, {{pay, 1.0}}, {{fixing, start, end, pay, 1.0, 1.0, "12M"}}, 0.03, OptionType_("CALL")};
    auto receiver = payer;
    receiver.type_ = OptionType_("PUT");
    const GSRMonteCarloSettings_ settings{4096, 1729, 64};
    for (int factors = 1; factors <= 3; ++factors) {
        const auto model = Model(1.0, 0.0, factors);
        const auto prices = PriceGSRSLVEuropeanOptions(model, {payer, receiver}, settings);
        const auto repeat = PriceGSRSLVEuropeanOptions(model, {payer, receiver}, settings);
        for (size_t i = 0; i < prices.size(); ++i) {
            const auto& option = i == 0 ? payer : receiver;
            ASSERT_NEAR(prices[i].price_, PriceGSREuropeanOption(*model.gaussian_, option).price_,
                        5.0 * prices[i].standardError_ + 4.0 * prices[i].conditionalError_ + 2e-5);
            ASSERT_GE(prices[i].conditionalError_, 0.0);
            ASSERT_DOUBLE_EQ(prices[i].price_, repeat[i].price_);
        }
        const double analyticParity =
            PriceGSREuropeanOption(*model.gaussian_, payer).price_ - PriceGSREuropeanOption(*model.gaussian_, receiver).price_;
        ASSERT_NEAR(prices[0].price_ - prices[1].price_, analyticParity, 5.0 * (prices[0].standardError_ + prices[1].standardError_) + 2e-5);
    }
    ASSERT_THROW(PriceGSRSLVEuropeanOptions(Model(), {payer}, {4, 1729, 6}), Exception_);
}

TEST(GSRSLVCalibrationTest, TestKnownFixingRetainedBeforeExercise) {
    const auto fixing = TODAY.AddDays(180), exercise = TODAY.AddDays(365), end = TODAY.AddDays(730);
    const Swaption_ known{exercise, {{end, 1.0}}, {{fixing, fixing, end, end, 1.0, 1.0, "12M"}}, 0.03, OptionType_("CALL")};
    const auto price = PriceGSRSLVEuropeanOptions(Model(), {known}, {8192, 1729, 4})[0];
    const Caplet_ reference{fixing, fixing, end, end, 1.0, 1.0, "12M", 0.03, OptionType_("CALL")};
    ASSERT_NEAR(price.price_, PriceGSREuropeanOption(*Model().gaussian_, reference).price_, 5.0 * price.standardError_ + 1e-5);
    ASSERT_DOUBLE_EQ(price.conditionalError_, 0.0);
}

TEST(GSRMarketCalibrationTest, TestVolatilityConversionOraclesAndInvalidInputs) {
    const auto model = Model();
    const auto exercise = TODAY.AddDays(365), end = TODAY.AddDays(730);
    const double forward = std::expm1(0.03), annuity = std::exp(-0.06);
    const Caplet_ caplet{exercise, exercise, end, end, 1.0, 1.0, "12M", forward, OptionType_("CALL")};
    VolQuote_ normal{"normal", caplet, 0.01, 0.001};
    const auto value = ConvertVolQuotes(*model.gaussian_->curve_, {normal})[0];
    ASSERT_NEAR(value.forward_, forward, 1e-14);
    ASSERT_NEAR(value.annuity_, annuity, 1e-14);
    ASSERT_NEAR(value.price_, annuity * 0.01 / std::sqrt(2.0 * 3.141592653589793), 1e-14);
    ASSERT_NEAR(value.vega_, value.price_ / 0.01, 1e-14);
    auto zero = normal;
    auto atMoney = caplet;
    atMoney.strike_ = value.forward_;
    zero.option_ = atMoney;
    zero.volatility_ = 0.0;
    ASSERT_NEAR(ConvertVolQuotes(*model.gaussian_->curve_, {zero})[0].vega_, annuity / std::sqrt(2.0 * 3.141592653589793), 1e-14);
    auto shifted = normal;
    shifted.convention_ = VolConvention_("SHIFTED_BLACK");
    shifted.shift_ = 0.02;
    shifted.volatility_ = 0.3;
    const auto black = ConvertVolQuotes(*model.gaussian_->curve_, {shifted})[0];
    ASSERT_NEAR(black.price_, annuity * (forward + 0.02) * (2.0 * NCDF(0.15) - 1.0), 1e-14);
    shifted.shift_ = -0.04;
    ASSERT_THROW(ConvertVolQuotes(*model.gaussian_->curve_, {shifted}), Exception_);
    ASSERT_THROW(ConvertVolQuotes(*model.gaussian_->curve_, {normal, normal}), Exception_);
    ASSERT_THROW(VolConvention_ bad("invalid"), Exception_);
    normal.convention_ = VolConvention_("NORMAL");
    normal.option_ = Bond();
    ASSERT_THROW(ConvertVolQuotes(*model.gaussian_->curve_, {normal}), Exception_);
}

TEST(GSRSLVCalibrationTest, TestAADPriceJacobianMatchesFrozenPathBumps) {
    const auto data = Model(1.1, 0.6);
    const Vector_<EuropeanRateOption_> options{Bond(0.94), Bond(0.97), Bond(1.0)};
    const GSRMonteCarloSettings_ settings{512, 1729};
    const GSRSLVPricingInternal::PreparedPricer_ pricer(data, options, settings);
    const Vector_<String_> labels{"kappa", "volOfVol", "leverage:0:0"};
    const auto jacobian = pricer.Jacobian(data, labels);
    ASSERT_EQ(jacobian.Rows(), 3);
    ASSERT_EQ(jacobian.Cols(), 3);
    for (int col = 0; col < 3; ++col) {
        GSRSLVSettings_ low{data.kappa_, data.volOfVol_, data.varianceCorrelations_, data.maxStep_}, high = low;
        double lowLeverage = 1.1, highLeverage = 1.1;
        if (col == 0) {
            low.kappa_ -= 1e-5;
            high.kappa_ += 1e-5;
        }
        if (col == 1) {
            low.volOfVol_ -= 1e-5;
            high.volOfVol_ += 1e-5;
        }
        if (col == 2) {
            lowLeverage -= 1e-5;
            highLeverage += 1e-5;
        }
        const GSRSLVModelData_ lowData("low", data.gaussian_, Model(lowLeverage).leverage_, low),
            highData("high", data.gaussian_, Model(highLeverage).leverage_, high);
        const auto down = pricer.Price(lowData), up = pricer.Price(highData);
        for (int row = 0; row < 3; ++row)
            ASSERT_NEAR(jacobian(row, col), (up[row].price_ - down[row].price_) / 2e-5, 2e-7) << labels[col];
    }
    const auto repeat = pricer.Jacobian(data, labels);
    for (int row = 0; row < 3; ++row)
        for (int col = 0; col < 3; ++col)
            ASSERT_DOUBLE_EQ(jacobian(row, col), repeat(row, col));
    ASSERT_THROW(pricer.Jacobian(data, {"invalid"}), Exception_);
    ASSERT_THROW(pricer.Jacobian(data, {"leverage:0:0", "leverage:0:0"}), Exception_);
    const AAD::GSRSLV_<> kernel(data);
    ASSERT_THROW(pricer.Jacobian(data, {kernel.ParameterLabels().front()}), Exception_);
}

TEST(GSRMarketCalibrationTest, TestSwaptionProjectionAnnuityParityAndVega) {
    Matrix_<> projection(1, 2);
    projection(0, 0) = 0.0;
    projection(0, 1) = -0.20;
    const GSRCurveData_ curve("dual", TODAY, "USD", {TODAY, TODAY.AddDays(1825)}, {0.0, -0.15}, {"12M"}, projection);
    const auto exercise = TODAY.AddDays(365), middle = TODAY.AddDays(730), end = TODAY.AddDays(1095);
    const double annuity = 0.9 * std::exp(-0.06) + 1.1 * std::exp(-0.09);
    const double forward = std::expm1(0.04) * (0.8 * std::exp(-0.06) + 0.7 * std::exp(-0.09)) / annuity;
    const Swaption_ payer{exercise,
                          {{middle, 0.9}, {end, 1.1}},
                          {{exercise, exercise, middle, middle, 1.0, 0.8, "12M"}, {middle, middle, end, end, 1.0, 0.7, "12M"}},
                          forward + 0.005,
                          OptionType_("CALL")};
    auto receiver = payer;
    receiver.type_ = OptionType_("PUT");
    for (const auto& convention : {VolConvention_("NORMAL"), VolConvention_("BLACK"), VolConvention_("SHIFTED_BLACK")}) {
        const double shift = convention == VolConvention_::Value_::SHIFTED_BLACK ? 0.02 : 0.0;
        const double vol = convention == VolConvention_::Value_::NORMAL ? 0.01 : 0.3;
        VolQuote_ call{"payer", payer, vol, 0.001, convention, shift}, put{"receiver", receiver, vol, 0.001, convention, shift};
        const auto prices = ConvertVolQuotes(curve, {call, put});
        ASSERT_NEAR(prices[0].forward_, forward, 1e-14);
        ASSERT_NEAR(prices[0].annuity_, annuity, 1e-14);
        ASSERT_NEAR(prices[0].price_ - prices[1].price_, -annuity * 0.005, 1e-14);
        auto high = call, low = call;
        high.volatility_ += 1e-6;
        low.volatility_ -= 1e-6;
        const double derivative = (ConvertVolQuotes(curve, {high})[0].price_ - ConvertVolQuotes(curve, {low})[0].price_) / 2e-6;
        ASSERT_NEAR(prices[0].vega_, derivative, 1e-8);
    }
    VolQuote_ bad{"bad", payer, std::numeric_limits<double>::quiet_NaN(), 0.001};
    ASSERT_THROW(ConvertVolQuotes(curve, {bad}), Exception_);
    bad.volatility_ = 0.01;
    bad.priceScale_ = 0.0;
    ASSERT_THROW(ConvertVolQuotes(curve, {bad}), Exception_);
}

TEST(GSRMarketCalibrationTest, TestMultiExpiryTenorStrikeFitAndHeldOutVolatilities) {
    const auto base = Model(1.0, 0.6);
    const GSRSLVSettings_ dynamics{base.kappa_, base.volOfVol_, base.varianceCorrelations_, base.maxStep_};
    Matrix_<> levels(1, 3);
    levels(0, 0) = 1.1;
    levels(0, 1) = 1.3;
    levels(0, 2) = 0.9;
    const GSRSLVModelData_ truth("truth", base.gaussian_, Handle_<GSRLeverageData_>(new GSRLeverageData_("truth", {0.0}, {0.0, 1.0, 2.0}, levels)),
                                 dynamics);
    const GSRSLVModelData_ initial("initial", base.gaussian_,
                                   Handle_<GSRLeverageData_>(new GSRLeverageData_("initial", {0.0}, {0.0, 1.0, 2.0}, Matrix_<>(1, 3, 1.0))),
                                   dynamics);
    GSRSLVCalibrationSettings_ settings;
    settings.pricing_.paths_ = 32768;
    settings.validation_.paths_ = 65536;
    settings.solver_.gradientTolerance_ = 1e-8;
    Vector_<VolQuote_> quotes, heldOut;
    const auto add = [&](int expiry, int tenor, double strikeOffset, Vector_<VolQuote_>* output) {
        const int days = tenor == 1 ? 182 : 365;
        const double accrual = days / DAYS_PER_YEAR, forward = std::expm1(0.03 * accrual) / accrual;
        const auto exercise = TODAY.AddDays(365 * expiry), end = exercise.AddDays(days);
        const Caplet_ option{exercise,           exercise, end, end, accrual, accrual, String::FromInt(6 * tenor) + "M", forward + strikeOffset,
                             OptionType_("CALL")};
        output->push_back(
            {"quote" + String::FromInt(expiry) + ":" + String::FromInt(tenor) + ":" + String::FromDouble(strikeOffset), option, 0.0, 0.003});
    };
    for (int expiry = 1; expiry <= 3; ++expiry)
        for (int tenor = 1; tenor <= 2; ++tenor) {
            for (double strike : {-0.005, 0.0, 0.005})
                add(expiry, tenor, strike, &quotes);
            add(expiry, tenor, 0.002, &heldOut);
        }
    const auto setVolatilities = [&](Vector_<VolQuote_>* output) {
        Vector_<EuropeanRateOption_> options;
        for (const auto& quote : *output)
            options.push_back(quote.option_);
        const auto prices = PriceGSRSLVEuropeanOptions(truth, options, settings.pricing_);
        const auto market = ConvertVolQuotes(*base.gaussian_->curve_, *output);
        for (size_t i = 0; i < output->size(); ++i) {
            auto& quote = (*output)[i];
            const auto& option = std::get<Caplet_>(quote.option_);
            quote.volatility_ = Distribution::BachelierIV(market[i].forward_, option.strike_, option.type_, prices[i].price_ / market[i].annuity_) /
                                std::sqrt((option.expiry_ - TODAY) / DAYS_PER_YEAR);
        }
    };
    setVolatilities(&quotes);
    setVolatilities(&heldOut);
    const Vector_<GSRSLVCalibrationParameter_> parameters{
        {"leverage:0:0", 0.2, 2.0, 1.0}, {"leverage:0:1", 0.2, 2.0, 1.0}, {"leverage:0:2", 0.2, 2.0, 1.0}};
    const auto fd = CalibrateGSRSLVMarket(initial, quotes, parameters, settings, heldOut);
    settings.useAADJacobian_ = true;
    const auto aad = CalibrateGSRSLVMarket(initial, quotes, parameters, settings, heldOut);
    ASSERT_TRUE(fd.converged_) << fd.terminationReason_;
    ASSERT_TRUE(aad.converged_) << aad.terminationReason_;
    ASSERT_EQ(aad.jacobianRank_, 3);
    ASSERT_TRUE(aad.fitWithinTolerance_);
    const size_t worst = std::max_element(aad.numericalErrors_.begin(), aad.numericalErrors_.end()) - aad.numericalErrors_.begin();
    ASSERT_TRUE(aad.numericalValidationPassed_) << "error=" << aad.numericalErrors_[worst]
                                                << " difference=" << std::abs(aad.validationPrices_[worst] - aad.modelPrices_[worst])
                                                << " fit SE=" << aad.standardErrors_[worst] << " fine SE=" << aad.validationStandardErrors_[worst];
    ASSERT_TRUE(aad.heldOutWithinTolerance_);
    ASSERT_EQ(aad.heldOutPrices_.size(), 6U);
    for (int i = 0; i < 3; ++i) {
        ASSERT_NEAR(aad.parameters_[i], levels(0, i), 5e-4);
        ASSERT_NEAR(aad.parameters_[i], fd.parameters_[i], 1e-4);
    }
    ASSERT_LT(aad.evaluations_, fd.evaluations_);
}

TEST(GSRSLVCalibrationTest, TestAADCalibrationCountsDerivativeBatches) {
    const auto model = Model(1.0, 0.6);
    GSRSLVCalibrationSettings_ settings;
    settings.pricing_.paths_ = settings.validation_.paths_ = 512;
    const double target = PriceGSRSLVEuropeanOptions(model, {Bond()}, settings.pricing_)[0].price_;
    const Vector_<CalibrationQuote_> quotes{{"bond", Bond(), target, 0.003}};
    const Vector_<GSRSLVCalibrationParameter_> parameters{{"leverage:0:0", 0.2, 2.0, 1.0}};
    const auto fd = CalibrateGSRSLV(model, quotes, parameters, settings);
    settings.useAADJacobian_ = true;
    const auto aad = CalibrateGSRSLV(model, quotes, parameters, settings);
    ASSERT_TRUE(aad.converged_);
    ASSERT_LT(aad.evaluations_, fd.evaluations_);
}

#if defined(DAL_USE_XAD_AAD)
TEST(GSRSLVCalibrationTest, TestAADStorageStaysBoundedAcrossPathBudgets) {
    const auto data = Model(1.1, 0.6);
    const Vector_<EuropeanRateOption_> options{Bond(0.94), Bond(0.97), Bond(1.0)};
    AAD::Clear(*AAD::Tape());
    const GSRSLVPricingInternal::PreparedPricer_ small(data, options, {128, 1729});
    const auto shortJacobian = small.Jacobian(data, {"leverage:0:0"});
    const size_t shortMemory = AAD::Tape()->tape_.getMemory();
    AAD::Clear(*AAD::Tape());
    const GSRSLVPricingInternal::PreparedPricer_ large(data, options, {512, 1729});
    const auto longJacobian = large.Jacobian(data, {"leverage:0:0"});
    const size_t longMemory = AAD::Tape()->tape_.getMemory();
    ASSERT_LE(longMemory, 2 * shortMemory) << "short=" << shortMemory << " long=" << longMemory;
    ASSERT_EQ(shortJacobian.Rows(), longJacobian.Rows());
    const auto high = large.Price(Model(1.10001, 0.6)), low = large.Price(Model(1.09999, 0.6));
    for (int row = 0; row < longJacobian.Rows(); ++row)
        ASSERT_NEAR(longJacobian(row, 0), (high[row].price_ - low[row].price_) / 2e-5, 2e-7);
    AAD::Clear(*AAD::Tape());
}
#endif

TEST(GSRSLVCalibrationTest, TestConditionalAADIncludesContinuationAndClearsTape) {
    const auto data = Model(1.1, 0.6);
    const auto exercise = TODAY.AddDays(365), fixing = TODAY.AddDays(540), start = fixing.AddDays(2), end = start.AddDays(365), pay = end.AddDays(2);
    const Swaption_ option{exercise, {{pay, 1.0}}, {{fixing, start, end, pay, 1.0, 1.0, "12M"}}, 0.03, OptionType_("CALL")};
    const GSRSLVPricingInternal::PreparedPricer_ pricer(data, {option}, {64, 1729, 8});
    const auto jacobian = pricer.Jacobian(data, {"leverage:0:0"});
    const auto high = pricer.Price(Model(1.10001, 0.6)), low = pricer.Price(Model(1.09999, 0.6));
    ASSERT_NEAR(jacobian(0, 0), (high[0].price_ - low[0].price_) / 2e-5, 2e-7);
    const auto repeated = pricer.Jacobian(data, {"leverage:0:0"});
    ASSERT_DOUBLE_EQ(jacobian(0, 0), repeated(0, 0));
    AAD::Rewind(*AAD::Tape());
    AAD::Number_ input(3.0);
    AAD::PutOnTape(input);
    AAD::NewRecording(*AAD::Tape());
    AAD::Number_ square = input * input;
    AAD::ZeroAdjoints(*AAD::Tape());
    AAD::Adjoint(square) = 1.0;
    AAD::PropagateToStart(*AAD::Tape());
    ASSERT_NEAR(AAD::AdjointValue(input), 6.0, 1e-12);
    AAD::Rewind(*AAD::Tape());
}

TEST(GSRSLVCalibrationTest, TestNonGaussianFitAndHeldOutValidation) {
    const auto truth = Model(1.3, 0.6), initial = Model(0.8, 0.6);
    GSRSLVCalibrationSettings_ settings;
    settings.pricing_ = {16384, 1729};
    settings.validation_ = {32768, 81173};
    const Vector_<EuropeanRateOption_> options{Bond(0.94), Bond(0.97), Bond(1.0)};
    const auto prices = PriceGSRSLVEuropeanOptions(truth, options, settings.pricing_);
    Vector_<CalibrationQuote_> quotes{{"itm", options[0], prices[0].price_, 0.002}, {"atm", options[1], prices[1].price_, 0.002}};
    const Vector_<CalibrationQuote_> heldOut{{"otm", options[2], prices[2].price_, 0.002}};
    const auto result = CalibrateGSRSLV(initial, quotes, {{"leverage:0:0", 0.2, 2.0, 1.0}}, settings, heldOut);
    ASSERT_TRUE(result.converged_) << result.terminationReason_;
    ASSERT_TRUE(result.fitWithinTolerance_);
    ASSERT_TRUE(result.numericalValidationPassed_) << "uncertainty=" << result.numericalErrors_[0] << ", " << result.numericalErrors_[1];
    ASSERT_TRUE(result.heldOutWithinTolerance_);
    ASSERT_NEAR(result.parameters_[0], 1.3, 1e-5);
    ASSERT_EQ(result.jacobianRank_, 1);
    ASSERT_GT(result.jacobianConditionEstimate_, 0.0);
    ASSERT_DOUBLE_EQ(initial.leverage_->values_(0, 0), 0.8);
    ASSERT_EQ(result.heldOutPrices_.size(), 1U);
}

TEST(GSRSLVCalibrationTest, TestCalibratedQuoteRiskIncludesRegularization) {
    GSRSLVCalibrationSettings_ settings;
    settings.pricing_ = {1024, 1729};
    settings.validation_ = {2048, 81173};
    settings.solver_.priorWeight_ = 2.0;
    const auto initial = Model(0.8, 0.6);
    const Vector_<CalibrationQuote_> quotes{
        {"bond", Bond(), PriceGSRSLVEuropeanOptions(Model(1.2, 0.6), {Bond()}, settings.pricing_)[0].price_, 0.005}};
    const Vector_<GSRSLVCalibrationParameter_> parameters{{"leverage:0:0", 0.2, 2.0, 1.0}};
    const auto risk = GSRSLVQuoteRisk(initial, quotes, parameters, {Bond()}, settings);
    ASSERT_TRUE(risk.calibration_.converged_);
    ASSERT_TRUE(risk.stable_[0]);
    ASSERT_TRUE(risk.activeSetStable_[0]);
    ASSERT_GT(risk.sensitivities_(0, 0), 0.0);
    ASSERT_LT(risk.sensitivities_(0, 0), 0.9);
    auto up = quotes, down = quotes;
    up[0].price_ += 5e-5;
    down[0].price_ -= 5e-5;
    const auto high = CalibrateGSRSLV(initial, up, parameters, settings), low = CalibrateGSRSLV(initial, down, parameters, settings);
    const double reference = (PriceGSRSLVEuropeanOptions(*high.model_, {Bond()}, settings.pricing_)[0].price_ -
                              PriceGSRSLVEuropeanOptions(*low.model_, {Bond()}, settings.pricing_)[0].price_) /
                             1e-4;
    ASSERT_NEAR(risk.sensitivities_(0, 0), reference, 1e-4);
    const double fitted = risk.calibration_.parameters_[0], step = 1e-5;
    const auto evaluate = [&](double level) { return PriceGSRSLVEuropeanOptions(Model(level, 0.6), {Bond()}, settings.pricing_)[0].price_; };
    const double middle = evaluate(fitted), highPrice = evaluate(fitted + step), lowPrice = evaluate(fitted - step);
    const double first = (highPrice - lowPrice) / (2.0 * step), second = (highPrice - 2.0 * middle + lowPrice) / (step * step);
    const double scaleSquared = quotes[0].priceScale_ * quotes[0].priceScale_;
    const double fullHessian = (first * first + (middle - quotes[0].price_) * second) / scaleSquared + settings.solver_.priorWeight_;
    ASSERT_GT(fullHessian, 0.0);
    ASSERT_NEAR(risk.sensitivities_(0, 0), first * first / (scaleSquared * fullHessian), 2e-4);
}

TEST(GSRSLVCalibrationTest, TestStagedSmileAndLeverageFitReachesJointOptimum) {
    GSRSLVCalibrationSettings_ settings;
    settings.pricing_ = {4096, 1729};
    settings.validation_ = {4096, 81173};
    settings.solver_.gradientTolerance_ = 1e-8;
    const auto truth = Model(1.2, 0.8), initial = Model(0.9, 0.25);
    Vector_<EuropeanRateOption_> options;
    for (int expiry = 1; expiry <= 2; ++expiry)
        for (double strike : {0.94, 0.97, 1.0})
            options.push_back(BondOption_{TODAY.AddDays(365 * expiry), TODAY.AddDays(365 * (expiry + 1)), strike, OptionType_("CALL")});
    const auto prices = PriceGSRSLVEuropeanOptions(truth, options, settings.pricing_);
    Vector_<CalibrationQuote_> quotes;
    for (size_t i = 0; i < prices.size(); ++i)
        quotes.push_back({"quote" + String::FromInt(i), options[i], prices[i].price_, 0.002});
    const auto fit = CalibrateGSRSLV(initial, quotes, {{"volOfVol", 0.1, 1.5, 1.0}, {"leverage:0:0", 0.3, 2.0, 1.0}}, settings);
    ASSERT_TRUE(fit.converged_) << fit.terminationReason_ << " objective=" << fit.objective_;
    ASSERT_TRUE(fit.fitWithinTolerance_);
    ASSERT_NEAR(fit.parameters_[0], 0.8, 1e-4);
    ASSERT_NEAR(fit.parameters_[1], 1.2, 1e-5);
    ASSERT_EQ(fit.jacobianRank_, 2);
}

TEST(GSRSLVCalibrationTest, TestIndependentValidationFailureIsSeparateFromConvergence) {
    GSRSLVCalibrationSettings_ settings;
    settings.pricing_ = {32, 1729};
    settings.validation_ = {32, 81173};
    const auto initial = Model();
    const double target = PriceGSRSLVEuropeanOptions(initial, {Bond()}, settings.pricing_)[0].price_;
    const auto fit =
        CalibrateGSRSLV(initial, {{"fit", Bond(), target, 1e-8}}, {{"leverage:0:0", 0.2, 2.0, 1.0}}, settings, {{"held", Bond(1.0), 1.0, 0.001}});
    ASSERT_TRUE(fit.converged_);
    ASSERT_TRUE(fit.fitWithinTolerance_);
    ASSERT_FALSE(fit.numericalValidationPassed_);
    ASSERT_FALSE(fit.heldOutWithinTolerance_);
    ASSERT_GT(fit.numericalErrors_[0], 1e-8);
}

TEST(GSRSLVCalibrationTest, TestLeverageSmoothingPenalizesSpatialAndTimeChanges) {
    const auto base = Model(1.0, 0.6);
    const Handle_<GSRLeverageData_> initialLeverage(new GSRLeverageData_("initial", {-0.02, 0.02}, {0.0, 1.0}, Matrix_<>(2, 2, 1.0)));
    Matrix_<> levels(2, 2);
    levels(0, 0) = 1.4;
    levels(0, 1) = 1.3;
    levels(1, 0) = 0.9;
    levels(1, 1) = 1.0;
    const Handle_<GSRLeverageData_> targetLeverage(new GSRLeverageData_("truth", {-0.02, 0.02}, {0.0, 1.0}, levels));
    const GSRSLVSettings_ dynamics{base.kappa_, base.volOfVol_, base.varianceCorrelations_, base.maxStep_};
    const GSRSLVModelData_ initial("initial", base.gaussian_, initialLeverage, dynamics), truth("truth", base.gaussian_, targetLeverage, dynamics);
    GSRSLVCalibrationSettings_ settings;
    settings.pricing_ = {1024, 1729};
    settings.validation_ = {1024, 81173};
    settings.solver_.priorWeight_ = 0.2;
    Vector_<EuropeanRateOption_> options;
    for (int expiry = 1; expiry <= 2; ++expiry)
        for (double strike : {0.94, 0.97, 1.0})
            options.push_back(BondOption_{TODAY.AddDays(365 * expiry), TODAY.AddDays(365 * (expiry + 1)), strike, OptionType_("CALL")});
    const auto prices = PriceGSRSLVEuropeanOptions(truth, options, settings.pricing_);
    Vector_<CalibrationQuote_> quotes;
    for (size_t i = 0; i < prices.size(); ++i)
        quotes.push_back({"quote" + String::FromInt(i), options[i], prices[i].price_, 0.001});
    const Vector_<GSRSLVCalibrationParameter_> parameters{
        {"leverage:0:0", 0.2, 2.0, 1.0}, {"leverage:0:1", 0.2, 2.0, 1.0}, {"leverage:1:0", 0.2, 2.0, 1.0}, {"leverage:1:1", 0.2, 2.0, 1.0}};
    const auto raw = CalibrateGSRSLV(initial, quotes, parameters, settings);
    settings.solver_.smoothingWeight_ = 50.0;
    const auto smooth = CalibrateGSRSLV(initial, quotes, parameters, settings);
    settings.useAADJacobian_ = true;
    const auto aadSmooth = CalibrateGSRSLV(initial, quotes, parameters, settings);
    ASSERT_TRUE(raw.converged_) << raw.terminationReason_;
    ASSERT_TRUE(smooth.converged_) << smooth.terminationReason_;
    ASSERT_TRUE(aadSmooth.converged_) << aadSmooth.terminationReason_;
    ASSERT_NEAR(aadSmooth.objective_, smooth.objective_, 1e-6);
    for (size_t i = 0; i < parameters.size(); ++i)
        ASSERT_NEAR(aadSmooth.parameters_[i], smooth.parameters_[i], 1e-4);
    const auto roughness = [](const Vector_<>& x) {
        return std::abs(x[0] - x[1]) + std::abs(x[2] - x[3]) + std::abs(x[0] - x[2]) + std::abs(x[1] - x[3]);
    };
    ASSERT_GT(roughness(raw.parameters_), 0.1);
    ASSERT_LT(roughness(smooth.parameters_), 0.2 * roughness(raw.parameters_));
    ASSERT_DOUBLE_EQ(initial.leverage_->values_(0, 0), 1.0);
}

TEST(GSRSLVCalibrationTest, TestBoundsRankAndMalformedCalibration) {
    GSRSLVCalibrationSettings_ settings;
    settings.pricing_ = {512, 1729};
    settings.validation_ = {512, 81173};
    const auto initial = Model();
    const double target = PriceGSRSLVEuropeanOptions(initial, {Bond()}, settings.pricing_)[0].price_;
    const Vector_<CalibrationQuote_> quotes{{"bond", Bond(), target, 0.001}};
    const auto unidentifiable = CalibrateGSRSLV(initial, quotes, {{"kappa", 0.1, 1.0, 1.0}}, settings);
    ASSERT_TRUE(unidentifiable.converged_);
    ASSERT_EQ(unidentifiable.jacobianRank_, 0);
    ASSERT_TRUE(std::isinf(unidentifiable.jacobianConditionEstimate_));
    ASSERT_TRUE(unidentifiable.activeBounds_[0]);
    ASSERT_THROW(GSRSLVQuoteRisk(initial, quotes, {{"kappa", 0.1, 1.0, 1.0}}, {Bond()}, settings), Exception_);
    const auto bounded = CalibrateGSRSLV(Model(0.8), quotes, {{"leverage:0:0", 0.2, 0.9, 1.0}}, settings);
    ASSERT_TRUE(bounded.converged_);
    ASSERT_TRUE(bounded.activeBounds_[0]);
    ASSERT_NEAR(bounded.parameters_[0], 0.9, 1e-12);
    ASSERT_THROW(CalibrateGSRSLV(initial, quotes, {}, settings), Exception_);
    ASSERT_THROW(CalibrateGSRSLV(initial, quotes, {{"leverage:0:0", 0.0, 2.0, 1.0}}, settings), Exception_);
    ASSERT_THROW(CalibrateGSRSLV(initial, quotes, {{"leverage:0:0", 0.1, 2.0, 0.0}}, settings), Exception_);
    ASSERT_THROW(CalibrateGSRSLV(initial, quotes, {{"leverage:0:0", 0.1, 2.0, 1.0}, {"leverage:0:0", 0.1, 2.0, 1.0}}, settings), Exception_);
    ASSERT_THROW(CalibrateGSRSLV(initial, quotes, {{"leverage:9:0", 0.1, 2.0, 1.0}}, settings), Exception_);
    ASSERT_THROW(CalibrateGSRSLV(initial, quotes, {{"leverage:0:0", 1.1, 2.0, 1.0}}, settings), Exception_);
    settings.solver_.maxIterations_ = 1;
    const auto incomplete = CalibrateGSRSLV(Model(0.8), quotes, {{"leverage:0:0", 0.1, 2.0, 1.0}}, settings);
    ASSERT_FALSE(incomplete.converged_);
    ASSERT_THROW(GSRSLVQuoteRisk(Model(0.8), quotes, {{"leverage:0:0", 0.1, 2.0, 1.0}}, {Bond()}, settings), Exception_);
}

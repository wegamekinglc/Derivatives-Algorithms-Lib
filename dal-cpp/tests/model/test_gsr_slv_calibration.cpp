//
// Created by Codex on 2026/10/2.
//

#include <gtest/gtest.h>

#include <dal/platform/platform.hpp>
#include <dal/model/gsrslvpricing.hpp>
#include <dal/model/gsrslvcalibration.hpp>

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

    GSREuropeanOption_ Bond(double strike = 0.97) { return GSRBondOption_{TODAY.AddDays(365), TODAY.AddDays(730), strike, OptionType_("CALL")}; }
} // namespace

TEST(GSRSLVCalibrationTest, TestMonteCarloMatchesIndependentGaussianPrices) {
    for (int factors = 1; factors <= 3; ++factors) {
        const auto model = Model(1.0, 0.0, factors);
        const Vector_<GSREuropeanOption_> options{Bond(0.94), Bond(0.97), Bond(1.0)};
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
    const GSRCaplet_ caplet{expiry, expiry, end, end, 1.0, 1.0, "12M", 0.03, OptionType_("CALL")};
    const GSRSwaption_ swaption{expiry, {{end, 1.0}}, {{expiry, expiry, end, end, 1.0, 1.0, "12M"}}, 0.03, OptionType_("CALL")};
    const auto result = PriceGSRSLVEuropeanOptions(Model(1.0, 0.6), {caplet, swaption}, {1024, 1729});
    ASSERT_NEAR(result[0].price_, result[1].price_, 1e-14);
    ASSERT_NEAR(result[0].standardError_, result[1].standardError_, 1e-14);
}

TEST(GSRSLVCalibrationTest, TestPaymentLagAndFutureCouponsMatchGaussianLimit) {
    const auto expiry = TODAY.AddDays(365), middle = TODAY.AddDays(730), end = TODAY.AddDays(1095);
    const GSRCaplet_ lagged{expiry, expiry, middle, middle.AddDays(90), 1.0, 0.75, "12M", 0.03, OptionType_("CALL")};
    const GSRSwaption_ swaption{expiry,
                                {{middle, 1.0}, {end, 1.0}},
                                {{expiry, expiry, middle, middle, 1.0, 1.0, "12M"}, {middle, middle, end, end, 1.0, 1.0, "12M"}},
                                0.03,
                                OptionType_("CALL")};
    const auto model = Model();
    const Vector_<GSREuropeanOption_> options{lagged, swaption};
    const auto prices = PriceGSRSLVEuropeanOptions(model, options, {20000, 1729});
    for (size_t i = 0; i < options.size(); ++i)
        ASSERT_NEAR(prices[i].price_, PriceGSREuropeanOption(*model.gaussian_, options[i]).price_, 5.0 * prices[i].standardError_ + 1e-6);
    const GSRBondOption_ immediate{TODAY, middle, 0.9, OptionType_("CALL")};
    const auto result = PriceGSRSLVEuropeanOptions(model, {immediate}, {4, 1729})[0];
    ASSERT_NEAR(result.price_, std::exp(-0.06) - 0.9, 1e-14);
    ASSERT_DOUBLE_EQ(result.standardError_, 0.0);
}

TEST(GSRSLVCalibrationTest, TestRejectsUnsupportedFutureCouponAndMalformedInputs) {
    const auto fixing = TODAY.AddDays(730), end = TODAY.AddDays(1095);
    const GSRSwaption_ lagged{TODAY.AddDays(365), {{end, 1.0}}, {{fixing, fixing, end, end.AddDays(2), 1.0, 1.0, "12M"}}, 0.03, OptionType_("CALL")};
    ASSERT_THROW(PriceGSRSLVEuropeanOptions(Model(), {lagged}), Exception_);
    ASSERT_THROW(PriceGSRSLVEuropeanOptions(Model(), {Bond()}, {3, 1729}), Exception_);
    ASSERT_THROW(PriceGSRSLVEuropeanOptions(Model(), {Bond()}, {1024, -1}), Exception_);
    ASSERT_THROW(PriceGSRSLVEuropeanOptions(Model(), {}), Exception_);
    const GSRCaplet_ overflowing{TODAY.AddDays(365), TODAY.AddDays(365), TODAY.AddDays(730), TODAY.AddDays(730), 1e-308, 1e308, "12M", 0.03,
                                 OptionType_("CALL")};
    ASSERT_THROW(PriceGSRSLVEuropeanOptions(Model(), {overflowing}, {4, 1729}), Exception_);
}

TEST(GSRSLVCalibrationTest, TestNonGaussianFitAndHeldOutValidation) {
    const auto truth = Model(1.3, 0.6), initial = Model(0.8, 0.6);
    GSRSLVCalibrationSettings_ settings;
    settings.pricing_ = {16384, 1729};
    settings.validation_ = {32768, 81173};
    const Vector_<GSREuropeanOption_> options{Bond(0.94), Bond(0.97), Bond(1.0)};
    const auto prices = PriceGSRSLVEuropeanOptions(truth, options, settings.pricing_);
    Vector_<GSRCalibrationQuote_> quotes{{"itm", options[0], prices[0].price_, 0.002}, {"atm", options[1], prices[1].price_, 0.002}};
    const Vector_<GSRCalibrationQuote_> heldOut{{"otm", options[2], prices[2].price_, 0.002}};
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
    const Vector_<GSRCalibrationQuote_> quotes{
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
}

TEST(GSRSLVCalibrationTest, TestStagedSmileAndLeverageFitReachesJointOptimum) {
    GSRSLVCalibrationSettings_ settings;
    settings.pricing_ = {4096, 1729};
    settings.validation_ = {4096, 81173};
    settings.solver_.gradientTolerance_ = 1e-8;
    const auto truth = Model(1.2, 0.8), initial = Model(0.9, 0.25);
    Vector_<GSREuropeanOption_> options;
    for (int expiry = 1; expiry <= 2; ++expiry)
        for (double strike : {0.94, 0.97, 1.0})
            options.push_back(GSRBondOption_{TODAY.AddDays(365 * expiry), TODAY.AddDays(365 * (expiry + 1)), strike, OptionType_("CALL")});
    const auto prices = PriceGSRSLVEuropeanOptions(truth, options, settings.pricing_);
    Vector_<GSRCalibrationQuote_> quotes;
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
    Vector_<GSREuropeanOption_> options;
    for (int expiry = 1; expiry <= 2; ++expiry)
        for (double strike : {0.94, 0.97, 1.0})
            options.push_back(GSRBondOption_{TODAY.AddDays(365 * expiry), TODAY.AddDays(365 * (expiry + 1)), strike, OptionType_("CALL")});
    const auto prices = PriceGSRSLVEuropeanOptions(truth, options, settings.pricing_);
    Vector_<GSRCalibrationQuote_> quotes;
    for (size_t i = 0; i < prices.size(); ++i)
        quotes.push_back({"quote" + String::FromInt(i), options[i], prices[i].price_, 0.001});
    const Vector_<GSRSLVCalibrationParameter_> parameters{
        {"leverage:0:0", 0.2, 2.0, 1.0}, {"leverage:0:1", 0.2, 2.0, 1.0}, {"leverage:1:0", 0.2, 2.0, 1.0}, {"leverage:1:1", 0.2, 2.0, 1.0}};
    const auto raw = CalibrateGSRSLV(initial, quotes, parameters, settings);
    settings.solver_.smoothingWeight_ = 50.0;
    const auto smooth = CalibrateGSRSLV(initial, quotes, parameters, settings);
    ASSERT_TRUE(raw.converged_) << raw.terminationReason_;
    ASSERT_TRUE(smooth.converged_) << smooth.terminationReason_;
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
    const Vector_<GSRCalibrationQuote_> quotes{{"bond", Bond(), target, 0.001}};
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

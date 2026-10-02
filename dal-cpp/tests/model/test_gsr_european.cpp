//
// Created by Codex on 2026/10/2.
//

#include <gtest/gtest.h>

#include <cmath>

#include <dal/platform/platform.hpp>

#include <dal/math/distribution/black.hpp>
#include <dal/math/random/base.hpp>
#include <dal/math/random/sobol.hpp>
#include <dal/model/gsrcalibration.hpp>
#include <dal/model/gsreuropean.hpp>

using namespace Dal;

namespace {
    const Date_ TODAY(2026, 10, 2);

    MultiFactorGSRModelData_ EuropeanModel(double g = 0.02) {
        const Handle_<GSRCurveData_> curve(new GSRCurveData_("curve", TODAY, "USD", {TODAY, TODAY.AddDays(1825)}, {0.0, -0.15}, {}, Matrix_<>(0, 0)));
        MultiFactorGSRVolSettings_ settings;
        settings.factorNames_ = {"level"};
        settings.gKnotDates_ = settings.hKnotDates_ = {TODAY};
        settings.gValues_ = Matrix_<>(1, 1, g);
        settings.hValues_ = Matrix_<>(1, 1, 1.0);
        settings.correlations_ = Matrix_<>(1, 1, 1.0);
        return MultiFactorGSRModelData_("model", curve, Handle_<MultiFactorGSRVolData_>(new MultiFactorGSRVolData_("vol", settings)));
    }

    GSRSwaption_ Swaption() {
        GSRSwaption_ option;
        option.expiry_ = TODAY.AddDays(365);
        option.strike_ = 0.03;
        for (int year = 2; year <= 4; ++year) {
            option.fixed_.push_back({TODAY.AddDays(365 * year), 1.0});
            option.floating_.push_back({TODAY.AddDays(365 * (year - 1)), TODAY.AddDays(365 * (year - 1)), TODAY.AddDays(365 * year),
                                        TODAY.AddDays(365 * year), 1.0, 1.0, "12M"});
        }
        return option;
    }
} // namespace

TEST(GSREuropeanTest, TestBondOptionMatchesIndependentBlackPrice) {
    const GSRBondOption_ option{TODAY.AddDays(365), TODAY.AddDays(1095), 0.94, OptionType_("CALL")};
    const double expected = std::exp(-0.03) * Distribution::BlackOpt(std::exp(-0.06), 0.04, 0.94, OptionType_("CALL"));
    const auto result = PriceGSREuropeanOption(EuropeanModel(), option);
    ASSERT_NEAR(result.price_, expected, 1e-12);
    ASSERT_EQ(result.numericalError_, 0.0);
}

TEST(GSREuropeanTest, TestCapletPaymentLagUsesPaymentMeasure) {
    const GSRCaplet_ option{TODAY.AddDays(365), TODAY.AddDays(365), TODAY.AddDays(730), TODAY.AddDays(912), 1.0, 0.75, "12M", 0.03,
                            OptionType_("CALL")};
    const double payTime = 912.0 / 365.0;
    const double forward = std::exp(0.03 - 0.0004 * (payTime - 2.0));
    const double expected = std::exp(-0.03 * payTime) * 0.75 * Distribution::BlackOpt(forward, 0.02, 1.03, OptionType_("CALL"));
    ASSERT_NEAR(PriceGSREuropeanOption(EuropeanModel(), option).price_, expected, 1e-12);
}

TEST(GSRCalibrationTest, TestIndependentBondQuotesRecoverVolatility) {
    const auto initial = EuropeanModel(0.009);
    Vector_<GSRCalibrationQuote_> quotes;
    for (int expiry = 1; expiry <= 3; ++expiry) {
        const GSRBondOption_ option{TODAY.AddDays(365 * expiry), TODAY.AddDays(365 * (expiry + 1)), 0.97, OptionType_("CALL")};
        const double target = std::exp(-0.03 * expiry) * Distribution::BlackOpt(std::exp(-0.03), 0.02 * std::sqrt(expiry), 0.97, OptionType_("CALL"));
        quotes.push_back({"bond" + String::FromInt(expiry), option, target, 1e-6});
    }
    const auto result = CalibrateGSRVolatility(initial, quotes, {{0, 0, 0.0, 0.1}});
    ASSERT_TRUE(result.converged_) << result.terminationReason_ << " objective=" << result.objective_;
    ASSERT_TRUE(result.fitWithinTolerance_);
    ASSERT_TRUE(result.numericalValidationPassed_);
    ASSERT_NEAR(result.model_->vol_->gValues_(0, 0), 0.02, 1e-9);
    ASSERT_EQ(initial.vol_->gValues_(0, 0), 0.009);
    ASSERT_EQ(result.jacobianRank_, 1);
    const double forward = std::exp(-0.03);
    const double dPlus = std::log(forward / 0.97) / 0.02 + 0.01;
    const double vega = std::exp(-0.03) * forward * std::exp(-0.5 * dPlus * dPlus) / std::sqrt(2.0 * 3.14159265358979323846);
    ASSERT_NEAR(result.quoteJacobian_(0, 0), vega, 1e-8);
}

TEST(GSREuropeanTest, TestOnePeriodSwaptionMatchesBondPutAndCaplet) {
    auto option = Swaption();
    option.fixed_.Resize(1);
    option.floating_.Resize(1);
    const double expected = std::exp(-0.03) * 1.03 * Distribution::BlackOpt(std::exp(-0.03), 0.02, 1.0 / 1.03, OptionType_("PUT"));
    const auto result = PriceGSREuropeanOption(EuropeanModel(), option);
    ASSERT_NEAR(result.price_, expected, 1e-12);
    const GSRCaplet_ caplet{option.expiry_, option.expiry_, TODAY.AddDays(730), TODAY.AddDays(730), 1.0, 1.0, "12M", 0.03, OptionType_("CALL")};
    ASSERT_NEAR(result.price_, PriceGSREuropeanOption(EuropeanModel(), caplet).price_, 1e-12);
}

TEST(GSREuropeanTest, TestTwoFactorSwaptionMatchesIndependentSobolIntegration) {
    const auto original = EuropeanModel();
    Matrix_<> g(2, 1), h(2, 2), correlation(2, 2, 0.25);
    g(0, 0) = 0.02;
    g(1, 0) = 0.015;
    h(0, 0) = h(0, 1) = 1.0;
    h(1, 0) = 0.3;
    h(1, 1) = -0.4;
    correlation(0, 0) = correlation(1, 1) = 1.0;
    const Handle_<MultiFactorGSRVolData_> vol(
        new MultiFactorGSRVolData_("vol", {"level", "slope"}, {TODAY}, g, {TODAY, TODAY.AddDays(1095)}, h, correlation));
    const MultiFactorGSRModelData_ model("two", original.curve_, vol);
    auto option = Swaption();
    const auto result = PriceGSREuropeanOption(model, option, {24, true});
    auto random = NewSobol(2, 1);
    Vector_<> normals(2);
    double value = 0.0;
    for (int path = 0; path < 131072; ++path) {
        random->FillNormal(&normals);
        double annuity = 0.0, finalBond = 0.0;
        for (int year = 2; year <= 4; ++year) {
            const double b0 = year - 1.0;
            const double b1 = year <= 3 ? 0.3 * (year - 1.0) : 0.6 - 0.4 * (year - 3.0);
            const double beta0 = -b0 * 0.02 - b1 * 0.015 * 0.25;
            const double beta1 = -b1 * 0.015 * std::sqrt(1.0 - 0.25 * 0.25);
            const double bond = std::exp(-0.03 * (year - 1) - 0.5 * (beta0 * beta0 + beta1 * beta1) + beta0 * normals[0] + beta1 * normals[1]);
            annuity += bond;
            finalBond = bond;
        }
        value += std::max(1.0 - finalBond - 0.03 * annuity, 0.0);
    }
    ASSERT_NEAR(result.price_, std::exp(-0.03) * value / 131072, 2e-6);
    ASSERT_LT(result.numericalError_, 1e-7);
    option.type_ = OptionType_("PUT");
    const auto receiver = PriceGSREuropeanOption(model, option, {24, true});
    const double parity = std::exp(-0.03) - std::exp(-0.12) - 0.03 * (std::exp(-0.06) + std::exp(-0.09) + std::exp(-0.12));
    ASSERT_NEAR(result.price_ - receiver.price_, parity, 1e-11);
}

TEST(GSREuropeanTest, TestZeroVolatilityNegativeStrikeAndSingularFactors) {
    auto option = Swaption();
    option.strike_ = -0.01;
    const double intrinsic = std::exp(-0.03) - std::exp(-0.12) + 0.01 * (std::exp(-0.06) + std::exp(-0.09) + std::exp(-0.12));
    ASSERT_NEAR(PriceGSREuropeanOption(EuropeanModel(0.0), option).price_, intrinsic, 1e-12);
    const auto original = EuropeanModel();
    const Handle_<MultiFactorGSRVolData_> vol(
        new MultiFactorGSRVolData_("vol", {"first", "second"}, {TODAY}, Matrix_<>(2, 1, 0.01), {TODAY}, Matrix_<>(2, 1, 1.0), Matrix_<>(2, 2, 1.0)));
    const MultiFactorGSRModelData_ singular("singular", original.curve_, vol);
    ASSERT_NEAR(PriceGSREuropeanOption(singular, option).price_, PriceGSREuropeanOption(original, option).price_, 1e-12);
    const GSRBondOption_ negative{TODAY.AddDays(365), TODAY.AddDays(730), -0.2, OptionType_("CALL")};
    ASSERT_NEAR(PriceGSREuropeanOption(original, negative).price_, std::exp(-0.06) + 0.2 * std::exp(-0.03), 1e-12);
}

TEST(GSREuropeanTest, TestRejectsInvalidOptionInputs) {
    const auto model = EuropeanModel();
    auto option = Swaption();
    option.expiry_ = TODAY.AddDays(-1);
    ASSERT_THROW(PriceGSREuropeanOption(model, option), Exception_);
    option = Swaption();
    option.floating_[0].start_ = TODAY;
    ASSERT_THROW(PriceGSREuropeanOption(model, option), Exception_);
    option = Swaption();
    option.fixed_[0].accrual_ = 0.0;
    ASSERT_THROW(PriceGSREuropeanOption(model, option), Exception_);
    option = Swaption();
    option.type_ = OptionType_("STRADDLE");
    ASSERT_THROW(PriceGSREuropeanOption(model, option), Exception_);
    ASSERT_THROW(PriceGSREuropeanOption(model, Swaption(), {1, false}), Exception_);
}

TEST(GSREuropeanTest, TestSignedLoadingsRetainBothExerciseBoundaries) {
    const Handle_<GSRCurveData_> curve(new GSRCurveData_("zero", TODAY, "USD", {TODAY, TODAY.AddDays(1095)}, {0.0, 0.0}, {}, Matrix_<>(0, 0)));
    Matrix_<> h(1, 2);
    h(0, 0) = 1.0;
    h(0, 1) = -2.0;
    const Handle_<MultiFactorGSRVolData_> vol(
        new MultiFactorGSRVolData_("vol", {"level"}, {TODAY}, Matrix_<>(1, 1, 0.02), {TODAY, TODAY.AddDays(730)}, h, Matrix_<>(1, 1, 1.0)));
    const MultiFactorGSRModelData_ model("model", curve, vol);
    GSRSwaption_ option;
    option.expiry_ = TODAY.AddDays(365);
    option.fixed_ = {{option.expiry_, 1.0}};
    option.floating_ = {{option.expiry_, option.expiry_, TODAY.AddDays(730), TODAY.AddDays(730), 1.0, 1.0, "12M"},
                        {option.expiry_, option.expiry_, TODAY.AddDays(1095), TODAY.AddDays(1095), 2.0, 2.0, "12M"}};
    option.strike_ = 0.0001;
    const double boundary = std::acosh((2.0 - option.strike_) * std::exp(0.5 * 0.02 * 0.02) / 2.0) / 0.02;
    const auto cdf = [](double x) { return 0.5 * std::erfc(-x / std::sqrt(2.0)); };
    const double expected = (2.0 - option.strike_) * (cdf(boundary) - cdf(-boundary)) - 2.0 * (cdf(boundary - 0.02) - cdf(-boundary - 0.02));
    ASSERT_NEAR(PriceGSREuropeanOption(model, option).price_, expected, 1e-12);
}

TEST(GSREuropeanTest, TestFutureCouponFixingAndPaymentLagConvexity) {
    GSRSwaption_ option;
    option.expiry_ = TODAY;
    option.fixed_ = {{TODAY.AddDays(1278), 1.0}};
    option.floating_ = {{TODAY.AddDays(548), TODAY.AddDays(730), TODAY.AddDays(1095), TODAY.AddDays(1278), 1.0, 1.0, "12M"}};
    option.strike_ = -0.1;
    const double fixing = 548.0 / 365.0, pay = 1278.0 / 365.0;
    const double expected = std::exp(-0.03 * pay) * (std::exp(0.03 - 0.0004 * fixing * (pay - 3.0)) - 0.9);
    ASSERT_NEAR(PriceGSREuropeanOption(EuropeanModel(), option).price_, expected, 1e-12);
    option.floating_[0].fixing_ = TODAY.AddDays(-1);
    ASSERT_THROW(PriceGSREuropeanOption(EuropeanModel(), option), Exception_);
}

TEST(GSREuropeanTest, TestThreeFactorConstantShapesMatchScalarEquivalent) {
    const auto original = EuropeanModel();
    Matrix_<> g(3, 1), h(3, 1), correlation(3, 3, 0.0);
    double variance = 0.0;
    for (int factor = 0; factor < 3; ++factor) {
        g(factor, 0) = 0.008 + 0.002 * factor;
        h(factor, 0) = factor == 1 ? -0.5 : 1.0;
        correlation(factor, factor) = 1.0;
        variance += std::pow(g(factor, 0) * h(factor, 0), 2);
    }
    const Handle_<MultiFactorGSRVolData_> vol(new MultiFactorGSRVolData_("vol", {"one", "two", "three"}, {TODAY}, g, {TODAY}, h, correlation));
    const MultiFactorGSRModelData_ model("three", original.curve_, vol);
    const auto actual = PriceGSREuropeanOption(model, Swaption(), {24, true});
    ASSERT_NEAR(actual.price_, PriceGSREuropeanOption(EuropeanModel(std::sqrt(variance)), Swaption()).price_, 1e-9);
    ASSERT_LT(actual.numericalError_, 1e-8);
}

TEST(GSRCalibrationTest, TestBucketRecoveryHeldOutPricingAndSmoothPrior) {
    const auto original = EuropeanModel();
    Matrix_<> g(1, 3);
    g(0, 0) = 0.008;
    g(0, 1) = 0.009;
    g(0, 2) = 0.01;
    const Handle_<MultiFactorGSRVolData_> vol(new MultiFactorGSRVolData_("vol", {"level"}, {TODAY, TODAY.AddDays(365), TODAY.AddDays(730)}, g,
                                                                         {TODAY}, Matrix_<>(1, 1, 1.0), Matrix_<>(1, 1, 1.0)));
    const MultiFactorGSRModelData_ initial("initial", original.curve_, vol);
    Vector_<GSRCalibrationQuote_> quotes;
    double variance = 0.0;
    const Vector_<> target{0.012, 0.018, 0.015};
    for (int year = 1; year <= 3; ++year) {
        variance += target[year - 1] * target[year - 1];
        const GSRBondOption_ option{TODAY.AddDays(365 * year), TODAY.AddDays(365 * (year + 1)), 0.97, OptionType_("CALL")};
        quotes.push_back({"bond" + String::FromInt(year), option,
                          std::exp(-0.03 * year) * Distribution::BlackOpt(std::exp(-0.03), std::sqrt(variance), 0.97, OptionType_("CALL")), 1e-6});
    }
    const Vector_<GSRCalibrationParameter_> parameters{{0, 0, 0.0, 0.05}, {0, 1, 0.0, 0.05}, {0, 2, 0.0, 0.05}};
    const auto result = CalibrateGSRVolatility(initial, quotes, parameters);
    ASSERT_TRUE(result.converged_) << result.terminationReason_;
    ASSERT_TRUE(result.fitWithinTolerance_);
    ASSERT_EQ(result.jacobianRank_, 3);
    for (int knot = 0; knot < 3; ++knot)
        ASSERT_NEAR(result.parameters_[knot], target[knot], 1e-9);
    const GSRBondOption_ heldOut{TODAY.AddDays(548), TODAY.AddDays(1278), 0.94, OptionType_("PUT")};
    const double expectedVariance = target[0] * target[0] + (183.0 / 365.0) * target[1] * target[1];
    const double expected =
        std::exp(-0.03 * (548.0 / 365.0)) * Distribution::BlackOpt(std::exp(-0.06), 2.0 * std::sqrt(expectedVariance), 0.94, OptionType_("PUT"));
    ASSERT_NEAR(PriceGSREuropeanOption(*result.model_, heldOut).price_, expected, 1e-10);
    GSRCalibrationSettings_ regularized;
    regularized.priorWeight_ = 0.1;
    regularized.smoothingWeight_ = 10.0;
    auto loose = quotes;
    for (auto& quote : loose)
        quote.priceScale_ = 0.001;
    const auto smooth = CalibrateGSRVolatility(initial, loose, parameters, regularized);
    ASSERT_TRUE(smooth.converged_) << smooth.terminationReason_;
    const double firstChange = smooth.parameters_[0] - g(0, 0);
    const double secondChange = smooth.parameters_[1] - g(0, 1);
    const double rawDifference = (result.parameters_[0] - g(0, 0)) - (result.parameters_[1] - g(0, 1));
    ASSERT_LT(std::abs(firstChange - secondChange), std::abs(rawDifference));
}

TEST(GSRCalibrationTest, TestUnderdeterminedFactorsReportRankDeficiency) {
    const auto original = EuropeanModel();
    Matrix_<> correlation(2, 2, 0.0);
    correlation(0, 0) = correlation(1, 1) = 1.0;
    const Handle_<MultiFactorGSRVolData_> vol(
        new MultiFactorGSRVolData_("vol", {"first", "second"}, {TODAY}, Matrix_<>(2, 1, 0.01), {TODAY}, Matrix_<>(2, 1, 1.0), correlation));
    const MultiFactorGSRModelData_ initial("initial", original.curve_, vol);
    const GSRBondOption_ option{TODAY.AddDays(365), TODAY.AddDays(730), 0.97, OptionType_("CALL")};
    const double target = std::exp(-0.03) * Distribution::BlackOpt(std::exp(-0.03), 0.02, 0.97, OptionType_("CALL"));
    GSRCalibrationSettings_ settings;
    settings.priorWeight_ = 1e-4;
    const auto result = CalibrateGSRVolatility(initial, {{"bond", option, target, 1e-6}}, {{0, 0, 0.0, 0.1}, {1, 0, 0.0, 0.1}}, settings);
    ASSERT_TRUE(result.converged_) << result.terminationReason_;
    ASSERT_EQ(result.jacobianRank_, 1);
    ASSERT_TRUE(std::isinf(result.jacobianConditionEstimate_));
    ASSERT_TRUE(result.fitWithinTolerance_);
}

TEST(GSRCalibrationTest, TestDifferentMaturitiesIdentifyTwoFactors) {
    const auto original = EuropeanModel();
    Matrix_<> g(2, 1, 0.009), h(2, 2), correlation(2, 2, 0.2);
    h(0, 0) = h(0, 1) = 1.0;
    h(1, 0) = 0.8;
    h(1, 1) = -0.3;
    correlation(0, 0) = correlation(1, 1) = 1.0;
    const Handle_<MultiFactorGSRVolData_> vol(
        new MultiFactorGSRVolData_("vol", {"level", "slope"}, {TODAY}, g, {TODAY, TODAY.AddDays(1095)}, h, correlation));
    const MultiFactorGSRModelData_ initial("initial", original.curve_, vol);
    Vector_<GSRCalibrationQuote_> quotes;
    for (int maturity : {2, 4}) {
        const double b0 = maturity - 1.0, b1 = maturity == 2 ? 0.8 : 1.3;
        const double variance = std::pow(b0 * 0.015, 2) + std::pow(b1 * 0.025, 2) + 2.0 * 0.2 * b0 * b1 * 0.015 * 0.025;
        const GSRBondOption_ option{TODAY.AddDays(365), TODAY.AddDays(365 * maturity), 0.97, OptionType_("CALL")};
        quotes.push_back({"bond" + String::FromInt(maturity), option,
                          std::exp(-0.03) * Distribution::BlackOpt(std::exp(-0.03 * (maturity - 1)), std::sqrt(variance), 0.97, OptionType_("CALL")),
                          1e-6});
    }
    const auto result = CalibrateGSRVolatility(initial, quotes, {{0, 0, 0.0, 0.1}, {1, 0, 0.0, 0.1}});
    ASSERT_TRUE(result.converged_) << result.terminationReason_;
    ASSERT_TRUE(result.fitWithinTolerance_);
    ASSERT_EQ(result.jacobianRank_, 2);
    ASSERT_NEAR(result.parameters_[0], 0.015, 1e-8);
    ASSERT_NEAR(result.parameters_[1], 0.025, 1e-8);
}

TEST(GSRCalibrationTest, TestBoundsAndNonConvergenceAreExplicit) {
    const auto initial = EuropeanModel(0.009);
    const GSRBondOption_ option{TODAY.AddDays(365), TODAY.AddDays(730), 0.97, OptionType_("CALL")};
    const double target = std::exp(-0.03) * Distribution::BlackOpt(std::exp(-0.03), 0.04, 0.97, OptionType_("CALL"));
    const Vector_<GSRCalibrationQuote_> quotes{{"bond", option, target, 1e-6}};
    const auto bounded = CalibrateGSRVolatility(initial, quotes, {{0, 0, 0.0, 0.015}});
    ASSERT_TRUE(bounded.converged_);
    ASSERT_FALSE(bounded.fitWithinTolerance_);
    ASSERT_TRUE(bounded.activeBounds_[0]);
    ASSERT_NEAR(bounded.parameters_[0], 0.015, 1e-14);
    GSRCalibrationSettings_ settings;
    settings.maxIterations_ = 1;
    ASSERT_FALSE(CalibrateGSRVolatility(initial, quotes, {{0, 0, 0.0, 0.1}}, settings).converged_);
    ASSERT_THROW(CalibrateGSRVolatility(initial, quotes, {{1, 0, 0.0, 0.1}}), Exception_);
    ASSERT_THROW(CalibrateGSRVolatility(initial, quotes, {{0, 0, 0.02, 0.1}}), Exception_);
    ASSERT_THROW(CalibrateGSRVolatility(initial, quotes, {{0, 0, 0.0, 0.1}, {0, 0, 0.0, 0.1}}), Exception_);
    auto invalid = quotes;
    invalid[0].priceScale_ = 0.0;
    ASSERT_THROW(CalibrateGSRVolatility(initial, invalid, {{0, 0, 0.0, 0.1}}), Exception_);
}

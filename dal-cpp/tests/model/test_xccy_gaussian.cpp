#include <gtest/gtest.h>

#include <array>
#include <cmath>

#include <dal/calibration/protocol.hpp>
#include <dal/model/xccygaussianmath.hpp>

#include "xccygaussiankerneloracle.hpp"

using Dal::Matrix_;
using Dal::Vector_;
using namespace XccyGaussianTestSupport;

TEST(XccyGaussianTest, TestExactJointTransitionAgainstBrownianKernels) {
    const auto correlation = Correlation();
    const std::array<double, 3> sigma{.01, .009, .12};
    const auto transition = Dal::XccyGaussianDetail::ConstantTransition(.03, .07, sigma, correlation, 1.0);
    const int driver[5] = {0, 1, 0, 1, 2};
    for (int i = 0; i < 5; ++i)
        for (int j = 0; j < 5; ++j) {
            const long double kernel = KernelIntegral(i, j, .03, .07, 1.L, 48);
            const long double refined = KernelIntegral(i, j, .03, .07, 1.L, 64);
            ASSERT_NEAR(static_cast<double>(kernel - refined), 0., 1e-16);
            const long double expected = refined * sigma[driver[i]] * sigma[driver[j]] * correlation(driver[i], driver[j]);
            ASSERT_NEAR(transition.covariance_(i, j), static_cast<double>(expected), 1e-16);
        }
    const double quanto = -.25 * .009 * .12;
    ASSERT_NEAR(transition.shift_[1], -quanto * static_cast<double>(KernelIntegral(1, 4, .03, .07, 1.L, 64)), 1e-16);
    ASSERT_NEAR(transition.shift_[3], -quanto * static_cast<double>(KernelIntegral(3, 4, .03, .07, 1.L, 64)), 1e-16);
}

TEST(XccyGaussianTest, TestSmallPositiveVarianceAndCrossTermSurviveFactorization) {
    const auto correlation = Correlation();
    const std::array<double, 3> sigma{1e-8, .009, .12};
    const auto transition = Dal::XccyGaussianDetail::ConstantTransition(.03, .07, sigma, correlation, 1.0);
    const auto lower = Dal::XccyGaussianDetail::ScaledFactor(transition.covariance_);
    ASSERT_GT(lower(0, 0), 0.);
    ASSERT_NEAR(transition.covariance_(0, 0), 9.70591106929188e-17, 1e-29);
    ASSERT_NEAR(transition.covariance_(0, 4), -2.36435731611935e-10, 1e-22);
    const int driver[5] = {0, 1, 0, 1, 2};
    for (int i = 0; i < 5; ++i)
        for (int j = 0; j < 5; ++j) {
            long double reconstruction = 0.L;
            for (int k = 0; k < 5; ++k)
                reconstruction += static_cast<long double>(lower(i, k)) * lower(j, k);
            const long double reference =
                KernelIntegral(i, j, .03, .07, 1.L, 64) * sigma[driver[i]] * sigma[driver[j]] * correlation(driver[i], driver[j]);
            const long double scale =
                std::sqrt(KernelIntegral(i, i, .03, .07, 1.L, 64) * KernelIntegral(j, j, .03, .07, 1.L, 64)) * sigma[driver[i]] * sigma[driver[j]];
            ASSERT_NEAR(static_cast<double>((transition.covariance_(i, j) - reference) / scale), 0., 1e-12);
            ASSERT_NEAR(static_cast<double>((reconstruction - reference) / scale), 0., 1e-12);
        }
    // The forbidden global-tau pivot truncation loses this entire cross term.
    const double tau = 64. * std::numeric_limits<double>::epsilon() * transition.covariance_(4, 4);
    ASSERT_LT(transition.covariance_(0, 0), tau);
    auto truncated = lower;
    for (int i = 0; i < 5; ++i)
        truncated(i, 0) = 0.;
    double wrongCross = 0.;
    for (int k = 0; k < 5; ++k)
        wrongCross += truncated(0, k) * truncated(4, k);
    ASSERT_GT(std::abs(wrongCross - transition.covariance_(0, 4)) / std::sqrt(transition.covariance_(0, 0) * transition.covariance_(4, 4)), 1e-12);
}

TEST(XccyGaussianTest, TestContinuousZeroMeanReversionAndSplitComposition) {
    const auto correlation = Correlation();
    const std::array<double, 3> sigma{.01, .009, .12};
    for (const double a : {0., 1e-12, .03, 1., 60.}) {
        const auto whole = Dal::XccyGaussianDetail::ConstantTransition(a, a, sigma, correlation, 1.);
        const auto first = Dal::XccyGaussianDetail::ConstantTransition(a, a, sigma, correlation, .37);
        const auto second = Dal::XccyGaussianDetail::ConstantTransition(a, a, sigma, correlation, .63);
        const auto joined = Dal::XccyGaussianDetail::Compose(first, second);
        for (int i = 0; i < 5; ++i) {
            ASSERT_NEAR(joined.shift_[i], whole.shift_[i], 1e-16);
            for (int j = 0; j < 5; ++j) {
                ASSERT_NEAR(joined.flow_(i, j), whole.flow_(i, j), 1e-14);
                ASSERT_NEAR(joined.covariance_(i, j), whole.covariance_(i, j), 1e-15);
                const long double reference = KernelIntegral(i, j, a, a, 1.L, 64) * sigma[Dal::XccyGaussianDetail::Driver(i)] *
                                              sigma[Dal::XccyGaussianDetail::Driver(j)] *
                                              correlation(Dal::XccyGaussianDetail::Driver(i), Dal::XccyGaussianDetail::Driver(j));
                ASSERT_NEAR(whole.covariance_(i, j), static_cast<double>(reference), 1e-15);
            }
        }
    }
}

TEST(XccyGaussianTest, TestZeroVolatilityRowsKeepFiveSlotsAndTrueRankFailure) {
    const auto correlation = Correlation();
    for (const std::array<double, 3>& sigma : {std::array<double, 3>{0., 0., 0.}, {0., .009, .12}, {.01, 0., .12}, {.01, .009, 0.}}) {
        const auto transition = Dal::XccyGaussianDetail::ConstantTransition(.03, .07, sigma, correlation, 1.);
        const auto lower = Dal::XccyGaussianDetail::ScaledFactor(transition.covariance_);
        ASSERT_EQ(lower.Rows(), 5);
        ASSERT_EQ(lower.Cols(), 5);
        for (int i = 0; i < 5; ++i)
            for (int j = 0; j < 5; ++j) {
                double reconstruction = 0.;
                for (int k = 0; k < 5; ++k)
                    reconstruction += lower(i, k) * lower(j, k);
                ASSERT_NEAR(reconstruction, transition.covariance_(i, j), 1e-16);
                if (sigma[Dal::XccyGaussianDetail::Driver(i)] == 0.)
                    ASSERT_DOUBLE_EQ(reconstruction, 0.);
            }
    }
    Matrix_<> inconsistent(5, 5, 0.);
    inconsistent(1, 0) = 1e-30;
    ASSERT_THROW(Dal::XccyGaussianDetail::ScaledFactor(inconsistent), Dal::CalibrationError_);
}

TEST(XccyGaussianTest, TestUnderflowIsNumericalFailureRatherThanZeroVolatility) {
    const auto correlation = Correlation();
    const std::array<double, 3> sigma{1e-200, .009, .12};
    ASSERT_THROW((void)Dal::XccyGaussianDetail::ConstantTransition(.03, .07, sigma, correlation, 1.), Dal::CalibrationError_);
}

TEST(XccyGaussianTest, TestUnequalBucketCompositionRetainsSymmetricFactorizableCovariance) {
    const auto correlation = Correlation();
    const auto first = Dal::XccyGaussianDetail::ConstantTransition(.13, .91, std::array<double, 3>{.013, .021, .07}, correlation, .37);
    const auto second = Dal::XccyGaussianDetail::ConstantTransition(.13, .91, std::array<double, 3>{.034, .009, .19}, correlation, .83);
    const auto joint = Dal::XccyGaussianDetail::Compose(first, second);
    ASSERT_NO_THROW((void)Dal::XccyGaussianDetail::ScaledFactor(joint.covariance_));
}

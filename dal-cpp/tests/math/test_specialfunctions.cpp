//
// Created by wegam on 2020/12/17.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/math/vectors.hpp>
#include <dal/math/specialfunctions.hpp>
#include <dal/utilities/algorithms.hpp>

using namespace Dal;

TEST(SpecialFunctionsTest, TestNCDF) {
    double x_min = -6.;
    double x_max = 6.;

    size_t n = 100001;

    auto x = Vector::XRange(x_min, x_max, n);
    Vector_<> y(n);
    Vector_<> z(n);
    Transform(x, [](double z) { return NCDF(z); }, &y);
    Transform(y, [](double z) { return InverseNCDF(z); }, &z);
    for (size_t i = 0; i != n; ++i)
        ASSERT_NEAR(x[i], z[i], 1e-6);

    x_min = -3.;
    x_max = 3.;
    x = Vector::XRange(x_min, x_max, n);
    Transform(x, [](double z) { return NCDF(z, false); }, &y);
    Transform(y, [](double z) { return InverseNCDF(z, false, true); }, &z);
    for (size_t i = 0; i != n; ++i)
        ASSERT_NEAR(x[i], z[i], 1e-6);
}

TEST(SpecialFunctionsTest, TestInverseNCDFPublishedQuantiles) {
    struct Quantile_ {
        double probability_;
        double quantile_;
    };
    const Quantile_ cases[] = {
        {0.001, -3.0902323061678132}, {0.025, -1.959963984540054}, {0.25, -0.6744897501960817}, {0.5, 0.0},
        {0.75, 0.6744897501960817},   {0.975, 1.959963984540054},  {0.999, 3.0902323061678132},
    };

    for (const auto& testCase : cases) {
        ASSERT_NEAR(InverseNCDF(testCase.probability_, false, false), testCase.quantile_, 5e-9);
        ASSERT_NEAR(InverseNCDF(testCase.probability_, true, true), testCase.quantile_, 5e-14);
    }
}

TEST(SpecialFunctionsTest, TestFastInverseNormalTailsAndJoinBoundaries) {
    for (const double probability : {1e-12, 1.0 / 4294967088.0, 0.5 / (1 << 30), 0.02425 - 1e-12, 0.02425, 0.02425 + 1e-12, 0.5, 0.97575 - 1e-12,
                                     0.97575, 0.97575 + 1e-12, 1.0 - 1e-12}) {
        const double fast = InverseNCDF(probability, false, false);
        const double precise = InverseNCDF(probability, true, true);
        ASSERT_TRUE(std::isfinite(fast));
        ASSERT_NEAR(fast, precise, 2e-8);
        ASSERT_NEAR(NCDF(fast, true), probability, 2e-16 + 1e-8 * std::min(probability, 1.0 - probability));
    }
}

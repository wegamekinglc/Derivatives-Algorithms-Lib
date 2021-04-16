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
    Transform(x, [](double z) { return NCDF(z);}, &y);
    Transform(y, [](double z) { return InverseNCDF(z);}, &z);
    for (size_t i = 0; i != n; ++i)
        ASSERT_NEAR(x[i], z[i], 1e-6);

    x_min = -3.;
    x_max = 3.;
    x = Vector::XRange(x_min, x_max, n);
    Transform(x, [](double z) { return NCDF(z, false);}, &y);
    Transform(y, [](double z) { return InverseNCDF(z, false, true);}, &z);
    for (size_t i = 0; i != n; ++i)
        ASSERT_NEAR(x[i], z[i], 1e-6);
}

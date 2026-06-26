//
// Created by wegam on 2026/4/26.
//

#include <gtest/gtest.h>
#include <cmath>

#include <dal/platform/platform.hpp>
#include <dal/math/integral/quadrature.hpp>

using Dal::Quad1DBase_;
using Dal::Quad1DFixed_;
using Dal::NormalExpectation_;
using Dal::QuadSimpson_;
using Dal::Quadrature::NCDFGaussHermiteWeights;
using Dal::Quadrature::SimpsonWeights;
using Dal::Quadrature::Increment;
using Dal::Vector_;

namespace {
    double Square(double x) { return x * x; }
    double Cube(double x) { return x * x * x; }
    double Quartic(double x) { return x * x * x * x; }
} // namespace

TEST(QuadratureTest, TestIncrementScalar) {
    double dst = 1.0;
    Increment(&dst, 2.0, 0.5);
    ASSERT_DOUBLE_EQ(dst, 2.0);
}

TEST(QuadratureTest, TestIncrementVector) {
    Vector_<> dst = {0.0, 0.0, 0.0};
    Vector_<> inc = {1.0, 2.0, 3.0};
    Increment(&dst, inc, 0.5);
    ASSERT_NEAR(dst[0], 0.5, 1e-10);
    ASSERT_NEAR(dst[1], 1.0, 1e-10);
    ASSERT_NEAR(dst[2], 1.5, 1e-10);
}

TEST(QuadratureTest, TestSimpsonWeightsBasic) {
    Vector_<> x(3), w(3);
    SimpsonWeights(3, 0.0, 1.0, &x, &w);

    ASSERT_NEAR(x[0], 0.0, 1e-10);
    ASSERT_NEAR(x[1], 0.5, 1e-10);
    ASSERT_NEAR(x[2], 1.0, 1e-10);

    const double dx = 0.5;
    ASSERT_NEAR(w[0], dx / 3.0, 1e-10);
    ASSERT_NEAR(w[1], 4.0 * dx / 3.0, 1e-10);
    ASSERT_NEAR(w[2], dx / 3.0, 1e-10);
}

TEST(QuadratureTest, TestSimpsonWeightsOddify) {
    // passing an even n should be corrected to odd
    Vector_<> x(5), w(5);
    SimpsonWeights(4, 0.0, 1.0, &x, &w);
    ASSERT_EQ(x.size(), 5u);
    ASSERT_NEAR(x[4], 1.0, 1e-10);
}

TEST(QuadratureTest, TestSimpsonWeightsSum) {
    for (int n : {3, 5, 7, 9}) {
        Vector_<> x(n), w(n);
        SimpsonWeights(n, 0.0, 4.0, &x, &w);
        double sum = 0.0;
        for (int i = 0; i < n; ++i)
            sum += w[i];
        ASSERT_NEAR(sum, 4.0, 1e-10);
    }
}

TEST(QuadratureTest, TestSimpsonConstant) {
    QuadSimpson_<> quad(3, 0.0, 1.0);
    while (!quad.IsComplete())
        quad.PutY(1.0);
    ASSERT_NEAR(quad.Result(), 1.0, 1e-10);
}

TEST(QuadratureTest, TestSimpsonLinear) {
    QuadSimpson_<> quad(3, 0.0, 1.0);
    while (!quad.IsComplete()) {
        double x = quad.GetX();
        quad.PutY(x);
    }
    ASSERT_NEAR(quad.Result(), 0.5, 1e-10);
}

TEST(QuadratureTest, TestSimpsonQuadratic) {
    QuadSimpson_<> quad(3, 0.0, 1.0);
    while (!quad.IsComplete()) {
        double x = quad.GetX();
        quad.PutY(Square(x));
    }
    ASSERT_NEAR(quad.Result(), 1.0 / 3.0, 1e-10);
}

TEST(QuadratureTest, TestSimpsonCubic) {
    QuadSimpson_<> quad(3, 0.0, 1.0);
    while (!quad.IsComplete()) {
        double x = quad.GetX();
        quad.PutY(Cube(x));
    }
    ASSERT_NEAR(quad.Result(), 0.25, 1e-10);
}

TEST(QuadratureTest, TestHermiteOneNode) {
    Vector_<> x(1), w(1);
    NCDFGaussHermiteWeights(&x, &w);
    ASSERT_NEAR(x[0], 0.0, 1e-10);
    ASSERT_NEAR(w[0], 1.0, 1e-10);
}

TEST(QuadratureTest, TestHermiteTwoNodes) {
    Vector_<> x(2), w(2);
    NCDFGaussHermiteWeights(&x, &w);
    ASSERT_NEAR(x[0], -1.0, 1e-10);
    ASSERT_NEAR(x[1], 1.0, 1e-10);
    ASSERT_NEAR(w[0], 0.5, 1e-10);
    ASSERT_NEAR(w[1], 0.5, 1e-10);
}

TEST(QuadratureTest, TestHermiteWeightsSumToOne) {
    for (int n : {1, 2, 3, 5, 7, 10}) {
        Vector_<> x(n), w(n);
        NCDFGaussHermiteWeights(&x, &w);
        double sum = 0.0;
        for (int i = 0; i < n; ++i)
            sum += w[i];
        ASSERT_NEAR(sum, 1.0, 1e-10) << "n=" << n;
    }
}

TEST(QuadratureTest, TestHermiteSymmetry) {
    for (int n : {3, 5, 7}) {
        Vector_<> x(n), w(n);
        NCDFGaussHermiteWeights(&x, &w);
        for (int i = 0; i < n / 2; ++i) {
            int j = n - 1 - i;
            ASSERT_NEAR(x[i], -x[j], 1e-10) << "n=" << n << " i=" << i;
            ASSERT_NEAR(w[i], w[j], 1e-10) << "n=" << n << " i=" << i;
        }
    }
}

TEST(QuadratureTest, TestNormalExpectConstant) {
    NormalExpectation_<> quad(5);
    while (!quad.IsComplete())
        quad.PutY(1.0);
    ASSERT_NEAR(quad.Result(), 1.0, 1e-10);
}

TEST(QuadratureTest, TestNormalExpectX) {
    NormalExpectation_<> quad(5);
    while (!quad.IsComplete()) {
        double x = quad.GetX();
        quad.PutY(x);
    }
    ASSERT_NEAR(quad.Result(), 0.0, 1e-10);
}

TEST(QuadratureTest, TestNormalExpectXSquared) {
    NormalExpectation_<> quad(5);
    while (!quad.IsComplete()) {
        double x = quad.GetX();
        quad.PutY(Square(x));
    }
    ASSERT_NEAR(quad.Result(), 1.0, 1e-8);
}

TEST(QuadratureTest, TestNormalExpectXFourth) {
    NormalExpectation_<> quad(5);
    while (!quad.IsComplete()) {
        double x = quad.GetX();
        quad.PutY(Quartic(x));
    }
    ASSERT_NEAR(quad.Result(), 3.0, 1e-8);
}

TEST(QuadratureTest, TestNormalExpectOddFunction) {
    // E[sin(x)] = 0 for standard normal (odd function)
    NormalExpectation_<> quad(5);
    while (!quad.IsComplete()) {
        double x = quad.GetX();
        quad.PutY(sin(x));
    }
    ASSERT_NEAR(quad.Result(), 0.0, 1e-10);
}

TEST(QuadratureTest, TestNormalExpectConvergence) {
    // as n increases, E[x^6] should converge to 15
    {
        NormalExpectation_<> quad(5);
        while (!quad.IsComplete()) {
            double x = quad.GetX();
            quad.PutY(x * x * x * x * x * x);
        }
        ASSERT_NEAR(quad.Result(), 15.0, 1e-10);
    }
    {
        NormalExpectation_<> quad(7);
        while (!quad.IsComplete()) {
            double x = quad.GetX();
            quad.PutY(x * x * x * x * x * x);
        }
        ASSERT_NEAR(quad.Result(), 15.0, 1e-10);
    }
}

TEST(QuadratureTest, TestStateTransitions) {
    QuadSimpson_<> quad(3, 0.0, 1.0);
    ASSERT_FALSE(quad.IsComplete());
    quad.PutY(1.0);
    quad.PutY(1.0);
    ASSERT_FALSE(quad.IsComplete());
    quad.PutY(1.0);
    ASSERT_TRUE(quad.IsComplete());

    quad.Restart();
    ASSERT_FALSE(quad.IsComplete());
    double x = quad.GetX();
    ASSERT_NEAR(x, 0.0, 1e-10);
}

TEST(QuadratureTest, TestSimpsonRestartReuse) {
    QuadSimpson_<> quad(5, 0.0, 1.0);
    while (!quad.IsComplete())
        quad.PutY(Square(quad.GetX()));
    double first = quad.Result();

    quad.Restart();
    while (!quad.IsComplete())
        quad.PutY(Square(quad.GetX()));
    double second = quad.Result();

    ASSERT_NEAR(first, second, 1e-10);
}

TEST(QuadratureTest, TestVectorValuedSimpson) {
    QuadSimpson_<Vector_<>> quad(3, 0.0, 1.0, Vector_<>(2));
    while (!quad.IsComplete()) {
        Vector_<> y(2);
        double x = quad.GetX();
        y[0] = x;
        y[1] = x * x;
        quad.PutY(y);
    }
    Vector_<> result = quad.Result();
    ASSERT_NEAR(result[0], 0.5, 1e-10);
    ASSERT_NEAR(result[1], 1.0 / 3.0, 1e-10);
}

TEST(QuadratureTest, TestVectorValuedNormal) {
    NormalExpectation_<Vector_<>> quad(5, Vector_<>(2));
    while (!quad.IsComplete()) {
        Vector_<> y(2);
        double x = quad.GetX();
        y[0] = 1.0;
        y[1] = Square(x);
        quad.PutY(y);
    }
    Vector_<> result = quad.Result();
    ASSERT_NEAR(result[0], 1.0, 1e-10);
    ASSERT_NEAR(result[1], 1.0, 1e-8);
}

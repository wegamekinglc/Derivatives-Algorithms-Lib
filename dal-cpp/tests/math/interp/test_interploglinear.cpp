//
// Created by wegam on 2026/4/10.
//

#include <gtest/gtest.h>
#include <dal/math/interp/interploglinear.hpp>
#include <dal/storage/json.hpp>
#include <dal/storage/splat.hpp>
#include <cmath>

using namespace Dal;

TEST(InterpTest, TestNewLogLinear) {
    Vector_<> x = {0.0, 1.0, 2.0, 3.0};
    Vector_<> f = {1.0, 2.0, 4.0, 8.0};

    Handle_<Interp1_> interp(Interp::NewLogLinear("test", x, f));

    // exact at knots
    for (int i = 0; i < x.size(); ++i)
        ASSERT_NEAR((*interp)(x[i]), f[i], 1e-10);

    // mid-point: exp of linear interp of logs
    double mid = 0.5;
    double expected = std::exp(0.5 * std::log(1.0) + 0.5 * std::log(2.0));
    ASSERT_NEAR((*interp)(mid), expected, 1e-10);
}

TEST(InterpTest, TestNewLogLinearRejectsNonPositiveF) {
    Vector_<> x = {0.0, 1.0, 2.0, 3.0};

    Vector_<> zero_f = {1.0, 0.0, 4.0, 8.0};
    ASSERT_THROW(Interp::NewLogLinear("test", x, zero_f), Dal::Exception_);

    Vector_<> negative_f = {1.0, -2.0, 4.0, 8.0};
    ASSERT_THROW(Interp::NewLogLinear("test", x, negative_f), Dal::Exception_);
}

TEST(InterpTest, TestNewLogLinearRejectsNonMonotonicX) {
    Vector_<> non_monotonic_x = {0.0, 2.0, 1.0, 3.0};
    Vector_<> f = {1.0, 2.0, 4.0, 8.0};

    ASSERT_THROW(Interp::NewLogLinear("test", non_monotonic_x, f), Dal::Exception_);
}

TEST(InterpTest, TestNewLogLinearRejectsMismatchedSizes) {
    Vector_<> x = {0.0, 1.0, 2.0, 3.0};
    Vector_<> f = {1.0, 2.0, 4.0};

    ASSERT_THROW(Interp::NewLogLinear("test", x, f), Dal::Exception_);
}

TEST(InterpTest, TestLogLinearSplatSerialization) {
    Vector_<> x = {0.0, 1.0, 2.0, 3.0};
    Vector_<> f = {1.0, 2.0, 4.0, 8.0};

    Handle_<Interp1_> src(Interp::NewLogLinear("test", x, f));

    auto dst = Splat(*src);
    Handle_<Storable_> rtn = UnSplat(dst, true);
    Handle_<Interp1_> val(std::dynamic_pointer_cast<const Interp1_>(rtn));

    ASSERT_TRUE(val.get() != nullptr);
    for (int i = 0; i < x.size(); ++i)
        ASSERT_NEAR((*val)(x[i]), f[i], 1e-10);
    ASSERT_NEAR((*val)(0.5), (*src)(0.5), 1e-10);
}

TEST(InterpTest, TestLogLinearJsonSerialization) {
    Vector_<> x = {0.0, 1.0, 2.0, 3.0};
    Vector_<> f = {1.0, 2.0, 4.0, 8.0};

    Handle_<Interp1_> src(Interp::NewLogLinear("test", x, f));

    auto json = JSON::WriteString(*src);
    Handle_<Storable_> rtn = JSON::ReadString(json, true);
    Handle_<Interp1_> val(std::dynamic_pointer_cast<const Interp1_>(rtn));

    ASSERT_TRUE(val.get() != nullptr);
    for (int i = 0; i < x.size(); ++i)
        ASSERT_NEAR((*val)(x[i]), f[i], 1e-10);
    ASSERT_NEAR((*val)(0.5), (*src)(0.5), 1e-10);
}

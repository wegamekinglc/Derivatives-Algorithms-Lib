//
// Created by wegam on 2020/11/10.
//

#include <gtest/gtest.h>
#include <dal/math/interp/interplinear.hpp>
#include <dal/platform/platform.hpp>
#include <dal/storage/json.hpp>
#include <dal/storage/splat.hpp>

using Dal::Interp1Linear_;
using Dal::Vector_;
using Dal::String_;
using Dal::Interp::NewLinear;
using Dal::Interp1_;
using Dal::Handle_;
using Dal::Storable_;

TEST(InterpTest, TestInterpLinearImplXWithSmooth) {
    Vector_<> x = {1., 2., 3., 4., 5.};
    Vector_<> f = {2.5, 3.5, 1.7, 2.8, 3.6};

    auto calculated = Dal::InterpLinearImplX<>(x, f, (x[2] + x[3]) / 2.0);
    auto expected = (f[2] + f[3]) / 2.;
    ASSERT_DOUBLE_EQ(calculated, expected);
}

TEST(InterpTest, TestInterp1Linear) {
    Vector_<> x = {1., 2., 3., 4., 5.};
    Vector_<> f = {2.5, 3.5, 1.7, 2.8, 3.6};

    Interp1Linear_ interp("interp", x, f);

    ASSERT_DOUBLE_EQ(f[2], interp(x[2]));
    ASSERT_DOUBLE_EQ(f[0], interp(x[0] - 1.));
    ASSERT_DOUBLE_EQ(f[4], interp(x[4] + 1.));
    ASSERT_DOUBLE_EQ((f[2] + f[3]) / 2., interp((x[2] + x[3]) / 2.));
}

TEST(InterpTest, TestWithMapInterp1Linear) {
    Vector_<> x = {1., 2., 3., 4., 5.};
    Vector_<> f = {2.5, 3.5, 1.7, 2.8, 3.6};
    std::map<double, double> values = {{x[0], f[0]},
                                       {x[1], f[1]},
                                       {x[2], f[2]},
                                       {x[3], f[3]},
                                       {x[4], f[4]}};

    Interp1Linear_ interp("interp", x, f);

    ASSERT_DOUBLE_EQ(f[2], interp(x[2]));
    ASSERT_DOUBLE_EQ(f[0], interp(x[0] - 1.));
    ASSERT_DOUBLE_EQ(f[4], interp(x[4] + 1.));
    ASSERT_DOUBLE_EQ((f[2] + f[3]) / 2., interp((x[2] + x[3]) / 2.));
}

TEST(InterpTest, TestInterp1LinearWithUnorderedX) {
    Vector_<> x = {2., 1., 3., 4., 5.};
    Vector_<> f = {3.5, 2.5, 1.7, 2.8, 3.6};

    ASSERT_THROW(Interp1Linear_ interp("interp", x, f), Dal::Exception_);
}

TEST(InterpTest, TestNewLinear) {
    Vector_<> x = {1., 2., 3., 4., 5.};
    Vector_<> f = {2.5, 3.5, 1.7, 2.8, 3.6};
    Handle_<Interp1_> interp(NewLinear("interp", x ,f));

    ASSERT_EQ(interp->name_, "interp");
    ASSERT_DOUBLE_EQ(f[2], (*interp)(x[2]));
    ASSERT_DOUBLE_EQ(f[0], (*interp)(x[0] - 1.));
    ASSERT_DOUBLE_EQ(f[4], (*interp)(x[4] + 1.));
    ASSERT_DOUBLE_EQ((f[2] + f[3]) / 2., (*interp)((x[2] + x[3]) / 2.));
}

TEST(InterpTest, TestInterp1LinearSplatSerialization) {
    Vector_<> x = {1., 2., 3., 4., 5.};
    Vector_<> f = {2.5, 3.5, 1.7, 2.8, 3.6};

    Interp1Linear_ src("interp", x, f);
    auto dst = Dal::Splat(src);
    Handle_<Storable_> rtn = Dal::UnSplat(dst, true);
    Handle_<Interp1Linear_> val(std::dynamic_pointer_cast<const Interp1Linear_>(rtn));

    ASSERT_TRUE(val.get() != nullptr);
    ASSERT_EQ(x, val->x());
    ASSERT_EQ(f, val->f());
    ASSERT_DOUBLE_EQ(src(2.5), (*val)(2.5));
}

TEST(InterpTest, TestInterp1LinearJsonSerialization) {
    Vector_<> x = {1., 2., 3., 4., 5.};
    Vector_<> f = {2.5, 3.5, 1.7, 2.8, 3.6};

    Interp1Linear_ src("interp", x, f);
    auto json = Dal::JSON::WriteString(src);
    Handle_<Storable_> rtn = Dal::JSON::ReadString(json, true);
    Handle_<Interp1Linear_> val(std::dynamic_pointer_cast<const Interp1Linear_>(rtn));

    ASSERT_TRUE(val.get() != nullptr);
    ASSERT_EQ(x, val->x());
    ASSERT_EQ(f, val->f());
    ASSERT_DOUBLE_EQ(src(2.5), (*val)(2.5));
}

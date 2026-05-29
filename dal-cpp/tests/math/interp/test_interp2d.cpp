//
// Created by wegam on 2022/5/11.
//

#include <dal/math/interp/interp2d.hpp>
#include <dal/platform/platform.hpp>
#include <gtest/gtest.h>

using Dal::Interp2Linear_;
using Dal::Matrix_;
using Dal::Vector_;
using Dal::Interp2_;
using Dal::Handle_;
using Dal::Interp::NewLinear2;

TEST(InterpTest, TestInterp2Linear) {
    Vector_<> x = {1., 2.};
    Vector_<> y = {1., 2.};
    Matrix_<> f(2, 2);
    f(0, 0) = 1.0; f(0, 1) = 2.0;
    f(1, 0) = 3.0; f(1, 1) = 4.0;
    Interp2Linear_ interp("interp", x, y, f);
    ASSERT_DOUBLE_EQ(f(0, 1), interp(x[0], y[1]));
}

TEST(InterpTest, TestInterp2LinearWithUnorderedX) {
    Vector_<> x = {1., 2.};
    Vector_<> y = {2., 1.};
    Matrix_<> f(2, 2);
    ASSERT_THROW(Interp2Linear_ interp("interp", x, y, f);, Dal::Exception_);
}

TEST(InterpTest, TestNewLinear2) {
    Vector_<> x = {1., 2.};
    Vector_<> y = {1., 2.};
    Matrix_<> f(2, 2);
    Handle_<Interp2_> interp(NewLinear2("interp", x ,y, f));

    ASSERT_EQ(interp->name_, "interp");
    ASSERT_DOUBLE_EQ(f(0, 1), (*interp)(x[0], y[1]));
}

//
// Created by wegam on 2020/12/17.
//

#include <gtest/gtest.h>
#include <dal/math/interp/interpcubic.hpp>
#include <dal/math/vectors.hpp>
#include <dal/storage/json.hpp>
#include <dal/storage/splat.hpp>
#include <cmath>

using namespace Dal;

Vector_<> Gaussian(const Vector_<>& x) {
    Vector_<> y(x.size());
    for (int i = 0; i < x.size(); ++i)
        y[i] = std::exp(-x[i] * x[i]);
    return y;
}

class ErrorFunction_ {
public:
    explicit ErrorFunction_(const Handle_<Interp1_>& f) : f_(f) {}
    double operator()(double x) const {
        double temp = (*f_)(x) - std::exp(-x*x);
        return temp * temp;
    }
private:
    Handle_<Interp1_> f_;
};


TEST(InterpTest, TestNewCubic) {
    int points[] = {5, 9, 17, 33};
    Vector_<> x, y;

    for (const auto n : points) {
        Vector_<> x = Vector::XRange(-1.7, 1.9, n);
        Vector_<> y = Gaussian(x);

        // Not-a-knot
        Interp::Boundary_ lhs(2, 0.);
        Interp::Boundary_ rhs(2, 0);
        Handle_<Interp1_> interp(Interp::NewCubic("interp", x, y, lhs, rhs));
        ErrorFunction_ func(interp);
        ASSERT_DOUBLE_EQ(func(x[0]), 0.0);
    }
}

TEST(InterpTest, TestCubicSplatSerialization) {
    Vector_<> x = Vector::XRange(-1.7, 1.9, 9);
    Vector_<> y = Gaussian(x);

    Interp::Boundary_ lhs(2, 0.);
    Interp::Boundary_ rhs(2, 0.);
    Handle_<Interp1_> src(Interp::NewCubic("interp", x, y, lhs, rhs));

    auto dst = Splat(*src);
    Handle_<Storable_> rtn = UnSplat(dst, true);
    Handle_<Interp1_> val(std::dynamic_pointer_cast<const Interp1_>(rtn));

    ASSERT_TRUE(val.get() != nullptr);
    double test_x = 0.5;
    ASSERT_NEAR((*val)(test_x), (*src)(test_x), 1e-10);
}

TEST(InterpTest, TestCubicJsonSerialization) {
    Vector_<> x = Vector::XRange(-1.7, 1.9, 9);
    Vector_<> y = Gaussian(x);

    Interp::Boundary_ lhs(2, 0.);
    Interp::Boundary_ rhs(2, 0.);
    Handle_<Interp1_> src(Interp::NewCubic("interp", x, y, lhs, rhs));

    auto json = JSON::WriteString(*src);
    Handle_<Storable_> rtn = JSON::ReadString(json, true);
    Handle_<Interp1_> val(std::dynamic_pointer_cast<const Interp1_>(rtn));

    ASSERT_TRUE(val.get() != nullptr);
    double test_x = 0.5;
    ASSERT_NEAR((*val)(test_x), (*src)(test_x), 1e-10);
}
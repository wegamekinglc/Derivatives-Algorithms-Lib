//
// Created by wegam on 2020/12/17.
//

#include <cmath>
#include <dal/math/interp/interpcubic.hpp>
#include <dal/math/vectors.hpp>
#include <gtest/gtest.h>

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
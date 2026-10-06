//
// Created by wegam on 2020/12/17.
//

#include <gtest/gtest.h>

#include <cmath>
#include <limits>

#include <dal/math/interp/interpcubic.hpp>
#include <dal/math/vectors.hpp>
#include <dal/storage/json.hpp>
#include <dal/storage/splat.hpp>

using namespace Dal;

TEST(InterpTest, TestCubicArchiveRejectsInvalidData) {
    for (const auto* payload :
         {R"({"~type":"Cubic1","x":[0,1,2],"f":[],"fpp":[]})", R"({"~type":"Cubic1","x":[0,1,2],"f":[1,2,3],"fpp":[0,0]})",
          R"({"~type":"Cubic1","x":[0,0,2],"f":[1,2,3],"fpp":[0,0,0]})", R"({"~type":"Cubic1","x":[0,1],"f":[1,2],"fpp":[0,0]})"})
        ASSERT_THROW(JSON::ReadString(String_(payload), true), Exception_);
}

TEST(InterpTest, TestCubicQueryIntervalsKnotsAndExtrapolation) {
    const auto stored = JSON::ReadString(
        R"({"~type":"Cubic1","x":[-2,-0.5,1.3,4],"f":[-3,1,0.5,6],"fpp":[0.2,-0.1,0.3,0.4]})", true);
    const auto cubic = std::dynamic_pointer_cast<const Interp1_>(stored);
    ASSERT_NE(cubic, nullptr);
    const Vector_<> knots{-2.0, -0.5, 1.3, 4.0};
    const Vector_<> values{-3.0, 1.0, 0.5, 6.0};
    const Vector_<> second{0.2, -0.1, 0.3, 0.4};
    for (size_t i = 0; i < knots.size(); ++i)
        ASSERT_DOUBLE_EQ((*cubic)(knots[i]), values[i]);
    for (double query : {-5.0, -1.9, -1.0, 0.0, 1.2, 1.4, 2.5, 3.9, 6.0}) {
        size_t right = 1;
        while (right < knots.size() - 1 && query > knots[right])
            ++right;
        const size_t left = right - 1;
        const double h = knots[right] - knots[left];
        const double b = (query - knots[left]) / h;
        const double a = 1.0 - b;
        const double expected = a * values[left] + b * values[right] -
                                a * b * ((1.0 + a) * second[left] + (1.0 + b) * second[right]) * (h * h) / 6.0;
        ASSERT_DOUBLE_EQ((*cubic)(query), expected);
    }
    ASSERT_TRUE(std::isnan((*cubic)(std::numeric_limits<double>::quiet_NaN())));
}

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

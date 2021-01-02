//
// Created by wegam on 2021/1/2.
//

#include <gtest/gtest.h>
#include <dal/math/rootfind.hpp>

using namespace Dal;

class F1_ {
public:
    double operator()(double x) const {
        return x * x - 1.0;
    }
};

TEST(RootFinderTest, TestBrent) {
    Brent_ finder(0.0, 1e-8, 0.01);
    Converged_ check(1e-8, 1e-8);
    F1_ func;

    double e = func(finder.NextX());
    while (!check(finder, e)) {
        e = func(finder.NextX());
    }
    ASSERT_NEAR(e, 0.0, 1e-8);
}
//
// Created by wegam on 2021/1/2.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/math/rootfind.hpp>
#include <dal/utilities/exceptions.hpp>

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

TEST(RootFinderTest, TestBrentWithNegtiveStepSize) {
    Brent_ finder(0.0, 1e-8, -0.01);
    Converged_ check(1e-8, 1e-8);
    F1_ func;

    double e = func(finder.NextX());
    while (!check(finder, e)) {
        e = func(finder.NextX());
    }
    ASSERT_NEAR(e, 0.0, 1e-8);
}


TEST(RootFinderTest, TestBracketedBrent) {
    F1_ func;
    BracketedBrent_ finder(std::make_pair(0.0, func(0.0)),
                           std::make_pair(2.0, func(2.0)),
                           1e-8);

    Converged_ check(1e-8, 1e-8);

    double e = func(finder.NextX());
    while (!check(finder, e)) {
        e = func(finder.NextX());
    }
    ASSERT_NEAR(e, 0.0, 1e-8);
}

TEST(RootFinderTest, TestBracketedBrentWithNonBracketedInterval) {
    F1_ func;
    ASSERT_THROW(BracketedBrent_(std::make_pair(0.0, func(0.0)), std::make_pair(0.2, func(0.2)), 1e-8),
                 Exception_);
}
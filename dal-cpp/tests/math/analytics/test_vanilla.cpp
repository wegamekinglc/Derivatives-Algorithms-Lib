//
// Created by wegam on 2022/9/11.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/math/aad/aad.hpp>
#include "dal/math/analytics/vanilla.hpp"

using namespace Dal::AAD;

TEST(AnalyticsTest, TestBlackScholes) {
    const auto vol = 0.2;
    const auto T = 2.0;
    const auto forward = 110.0;
    const auto strike = 120.0;

    auto call_price = BlackScholes(forward, strike, vol, T);
    ASSERT_NEAR(call_price, 8.53592506466286, 1e-10);
}

TEST(AnalyticsTest, TestBlackScholesAAD) {
    Clear(*Tape());

    Number_ vol(0.2);
    Number_ T(2.0);
    Number_ forward(110.0);
    Number_ strike(120.0);

    PutOnTape(vol);
    PutOnTape(T);
    PutOnTape(strike);
    PutOnTape(forward);
    Mark(*Tape());

    auto call_price = BlackScholes(forward, strike, vol, T);
    ASSERT_NEAR(Value(call_price), 8.53592506466286, 1e-10);
    Adjoint(call_price) = 1.0;
    PropagateToMark(*Tape());
    ASSERT_NEAR(Adjoint(forward), 0.433995720171781, 1e-8);
    ASSERT_NEAR(Adjoint(vol), 61.2095050098522, 1e-8);
    ASSERT_NEAR(Adjoint(T), 3.06047525, 1e-8);
}

TEST(AnalyticsTest, TestBachelier) {
    const auto vol = 22.0;
    const auto T = 2.0;
    const auto forward = 110.0;
    const auto strike = 120.0;

    auto call_price = Bachelier(forward, strike, vol, T);
    ASSERT_NEAR(call_price, 8.047832538, 1e-6);
}

TEST(AnalyticsTest, TestBachelierAAD) {
    Clear(*Tape());

    Number_ vol(22.0);
    Number_ T(2.0);
    Number_ forward(110.0);
    Number_ strike(120.0);

    PutOnTape(vol);
    PutOnTape(T);
    PutOnTape(strike);
    PutOnTape(forward);
    Mark(*Tape());

    auto call_price = Bachelier(forward, strike, vol, T);
    Adjoint(call_price) = 1.0;
    PropagateToMark(*Tape());

    ASSERT_NEAR(Value(call_price), 8.047832538, 1e-6);
    ASSERT_NEAR(Adjoint(forward), 0.37394902960009541, 1e-8);
    ASSERT_NEAR(Adjoint(vol), 0.53578740155317184, 1e-8);
    ASSERT_NEAR(Adjoint(T), 2.9468307085424446, 1e-8);
}

TEST(AnalyticsTest, TestIVolRejectsNonPositiveMaturity) {
    ASSERT_THROW(BlackScholesIVol(110.0, 120.0, 8.5, 0.0), Dal::Exception_);
    ASSERT_THROW(BachelierIVol(110.0, 120.0, 8.0, -1.0), Dal::Exception_);
}

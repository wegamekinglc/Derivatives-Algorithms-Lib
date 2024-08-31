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
    Number_::Tape()->Clear();

    Number_ vol(0.2);
    Number_ T(2.0);
    Number_ forward(110.0);
    Number_ strike(120.0);

    Number_::Tape()->registerInput(vol);
    Number_::Tape()->registerInput(T);
    Number_::Tape()->registerInput(strike);
    Number_::Tape()->registerInput(forward);
    Number_::Tape()->Mark();

    auto call_price = BlackScholes(forward, strike, vol, T);
    ASSERT_NEAR(call_price.value(), 8.53592506466286, 1e-10);
    call_price.PropagateToMark();
    ASSERT_NEAR(GetGradient(forward), 0.433995720171781, 1e-8);
    ASSERT_NEAR(GetGradient(vol), 61.2095050098522, 1e-8);
    ASSERT_NEAR(GetGradient(T), 3.06047525, 1e-8);
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
    Number_::Tape()->Clear();

    Number_ vol(22.0);
    Number_ T(2.0);
    Number_ forward(110.0);
    Number_ strike(120.0);

    Number_::Tape()->registerInput(vol);
    Number_::Tape()->registerInput(T);
    Number_::Tape()->registerInput(strike);
    Number_::Tape()->registerInput(forward);
    Number_::Tape()->Mark();

    auto call_price = Bachelier(forward, strike, vol, T);
    call_price.PropagateToMark();

    ASSERT_NEAR(call_price.value(), 8.047832538, 1e-6);
    ASSERT_NEAR(GetGradient(forward), 0.37394902960009541, 1e-8);
    ASSERT_NEAR(GetGradient(vol), 0.53578740155317184, 1e-8);
    ASSERT_NEAR(GetGradient(T), 2.9468307085424446, 1e-8);
}

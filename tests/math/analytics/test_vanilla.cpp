//
// Created by wegam on 2022/9/11.
//

#include <dal/math/aad/aad.hpp>
#include "dal/math/analytics/vanilla.hpp"
#include <gtest/gtest.h>

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
    Number_ vol(0.2);
    Number_ T(2.0);
    Number_ forward(110.0);
    Number_ strike(120.0);

    auto& tape = Number_::getTape();
    tape.setActive();

    tape.reset();
    tape.registerInput(vol);
    tape.registerInput(T);
    tape.registerInput(strike);
    tape.registerInput(forward);
    
    auto call_price = BlackScholes(forward, strike, vol, T);
    ASSERT_NEAR(call_price.value(), 8.53592506466286, 1e-10);

    call_price.setGradient(1.0);
    tape.evaluate(tape.getPosition(), tape.getZeroPosition());
    ASSERT_NEAR(forward.getGradient(), 0.433995720171781, 1e-8);
    ASSERT_NEAR(vol.getGradient(), 61.2095050098522, 1e-8);
    ASSERT_NEAR(T.getGradient(), 3.06047525, 1e-8);
    tape.reset();
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
    Number_ vol(22.0);
    Number_ T(2.0);
    Number_ forward(110.0);
    Number_ strike(120.0);

    auto& tape = Number_::getTape();
    tape.setActive();

    tape.reset();
    tape.registerInput(vol);
    tape.registerInput(T);
    tape.registerInput(strike);
    tape.registerInput(forward);

    auto call_price = Bachelier(forward, strike, vol, T);
    ASSERT_NEAR(call_price.value(), 8.047832538, 1e-6);

    call_price.setGradient(1.0);
    tape.evaluate(tape.getPosition(), tape.getZeroPosition());
    ASSERT_NEAR(forward.getGradient(), 0.37394902960009541, 1e-8);
    ASSERT_NEAR(vol.getGradient(), 0.53578740155317184, 1e-8);
    ASSERT_NEAR(T.getGradient(), 2.9468307085424446, 1e-8);
    tape.reset();
}

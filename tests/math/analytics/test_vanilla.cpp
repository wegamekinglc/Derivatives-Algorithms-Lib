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

    auto call_price = BlackScholes(forward, 120.0, vol, T);
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
    
    auto call_price = BlackScholes<Number_>(forward, strike, vol, T);
    ASSERT_NEAR(call_price.value(), 8.53592506466286, 1e-10);

    call_price.setGradient(1.0);
    tape.evaluate(tape.getPosition(), tape.getZeroPosition());
    ASSERT_NEAR(forward.getGradient(), 0.433995720171781, 1e-8);
    ASSERT_NEAR(vol.getGradient(), 61.2095050098522, 1e-8);
    ASSERT_NEAR(T.getGradient(), 3.06047525, 1e-8);
    tape.reset();
}

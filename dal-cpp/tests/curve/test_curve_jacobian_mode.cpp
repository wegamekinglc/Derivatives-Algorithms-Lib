//
// Created by dal-implementer on 2026/6/18.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/curve/calibration.hpp>

using namespace Dal;

// CurveJacobianMode_ is the runtime on/off flag for the curve-calibration AAD analytic Jacobian
// (PR3 of the analytic-Jacobian redesign). It is a Machinist-generated, switchable enum with
// exactly two values: BUMPED (the byte-for-byte pre-analytic baseline, the default) and ANALYTIC
// (the AAD-derived dense Jacobian, best-effort hint that never throws). These tests pin the
// enum shape, the default-constructed value, and the round-trip string mapping.

TEST(CurveJacobianModeTest, TestHasBumpedAndAnalyticValues) {
    const CurveJacobianMode_ bumped = CurveJacobianMode_::Value_::BUMPED;
    const CurveJacobianMode_ analytic = CurveJacobianMode_::Value_::ANALYTIC;
    ASSERT_EQ(bumped.Switch(), CurveJacobianMode_::Value_::BUMPED);
    ASSERT_EQ(analytic.Switch(), CurveJacobianMode_::Value_::ANALYTIC);
    ASSERT_NE(bumped, analytic);
}

// The Machinist-generated enum carries a _NOT_SET sentinel that the default ctor assigns. Gradient
// defends against this sentinel (it engages analytic ONLY on explicit ANALYTIC, so a default-
// constructed / uninitialized mode routes to bumped like BUMPED). This pins the sentinel value.
TEST(CurveJacobianModeTest, TestDefaultConstructedIsNotSet) {
    ASSERT_EQ(CurveJacobianMode_().Switch(), CurveJacobianMode_::Value_::_NOT_SET);
}

TEST(CurveJacobianModeTest, TestStringRoundTrip) {
    ASSERT_STREQ(CurveJacobianMode_(CurveJacobianMode_::Value_::BUMPED).String(), "BUMPED");
    ASSERT_STREQ(CurveJacobianMode_(CurveJacobianMode_::Value_::ANALYTIC).String(), "ANALYTIC");
    ASSERT_EQ(CurveJacobianMode_(String_("BUMPED")).Switch(), CurveJacobianMode_::Value_::BUMPED);
    ASSERT_EQ(CurveJacobianMode_(String_("ANALYTIC")).Switch(), CurveJacobianMode_::Value_::ANALYTIC);
}

TEST(CurveJacobianModeTest, TestOptionsDefaultIsBumped) {
    // CurveCalibrationOptions_ is a peer of CurveCalibrationSpec_ (NOT serialized with the spec).
    // A default-constructed CurveCalibrationOptions_ has jacobianMode_ == BUMPED, reproducing the
    // pre-analytic bumped path byte-for-byte. This is the migration gate's foundation.
    const CurveCalibrationOptions_ options;
    ASSERT_EQ(options.jacobianMode_.Switch(), CurveJacobianMode_::Value_::BUMPED);
}

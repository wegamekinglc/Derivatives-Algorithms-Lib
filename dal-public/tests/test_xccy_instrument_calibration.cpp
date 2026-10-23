#include <gtest/gtest.h>

#include <dal/platform/platform.hpp>
#include <dal-public/src/calibrationinstrument.hpp>
#include <dal-public/src/xccycalibration.hpp>

#include "xccyinstrumentfixture.hpp"

TEST(PublicXccyInstrumentTest, TestOwningInputFailureAndDefaults) {
    Dal::XccyInstrumentCalibrationSpec_ spec;
    ASSERT_DOUBLE_EQ(spec.solver_.initialGuess_, 0.);
    try {
        static_cast<void>(Dal::CalibrateXccyInstrumentMarket(spec));
        FAIL();
    } catch (const Dal::CalibrationError_& error) {
        ASSERT_TRUE(error.Failure().report_);
        ASSERT_FALSE(error.Failure().candidate_);
    }
    ASSERT_THROW(Dal::NewXccyScriptInstrument(Dal::Handle_<Dal::CrossCurrencySwap_>(), {}), Dal::CalibrationError_);
}

TEST(PublicXccyInstrumentTest, TestNativeFacadeAndCapabilityGetter) {
    const auto spec = Dal::TestXccy::Spec();
    const auto result = Dal::CalibrateXccyInstrumentMarket(spec);
    ASSERT_TRUE(Dal::XccyInstrumentResultBasisCurve(result));
    ASSERT_EQ(Dal::XccyInstrumentResultReport(result)->Data().summary_.status_, "Succeeded");
    const auto instrument = spec.observations_[0]->Instrument();
    const auto report = Dal::XccyInstrumentCapabilities(instrument, "BasisSpread", spec.context_);
    ASSERT_EQ(report->Data().plans_.size(), 3);
    ASSERT_TRUE(report->Data().plans_[0].admitted_);
    ASSERT_FALSE(report->Data().plans_[2].admitted_);
    ASSERT_EQ(report->Data().plans_[2].reasonCode_, "MCCrossCurrencyUnsupported");
    const auto json = Dal::DumpCalibrationInstrumentJson(instrument);
    ASSERT_EQ(Dal::DumpCalibrationInstrumentJson(Dal::LoadCalibrationInstrumentJson(json)), json);
}

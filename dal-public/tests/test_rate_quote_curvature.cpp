//
// Created by Codex on 2026/10/10.
//

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

#include <dal-public/src/ratecurvature.hpp>
#include <dal/curve/calibration.hpp>
#include <dal/curve/curveparameterization.hpp>
#include <dal/curve/ycconst.hpp>
#include <dal/math/aad/native.hpp>
#include <dal/time/holidays.hpp>

#include "jointquoteriskfixtures.hpp"

namespace {
    Dal::CurveCalibrationSpec_ SingleSpec() {
        Dal::CurveCalibrationSpec_ spec;
        spec.today_ = Dal::Date_(2025, 1, 2);
        spec.ccy_ = "USD";
        spec.curveName_ = "quote_curvature";
        spec.parameterization_ = Dal::CurveParameterization_::Value_::LOG_DISCOUNT;
        spec.knotPolicy_ = Dal::CurveKnotPolicy_::Value_::INPUT;
        spec.tolerance_ = 1.0e-12;
        spec.initialGuess_ = 0.025;
        spec.knotDates_.push_back(spec.today_);
        Dal::RateIndexConvention_ convention;
        convention.dayBasis_ = Dal::DayBasis::Act365F();
        convention.businessDayConvention_ = Dal::BizDayConvention_("Unadjusted");
        convention.accrualHolidays_ = Dal::Holidays::None();
        for (int years = 1; years <= 2; ++years) {
            const auto maturity = Dal::Date::AddMonths(spec.today_, 12 * years);
            spec.knotDates_.push_back(maturity);
            spec.instruments_.push_back(
                Dal::Handle_<Dal::YCInstrument_>(new Dal::Deposit_(spec.today_, spec.today_, maturity, 0.02 + 0.005 * years, convention)));
        }
        return spec;
    }

    Dal::AAD::Number_ MixedObjective(Dal::AAD::RecordingScope_*, const Dal::Vector_<Dal::AAD::Number_>& x) {
        return x[0] * x[1] + 0.3 * x[2] * x[2] + 0.7 * x[0] * x[3];
    }

    Dal::AAD::BumpOverAADRequest_ Bumps(int inputs, double step = 2.0e-5) {
        Dal::AAD::BumpOverAADRequest_ request;
        request.directions_ = Dal::Matrix_<>(1, inputs, 0.0);
        request.directions_(0, 0) = 1.0;
        request.steps_ = {step};
        return request;
    }

    template <class F_> void AssertFailure(F_&& operation, const std::string& text) {
        try {
            operation();
            FAIL() << "Expected " << text;
        } catch (const Dal::Exception_& error) {
            ASSERT_NE(std::string(error.what()).find(text), std::string::npos) << error.what();
        }
    }

    Dal::AAD::NativeScalarFunction_ ForwardPayment(const Dal::JointMultiCurveCalibrationSpec_& spec) {
        Dal::Vector_<Dal::CurveDefinition_> definitions;
        for (const auto& declaration : spec.curves_)
            definitions.push_back(Dal::MakeCurveDefinition(declaration.curveName_, spec.ccy_, declaration.parameterization_, declaration.logDfScheme_,
                                                           declaration.knotDates_, spec.today_, Dal::DayBasis::Act365F()));
        const auto today = spec.today_;
        const bool layered = spec.curves_[1].baseLayeredOverDiscount_;
        return [definitions, today, layered](Dal::AAD::RecordingScope_*, const Dal::Vector_<Dal::AAD::Number_>& x) {
            using Number_ = Dal::AAD::Number_;
            const auto discount = Dal::BuildDiscountCurveT<Number_>(definitions[0], Dal::Vector_<Number_>(x.begin(), x.begin() + 2));
            const Dal::Handle_<Dal::Tape::DiscountCurve_<Number_>> base =
                layered ? Dal::Handle_<Dal::Tape::DiscountCurve_<Number_>>(discount) : Dal::Handle_<Dal::Tape::DiscountCurve_<Number_>>();
            const auto projection = Dal::BuildDiscountCurveT<Number_, Dal::Tape::DiscountCurve_<Number_>>(
                definitions[1], Dal::Vector_<Number_>(x.begin() + 2, x.begin() + 4), base);
            const auto start = Dal::Date::AddMonths(today, 12), end = Dal::Date::AddMonths(today, 24);
            return 100.0 * (*discount)(today, end) * (1.0 / (*projection)(start, end) - 1.0 - 0.02) + 0.3 * x[4] * x[7];
        };
    }

    double PassiveForwardPayment(Dal::JointMultiCurveCalibrationSpec_ spec, const Dal::Vector_<>& quotes) {
        size_t index = 0;
        for (int block = 0; block < 2; ++block) {
            auto& instruments = spec.curves_[block].instruments_;
            std::sort(instruments.begin(), instruments.end(),
                      [](const auto& lhs, const auto& rhs) { return lhs->TimeSpan().second < rhs->TimeSpan().second; });
            for (auto& instrument : instruments)
                instrument = JointQuoteRiskFixtures::Instrument(spec.today_, instrument->TimeSpan().second, block, quotes[index++]);
        }
        // This reference calls the passive solver and prices its actual curves without the new provider or pullback.
        const auto calibrated = Dal::CalibrateJointMultiCurve(spec);
        const auto& discount = *calibrated.discountCurves_.at(spec.curves_[0].targetCollateral_);
        const auto& projection = *calibrated.forwardCurves_.at(spec.curves_[1].targetTenor_);
        const auto start = Dal::Date::AddMonths(spec.today_, 12), end = Dal::Date::AddMonths(spec.today_, 24);
        return 100.0 * discount(spec.today_, end) * (1.0 / projection(start, end) - 1.0 - 0.02) + 0.3 * quotes[0] * quotes[3];
    }

    class CustomDeposit_ : public Dal::Deposit_ {
        int* calls_;

    public:
        CustomDeposit_(const Dal::Date_& today, int* calls)
            : Deposit_(today, Dal::Date::AddMonths(today, 12), 0.03, Dal::DayBasis::Act365F()), calls_(calls) {}
        double MarketRate() const override {
            ++*calls_;
            return 0.03;
        }
    };

    class CustomCurve_ final : public Dal::Tape::DiscountPWC_<double> {
        int* calls_;

    public:
        CustomCurve_(const Dal::Date_& today, int* calls) : DiscountPWC_("custom", "USD", {today}, {0.01}), calls_(calls) {}
        void Write(Dal::Archive::Store_&) const override { ++*calls_; }
    };
} // namespace

TEST(RateQuoteCurvatureTest, TestSingleMixedObjectiveAnalyticCurvature) {
    const auto spec = SingleSpec();
    const auto calibration = Dal::NewRateCalibration(spec);
    Dal::AAD::BumpOverAADRequest_ request;
    request.directions_ = Dal::Matrix_<>(3, 2, 0.0);
    request.directions_(0, 0) = 1.0;
    request.directions_(1, 1) = 1.0;
    request.directions_(2, 0) = 0.6;
    request.directions_(2, 1) = -0.8;
    request.steps_ = {2.0e-5, 2.0e-5, 2.0e-5};
    const auto result = Dal::EvaluateRateQuoteCurvature(MixedObjective, calibration, request);
    const double q0 = calibration.Point()[0], q1 = calibration.Point()[1];
    const double t0 = Dal::DayBasis::Act365F()(spec.today_, spec.knotDates_[1], nullptr);
    const double t1 = Dal::DayBasis::Act365F()(spec.today_, spec.knotDates_[2], nullptr);
    const double theta0 = -std::log1p(q0 * t0), theta1 = -std::log1p(q1 * t1);
    const double a = -t0 / (1.0 + q0 * t0), b = -t1 / (1.0 + q1 * t1);
    const double h00 = a * a * (theta1 + 0.7 * q1) + 0.6;
    const double h01 = a * (b + 0.7), h11 = theta0 * b * b;
    ASSERT_NEAR(result.Value(), theta0 * theta1 + 0.3 * q0 * q0 + 0.7 * theta0 * q1, 1.0e-11);
    ASSERT_NEAR(result.Gradient()[0], a * (theta1 + 0.7 * q1) + 0.6 * q0, 1.0e-9);
    ASSERT_NEAR(result.Gradient()[1], theta0 * (b + 0.7), 1.0e-9);
    for (int row = 0; row < 3; ++row) {
        ASSERT_NEAR(result.HessianProducts()(row, 0), h00 * request.directions_(row, 0) + h01 * request.directions_(row, 1), 2.0e-7);
        ASSERT_NEAR(result.HessianProducts()(row, 1), h01 * request.directions_(row, 0) + h11 * request.directions_(row, 1), 2.0e-7);
    }
    ASSERT_EQ(result.Execution().quoteGradientEvaluations_, 7U);
    ASSERT_EQ(result.BaseCalibration().Provenance().Axis().fingerprint_, calibration.Provenance().Axis().fingerprint_);
    // A frozen inverse misses theta'' in H00, giving only the direct 0.6 contribution.
    ASSERT_GT(std::abs(h00 - 0.6), 0.01);
}

TEST(RateQuoteCurvatureTest, TestJointIndependentPaymentCurvatureAndOrdering) {
    for (bool layered : {false, true}) {
        auto spec = JointQuoteRiskFixtures::Spec(4, 2, Dal::CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD, layered);
        spec.tolerance_ = 1.0e-12;
        for (auto& declaration : spec.curves_)
            std::reverse(declaration.instruments_.begin(), declaration.instruments_.end());
        const auto calibration = Dal::NewRateCalibration(spec);
        const auto objective = ForwardPayment(spec);
        ASSERT_EQ(calibration.Provenance().Axis().parameterRanges_[0].blockKey_, "curve:0");
        ASSERT_EQ(calibration.Provenance().Axis().parameterRanges_[1].blockKey_, "curve:1");
        for (double step : {4.0e-4, 2.0e-4, 1.0e-4}) {
            auto request = Bumps(4, step);
            request.directions_(0, 1) = -0.4;
            request.directions_(0, 2) = 0.7;
            request.directions_(0, 3) = -0.2;
            const auto result = Dal::EvaluateRateQuoteCurvature(objective, calibration, request);
            ASSERT_NEAR(result.Value(), PassiveForwardPayment(spec, calibration.Point()), 1.0e-9);
            for (int column = 0; column < 4; ++column) {
                double prices[2][2];
                for (int outer = 0; outer < 2; ++outer) {
                    for (int inner = 0; inner < 2; ++inner) {
                        auto point = calibration.Point();
                        for (int index = 0; index < 4; ++index)
                            point[index] += (outer == 0 ? step : -step) * request.directions_(0, index);
                        point[column] += inner == 0 ? 1.0e-4 : -1.0e-4;
                        prices[outer][inner] = PassiveForwardPayment(spec, point);
                    }
                }
                const double reference = ((prices[0][0] - prices[0][1]) - (prices[1][0] - prices[1][1])) / (4.0 * step * 1.0e-4);
                ASSERT_NEAR(result.HessianProducts()(0, column), reference, 2.0e-3)
                    << "layered=" << layered << "; step=" << step << "; column=" << column;
            }
        }
    }
}

TEST(RateQuoteCurvatureTest, TestSnapshotOwnsDefinitionAndFixedBase) {
    auto spec = SingleSpec();
    auto base = std::make_shared<Dal::Tape::DiscountPWC_<double>>("base", "USD", Dal::Vector_<Dal::Date_>{spec.today_}, Dal::Vector_<>{0.01});
    spec.baseCurve_ = Dal::Handle_<Dal::DiscountCurve_>(base);
    const auto calibration = Dal::NewRateCalibration(spec);
    const auto before = Dal::EvaluateRateQuoteCurvature(MixedObjective, calibration, Bumps(2));
    const Dal::Vector_<> shift{0.02};
    base->ApplyDX(shift.begin(), 1.0);
    spec.instruments_.clear();
    spec.knotDates_.clear();
    const auto after = Dal::EvaluateRateQuoteCurvature(MixedObjective, calibration, Bumps(2));
    ASSERT_EQ(before.Value(), after.Value());
    ASSERT_EQ(before.Gradient(), after.Gradient());
    ASSERT_EQ(before.HessianProducts()(0, 0), after.HessianProducts()(0, 0));
    auto bumped = calibration.Point();
    bumped[1] += 1.0e-4;
    const auto rebuilt = Dal::RecalibrateRateWithRisk(calibration, bumped);
    ASSERT_EQ(rebuilt.Point(), bumped);
    ASSERT_NE(rebuilt.Provenance().State().fingerprint_, calibration.Provenance().State().fingerprint_);
    ASSERT_EQ(rebuilt.Provenance().Axis().fingerprint_, calibration.Provenance().Axis().fingerprint_);
}

TEST(RateQuoteCurvatureTest, TestRejectUnsupportedAndNonSquareCalibration) {
    auto spec = SingleSpec();
    spec.solveMode_ = Dal::CurveSolveMode_::Value_::APPROXIMATE;
    AssertFailure([&] { (void)Dal::NewRateCalibration(spec); }, "requires EXACT");
    spec = SingleSpec();
    spec.instruments_.pop_back();
    AssertFailure([&] { (void)Dal::NewRateCalibration(spec); }, "square");
    int calls = 0;
    spec = SingleSpec();
    spec.instruments_[0] = Dal::Handle_<Dal::YCInstrument_>(new CustomDeposit_(spec.today_, &calls));
    AssertFailure([&] { (void)Dal::NewRateCalibration(spec); }, "unsupported native instrument");
    ASSERT_EQ(calls, 0);
    auto joint = JointQuoteRiskFixtures::Spec(4, 2);
    joint.curves_[0].knotDates_.push_back(Dal::Date::AddMonths(joint.today_, 30));
    AssertFailure([&] { (void)Dal::NewRateCalibration(joint); }, "square");
}

TEST(RateQuoteCurvatureTest, TestRequestAdmissionPrecedesObjective) {
    const auto calibration = Dal::NewRateCalibration(SingleSpec());
    int calls = 0;
    const Dal::AAD::NativeScalarFunction_ objective = [&](auto*, const auto& x) {
        ++calls;
        return x[0];
    };
    for (int failure = 0; failure < 6; ++failure) {
        auto request = Bumps(2);
        switch (failure) {
        case 0:
            request.steps_[0] = 0.0;
            break;
        case 1:
            request.directions_(0, 0) = 0.0;
            break;
        case 2:
            request.directions_(0, 1) = std::numeric_limits<double>::infinity();
            break;
        case 3:
            request.steps_.clear();
            break;
        case 4:
            request.numericPayloadBudgetBytes_ = Dal::AAD::BumpOverAADPayloadBytes(2, 1) - 1;
            break;
        case 5:
            request.steps_[0] = std::numeric_limits<double>::denorm_min();
            break;
        }
        ASSERT_THROW((void)Dal::EvaluateRateQuoteCurvature(objective, calibration, request), Dal::Exception_);
    }
    ASSERT_EQ(calls, 0);
    auto request = Bumps(2);
    request.recordingCapacityBudgetBytes_ = 0;
    AssertFailure([&] { (void)Dal::EvaluateRateQuoteCurvature(objective, calibration, request); }, "base; stage=calibration");
    ASSERT_EQ(calls, 0);
    ASSERT_THROW((void)Dal::RecalibrateRateWithRisk(calibration, {0.02}), Dal::Exception_);
    ASSERT_THROW((void)Dal::RecalibrateRateWithRisk(calibration, {0.02, std::numeric_limits<double>::quiet_NaN()}), Dal::Exception_);
}

TEST(RateQuoteCurvatureTest, TestFailuresRestoreModeAndOwningResults) {
    const auto calibration = Dal::NewRateCalibration(SingleSpec());
    const auto mode = Dal::AAD::SetNumResultsForAAD(true, 3);
    const auto accepted = Dal::EvaluateRateQuoteCurvature(MixedObjective, calibration, Bumps(2));
    ASSERT_TRUE(Dal::AAD::Tape()->multi_);
    ASSERT_EQ(Dal::AAD::Tape()->numAdj_, 3U);
    for (int failAt : {1, 2, 3}) {
        int calls = 0;
        const Dal::AAD::NativeScalarFunction_ objective = [&](auto* recording, const auto& x) {
            if (++calls == failAt)
                throw std::runtime_error("deliberate objective failure");
            return MixedObjective(recording, x);
        };
        const std::string context =
            failAt == 1 ? "base; stage=objective" : (failAt == 2 ? "direction=0; plus; stage=objective" : "direction=0; minus; stage=objective");
        AssertFailure([&] { (void)Dal::EvaluateRateQuoteCurvature(objective, calibration, Bumps(2)); }, context);
        ASSERT_TRUE(Dal::AAD::Tape()->multi_);
        ASSERT_EQ(Dal::AAD::Tape()->numAdj_, 3U);
    }
    const auto recovered = Dal::EvaluateRateQuoteCurvature(MixedObjective, calibration, Bumps(2));
    ASSERT_EQ(accepted.Gradient(), recovered.Gradient());
    ASSERT_EQ(accepted.HessianProducts()(0, 0), recovered.HessianProducts()(0, 0));
    ASSERT_FALSE(Dal::AAD::NativeOperations_::Capabilities().higherOrder_);
}

TEST(RateQuoteCurvatureTest, TestActiveRecordingRejectedWithoutCorruption) {
    const auto calibration = Dal::NewRateCalibration(SingleSpec());
    Dal::AAD::RecordingScope_ recording;
    Dal::AAD::Number_ input;
    recording.RegisterInput(input, 2.0);
    recording.StartRecording();
    ASSERT_THROW((void)Dal::NewRateCalibration(SingleSpec()), Dal::Exception_);
    ASSERT_THROW((void)Dal::RecalibrateRateWithRisk(calibration, calibration.Point()), Dal::Exception_);
    ASSERT_THROW((void)Dal::EvaluateRateQuoteCurvature(MixedObjective, calibration, Bumps(2)), Dal::Exception_);
    const Dal::AAD::Number_ output = input * input;
    recording.FinishRecording();
    recording.ClearAdjoints();
    auto root = output;
    Dal::AAD::NativeOperations_::SetSeed(root, 1.0);
    recording.Reverse();
    ASSERT_EQ(Dal::AAD::NativeOperations_::ReadAdjoint(input), 4.0);
    recording.Close();
}

TEST(RateQuoteCurvatureTest, TestFrozenInverseNegativeControl) {
    const auto calibration = Dal::NewRateCalibration(SingleSpec());
    const double step = 2.0e-5;
    const auto result = Dal::EvaluateRateQuoteCurvature(MixedObjective, calibration, Bumps(2, step));
    double gradients[2];
    for (int side = 0; side < 2; ++side) {
        auto point = calibration.Point();
        point[0] += side == 0 ? step : -step;
        const auto rebuilt = Dal::RecalibrateRateWithRisk(calibration, point);
        auto inputs = rebuilt.Parameters();
        inputs.Append(point);
        Dal::AAD::BumpOverAADRequest_ firstOrder;
        firstOrder.directions_ = Dal::Matrix_<>(0, 4);
        const auto differentiated = Dal::AAD::EvaluateBumpOverAAD(MixedObjective, inputs, firstOrder);
        gradients[side] = differentiated.Gradient()[2];
        for (int parameter = 0; parameter < 2; ++parameter)
            gradients[side] += calibration.Provenance().EffectiveInverse()(parameter, 0) * differentiated.Gradient()[parameter] /
                               calibration.Provenance().Tolerance();
    }
    ASSERT_GT(std::abs((gradients[0] - gradients[1]) / (2.0 * step) - result.HessianProducts()(0, 0)), 0.01);
}

TEST(RateQuoteCurvatureTest, TestFactoryWideModeBaseOnlyAndCapacityLimit) {
    const auto mode = Dal::AAD::SetNumResultsForAAD(true, 3);
    const auto calibration = Dal::NewRateCalibration(SingleSpec());
    ASSERT_TRUE(Dal::AAD::Tape()->multi_);
    ASSERT_EQ(Dal::AAD::Tape()->numAdj_, 3U);
    Dal::AAD::BumpOverAADRequest_ request;
    request.directions_ = Dal::Matrix_<>(0, 2);
    request.numericPayloadBudgetBytes_ = Dal::AAD::BumpOverAADPayloadBytes(2, 0);
    const auto result = Dal::EvaluateRateQuoteCurvature(MixedObjective, calibration, request);
    ASSERT_EQ(result.Execution().quoteGradientEvaluations_, 1U);
    ASSERT_EQ(result.Execution().numericPayloadBytes_, *request.numericPayloadBudgetBytes_);
    ASSERT_GT(result.Execution().peakTapeBytes_, 0U);
    ASSERT_GT(result.Execution().cleanupReserveBytes_, 0U);
    request.recordingCapacityBudgetBytes_ = result.Execution().peakTapeBytes_;
    ASSERT_THROW((void)Dal::EvaluateRateQuoteCurvature(MixedObjective, calibration, request), Dal::Exception_);
    request.recordingCapacityBudgetBytes_ = result.Execution().peakTapeBytes_ + result.Execution().cleanupReserveBytes_;
    const auto bounded = Dal::EvaluateRateQuoteCurvature(MixedObjective, calibration, request);
    ASSERT_EQ(result.Gradient(), bounded.Gradient());
    ASSERT_TRUE(Dal::AAD::Tape()->multi_);
    ASSERT_EQ(Dal::AAD::Tape()->numAdj_, 3U);
}

TEST(RateQuoteCurvatureTest, TestCalibrationFailureContextAndRecovery) {
    const auto calibration = Dal::NewRateCalibration(SingleSpec());
    auto request = Bumps(2, 1.0);
    request.directions_(0, 0) = 0.0;
    request.directions_(0, 1) = 1.0;
    AssertFailure([&] { (void)Dal::EvaluateRateQuoteCurvature(MixedObjective, calibration, request); }, "direction=0; minus; stage=calibration");
    const auto recovered = Dal::EvaluateRateQuoteCurvature(MixedObjective, calibration, Bumps(2));
    ASSERT_TRUE(std::isfinite(recovered.HessianProducts()(0, 0)));
}

TEST(RateQuoteCurvatureTest, TestNativeInstrumentConventionsPreserved) {
    for (int type = 0; type < 5; ++type) {
        auto spec = SingleSpec();
        const auto start = spec.knotDates_[1], end = spec.knotDates_[2];
        const auto index = JointQuoteRiskFixtures::Index(0);
        const auto leg = JointQuoteRiskFixtures::Leg(6);
        if (type == 0)
            spec.instruments_[1] = Dal::Handle_<Dal::YCInstrument_>(new Dal::FRA_(spec.today_, start, end, 0.03, index));
        else if (type == 1)
            spec.instruments_[1] = Dal::Handle_<Dal::YCInstrument_>(new Dal::STIR_(spec.today_, start, end, 0.03, index));
        else if (type == 2)
            spec.instruments_[1] = Dal::Handle_<Dal::YCInstrument_>(new Dal::Future_(spec.today_, start, end, 0.03, index, 0.0017));
        else if (type == 3)
            spec.instruments_[1] = Dal::Handle_<Dal::YCInstrument_>(new Dal::Swap_(spec.today_, spec.today_, end, 0.03, leg, index, leg));
        else
            spec.instruments_[1] = Dal::Handle_<Dal::YCInstrument_>(new Dal::OISSwap_(spec.today_, spec.today_, end, 0.03, leg, index, leg));
        const auto expected = Dal::CalibrateYieldCurve(spec);
        const auto actual = Dal::NewRateCalibration(spec);
        const auto parameters = Dal::InspectCurveParameters(*expected.curve_, spec.today_).passiveParameters_;
        ASSERT_EQ(actual.Parameters(), parameters) << "instrument=" << type;
        ASSERT_EQ(actual.Provenance().Axis().quotes_[1].displayName_, spec.instruments_[1]->Name());
        const auto replay = Dal::RecalibrateRateWithRisk(actual, actual.Point());
        ASSERT_EQ(replay.Parameters(), actual.Parameters());
    }
}

TEST(RateQuoteCurvatureTest, TestCustomCurveAndSingularSystemReject) {
    auto spec = SingleSpec();
    int calls = 0;
    spec.baseCurve_ = Dal::Handle_<Dal::DiscountCurve_>(new CustomCurve_(spec.today_, &calls));
    AssertFailure([&] { (void)Dal::NewRateCalibration(spec); }, "unsupported native fixed curve");
    ASSERT_EQ(calls, 0);
    spec = SingleSpec();
    spec.instruments_[1] = spec.instruments_[0];
    ASSERT_THROW((void)Dal::NewRateCalibration(spec), Dal::Exception_);
}

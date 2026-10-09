//
// Created by Codex on 2026/10/10.
//

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <functional>
#include <future>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>

#include <dal/math/aad/bumpoveraad.hpp>
#include <dal/math/aad/native.hpp>
#include <dal/model/dupire.hpp>
#include <dal/model/dupirecurvature.hpp>
#include <dal/model/dupirerisk.hpp>
#include <dal/platform/platform.hpp>

#include "../math/aad/models/flat_ivs.hpp"
#include "dupireriskinputs.hpp"

namespace {
    using Dal::Matrix_;
    using Dal::Vector_;

    Dal::AAD::BumpOverAADRequest_ Direction(const Vector_<>& direction, double step) {
        Dal::AAD::BumpOverAADRequest_ request;
        request.directions_ = Matrix_<>(1, static_cast<int>(direction.size()));
        std::copy(direction.begin(), direction.end(), request.directions_.Data());
        request.steps_ = {step};
        return request;
    }

    double FreshObjective(const Dal::AAD::IVS_& base, const Dal::DupireRiskInputs_& inputs) {
        const auto surface = Dal::CalibrateDupireWithRisk(base, inputs).Surface();
        const double first = surface->vols_(4, 0);
        const double second = surface->vols_(4, 1);
        return first + 0.7 * second * second + 2.0 * first * inputs.quoteSpreads_(1, 0);
    }

    constexpr std::array<double, 3> CURVATURE_STEPS{4e-4, 2e-4, 1e-4};
    constexpr double COORDINATE_STEP = 2e-4;

    Vector_<> IndependentProducts(const Dal::AAD::IVS_& base, const Dal::DupireRiskInputs_& inputs, const Vector_<>& direction) {
        Vector_<> reference(6);
        for (int quote = 0; quote < 6; ++quote) {
            double values[2][2];
            for (int outer = 0; outer < 2; ++outer)
                for (int inner = 0; inner < 2; ++inner) {
                    auto bumped = inputs;
                    for (int column = 0; column < 6; ++column)
                        bumped.quoteSpreads_(column / 2, column % 2) += (outer == 0 ? 1.0 : -1.0) * COORDINATE_STEP * direction[column];
                    bumped.quoteSpreads_(quote / 2, quote % 2) += (inner == 0 ? 1.0 : -1.0) * COORDINATE_STEP;
                    values[outer][inner] = FreshObjective(base, bumped);
                }
            reference[quote] = (values[0][0] - values[0][1] - values[1][0] + values[1][1]) / (4.0 * COORDINATE_STEP * COORDINATE_STEP);
        }
        return reference;
    }

    bool MatchesValueDifferences(const Dal::DupireQuoteCurvatureResult_& actual, const Vector_<>& reference, double step) {
        bool passes = true;
        for (int quote = 0; quote < 6; ++quote) {
            const double estimate = actual.HessianProducts()(0, quote);
            const double tolerance = 0.08 + 0.02 * std::abs(reference[quote]);
            const bool pass = std::abs(estimate - reference[quote]) <= tolerance;
            std::cout << std::setprecision(17) << "DupireCurvature quote=" << quote << " step=" << step << " aad=" << estimate
                      << " valueDifference=" << reference[quote] << " tolerance=" << tolerance << " pass=" << pass << '\n';
            passes = passes && pass;
        }
        return passes;
    }

    Dal::AAD::Number_ QuoteSquare(Dal::AAD::RecordingScope_*, const Vector_<Dal::AAD::Number_>& x) { return x[x.size() - 6] * x[x.size() - 6]; }

    class CallbackIVS_ final : public Dal::AAD::IVS_ {
        std::function<double()> volatility_;

    public:
        explicit CallbackIVS_(std::function<double()> volatility) : IVS_(100.0, 0.05, 0.02), volatility_(std::move(volatility)) {}
        [[nodiscard]] double ImpliedVol(double, double) const override { return volatility_(); }
    };
} // namespace

TEST(DupireQuoteCurvatureTest, TestRecalibrationMatchesIndependentFreshCalibration) {
    const Dal::AAD::FlatIVS_ base(100.0, 0.05, 0.02, 0.2);
    const auto original = Dal::CalibrateDupireWithRisk(base, Dal::Test::SmallRiskInputs(), "sealed");
    auto inputs = original.Inputs();
    inputs.quoteSpreads_.Fill(0.001);
    const auto expected = Dal::CalibrateDupireWithRisk(base, inputs, "independent");
    const auto actual = Dal::RecalibrateDupireWithRisk(original, inputs.quoteSpreads_);
    ASSERT_TRUE(actual.Matches(expected));
    ASSERT_EQ(actual.Surface()->Name(), "sealed");
    ASSERT_FALSE(actual.Matches(original));
    ASSERT_EQ(original.Inputs().quoteSpreads_(0, 0), 0.0);
}

TEST(DupireQuoteCurvatureTest, TestDirectQuoteGammaCrossAndSignedHessianProducts) {
    const Dal::AAD::FlatIVS_ base(100.0, 0.05, 0.02, 0.2);
    const auto calibration = Dal::CalibrateDupireWithRisk(base, Dal::Test::SmallRiskInputs());
    const auto objective = [](Dal::AAD::RecordingScope_*, const Dal::Vector_<Dal::AAD::Number_>& x) {
        const size_t quote = x.size() - 6;
        return x[quote] * x[quote] + 3.0 * x[quote] * x[quote + 1] + 2.0 * x[quote + 1] * x[quote + 1];
    };
    Dal::AAD::BumpOverAADRequest_ bumps;
    bumps.directions_ = Dal::Matrix_<>(3, 6, 0.0);
    bumps.directions_(0, 0) = 1.0;
    bumps.directions_(1, 1) = 1.0;
    bumps.directions_(2, 0) = 1.0;
    bumps.directions_(2, 1) = -2.0;
    bumps.steps_ = {5e-5, 5e-5, 5e-5};
    const auto actual = Dal::EvaluateDupireQuoteCurvature(objective, calibration, bumps);
    ASSERT_EQ(actual.Value(), 0.0);
    ASSERT_EQ(actual.HessianProducts().Rows(), 3);
    ASSERT_EQ(actual.HessianProducts().Cols(), 6);
    ASSERT_NEAR(actual.HessianProducts()(0, 0), 2.0, 1e-10);
    ASSERT_NEAR(actual.HessianProducts()(0, 1), 3.0, 1e-10);
    ASSERT_NEAR(actual.HessianProducts()(1, 0), 3.0, 1e-10);
    ASSERT_NEAR(actual.HessianProducts()(1, 1), 4.0, 1e-10);
    ASSERT_NEAR(actual.HessianProducts()(2, 0), -4.0, 1e-10);
    ASSERT_NEAR(actual.HessianProducts()(2, 1), -5.0, 1e-10);
    ASSERT_EQ(actual.Execution().quoteGradientEvaluations_, 7);
    ASSERT_EQ(actual.Execution().calibrations_, 7);
    ASSERT_EQ(actual.Execution().objectiveReverseSweeps_, 7);
    ASSERT_EQ(actual.Execution().calibrationReverseSweeps_, 7);
}

TEST(DupireQuoteCurvatureTest, TestNonlinearCalibrationAndMixedQuoteTermsMatchIndependentValueDifferences) {
    const Dal::AAD::MertonIVS_ base(100.0, 0.2, 0.08, -0.1, 0.15);
    auto inputs = Dal::Test::SmallRiskInputs();
    inputs.quoteStrikes_ = {75.0, 105.0, 135.0};
    inputs.quoteMaturities_ = {0.4, 1.2};
    inputs.quoteSpreads_.Fill(0.001);
    const auto snapshot = Dal::CalibrateDupireWithRisk(base, inputs);
    const Vector_<> direction{0.3, -0.2, 1.0, 0.4, -0.1, 0.2};
    const auto objective = [](Dal::AAD::RecordingScope_*, const Vector_<Dal::AAD::Number_>& x) {
        return x[8] + 0.7 * x[9] * x[9] + 2.0 * x[8] * x[20];
    };
    const auto reference = IndependentProducts(base, inputs, direction);
    std::array<bool, 3> passes{};
    for (size_t step = 0; step < CURVATURE_STEPS.size(); ++step) {
        const auto actual = Dal::EvaluateDupireQuoteCurvature(objective, snapshot, Direction(direction, CURVATURE_STEPS[step]));
        ASSERT_NEAR(actual.Value(), FreshObjective(base, inputs), 1e-10);
        passes[step] = MatchesValueDifferences(actual, reference, CURVATURE_STEPS[step]);
    }
    ASSERT_TRUE((passes[0] && passes[1]) || (passes[1] && passes[2]));

    const auto linear = [](Dal::AAD::RecordingScope_*, const Vector_<Dal::AAD::Number_>& x) { return x[8]; };
    const auto curvature = Dal::EvaluateDupireQuoteCurvature(linear, snapshot, Direction(direction, 2e-4));
    double magnitude = 0.0;
    for (double value : curvature.HessianProducts())
        magnitude += std::abs(value);
    ASSERT_GT(magnitude, 0.5);
}

TEST(DupireQuoteCurvatureTest, TestRecalibrationOwnsFrozenBaseAndReplacementSpreads) {
    int samples = 0;
    double volatility = 0.2;
    auto base = std::make_unique<CallbackIVS_>([&]() {
        ++samples;
        return volatility;
    });
    const auto original = Dal::CalibrateDupireWithRisk(*base, Dal::Test::SmallRiskInputs(), "sealed");
    const int frozenSamples = samples;
    volatility = 0.7;
    base.reset();
    Matrix_<> spreads(3, 2, 0.002);
    const auto actual = Dal::RecalibrateDupireWithRisk(original, spreads);
    spreads.Fill(-100.0);
    const Dal::AAD::FlatIVS_ reference(100.0, 0.05, 0.02, 0.2);
    auto expectedInputs = Dal::Test::SmallRiskInputs();
    expectedInputs.quoteSpreads_.Fill(0.002);
    ASSERT_TRUE(actual.Matches(Dal::CalibrateDupireWithRisk(reference, expectedInputs)));
    ASSERT_EQ(samples, frozenSamples);
    ASSERT_EQ(actual.Surface()->spots_, original.Surface()->spots_);
    ASSERT_EQ(actual.Surface()->times_, original.Surface()->times_);
    ASSERT_EQ(actual.Algorithm(), original.Algorithm());
    ASSERT_EQ(actual.Surface()->Name(), "sealed");
    ASSERT_EQ(actual.Inputs().quoteSpreads_(0, 0), 0.002);
    ASSERT_EQ(original.Inputs().quoteSpreads_(0, 0), 0.0);
    ASSERT_THROW(Dal::ValidateDupireQuoteRecalibration(original, spreads), Dal::Exception_);
    ASSERT_THROW((void)Dal::RecalibrateDupireWithRisk(original, Matrix_<>(2, 3, 0.0)), Dal::Exception_);
    spreads.Fill(std::numeric_limits<double>::quiet_NaN());
    ASSERT_THROW((void)Dal::RecalibrateDupireWithRisk(original, spreads), Dal::Exception_);
    ASSERT_TRUE(Dal::RecalibrateDupireWithRisk(original, original.Inputs().quoteSpreads_).Matches(original));
}

TEST(DupireQuoteCurvatureTest, TestPassiveRecalibrationPreservesActiveRecordingAndNestedDriverRejects) {
    const Dal::AAD::FlatIVS_ base(100.0, 0.05, 0.02, 0.2);
    const auto snapshot = Dal::CalibrateDupireWithRisk(base, Dal::Test::SmallRiskInputs());
    const auto mode = Dal::AAD::SetNumResultsForAAD(false, 1);
    Dal::AAD::RecordingScope_ recording;
    Dal::AAD::Number_ input;
    recording.RegisterInput(input, 3.0);
    recording.StartRecording();
    const auto occupied = Dal::AAD::Tape()->nodes_.OccupiedSlots();
    Dal::ValidateDupireQuoteRecalibration(snapshot, snapshot.Inputs().quoteSpreads_);
    const auto rebuilt = Dal::RecalibrateDupireWithRisk(snapshot, Matrix_<>(3, 2, 0.001));
    ASSERT_EQ(Dal::AAD::Tape()->nodes_.OccupiedSlots(), occupied);
    ASSERT_TRUE(rebuilt.Surface().get() != nullptr);
    ASSERT_THROW((void)Dal::EvaluateDupireQuoteCurvature(QuoteSquare, snapshot, Direction({1, 0, 0, 0, 0, 0}, 5e-5)), Dal::Exception_);
    Dal::AAD::Number_ output = input * input;
    recording.FinishRecording();
    recording.ClearAdjoints();
    Dal::AAD::NativeOperations_::SetSeed(output, 1.0);
    recording.Reverse();
    ASSERT_EQ(Dal::AAD::NativeOperations_::ReadAdjoint(input), 6.0);
    recording.Close();
}

TEST(DupireQuoteCurvatureTest, TestAllBumpsAndPayloadAdmitBeforeObjectiveExecution) {
    const Dal::AAD::FlatIVS_ base(100.0, 0.05, 0.02, 0.2);
    const auto snapshot = Dal::CalibrateDupireWithRisk(base, Dal::Test::SmallRiskInputs());
    int calls = 0;
    const auto function = [&](Dal::AAD::RecordingScope_* recording, const Vector_<Dal::AAD::Number_>& x) {
        ++calls;
        return QuoteSquare(recording, x);
    };
    const auto valid = Direction({1, 0, 0, 0, 0, 0}, 5e-5);
    Vector_<Dal::AAD::BumpOverAADRequest_> invalid;
    invalid.push_back(valid);
    invalid.back().directions_ = Matrix_<>(1, 5, 1.0);
    invalid.push_back(valid);
    invalid.back().steps_.clear();
    invalid.push_back(valid);
    invalid.back().directions_.Fill(0.0);
    invalid.push_back(valid);
    invalid.back().steps_[0] = 0.0;
    invalid.push_back(valid);
    invalid.back().directions_(0, 0) = std::numeric_limits<double>::infinity();
    invalid.push_back(valid);
    invalid.back().numericPayloadBudgetBytes_ = Dal::AAD::BumpOverAADPayloadBytes(6, 1) - 1;
    invalid.push_back(valid);
    invalid.back().directions_ = Matrix_<>(2, 6, 1.0);
    invalid.back().steps_ = {5e-5, 0.3};
    for (const auto& request : invalid) {
        ASSERT_THROW((void)Dal::EvaluateDupireQuoteCurvature(function, snapshot, request), Dal::Exception_);
        ASSERT_EQ(calls, 0);
    }
    ASSERT_THROW((void)Dal::EvaluateDupireQuoteCurvature({}, snapshot, valid), Dal::Exception_);
    ASSERT_NEAR(Dal::EvaluateDupireQuoteCurvature(function, snapshot, valid).HessianProducts()(0, 0), 2.0, 1e-10);
    ASSERT_EQ(calls, 3);
}

TEST(DupireQuoteCurvatureTest, TestExactPayloadAndRecordingBudgetsRestoreWideMode) {
    const Dal::AAD::FlatIVS_ base(100.0, 0.05, 0.02, 0.2);
    const auto snapshot = Dal::CalibrateDupireWithRisk(base, Dal::Test::SmallRiskInputs());
    const auto mode = Dal::AAD::SetNumResultsForAAD(true, 4);
    auto request = Direction({1, 0, 0, 0, 0, 0}, 5e-5);
    request.numericPayloadBudgetBytes_ = Dal::AAD::BumpOverAADPayloadBytes(6, 1);
    const auto reference = Dal::EvaluateDupireQuoteCurvature(QuoteSquare, snapshot, request);
    ASSERT_EQ(reference.Execution().numericPayloadBytes_, *request.numericPayloadBudgetBytes_);
    request.recordingCapacityBudgetBytes_ = reference.Execution().peakTapeBytes_ + reference.Execution().cleanupReserveBytes_;
    const auto exact = Dal::EvaluateDupireQuoteCurvature(QuoteSquare, snapshot, request);
    ASSERT_EQ(exact.Execution().peakTapeBytes_, reference.Execution().peakTapeBytes_);
    --*request.recordingCapacityBudgetBytes_;
    ASSERT_THROW((void)Dal::EvaluateDupireQuoteCurvature(QuoteSquare, snapshot, request), Dal::Exception_);
    request.recordingCapacityBudgetBytes_ = 0;
    ASSERT_THROW((void)Dal::EvaluateDupireQuoteCurvature(QuoteSquare, snapshot, request), Dal::Exception_);
    ASSERT_TRUE(Dal::AAD::Tape()->multi_);
    ASSERT_EQ(Dal::AAD::Tape()->numAdj_, 4);
    request.recordingCapacityBudgetBytes_.reset();
    ASSERT_NEAR(Dal::EvaluateDupireQuoteCurvature(QuoteSquare, snapshot, request).HessianProducts()(0, 0), 2.0, 1e-10);
    ASSERT_TRUE(Dal::AAD::Tape()->multi_);
    ASSERT_EQ(Dal::AAD::Tape()->numAdj_, 4);
}

TEST(DupireQuoteCurvatureTest, TestEveryObjectiveFailurePhaseRestoresModeAndRecovers) {
    const Dal::AAD::FlatIVS_ base(100.0, 0.05, 0.02, 0.2);
    const auto snapshot = Dal::CalibrateDupireWithRisk(base, Dal::Test::SmallRiskInputs());
    const auto mode = Dal::AAD::SetNumResultsForAAD(true, 4);
    const auto request = Direction({1, 0, 0, 0, 0, 0}, 5e-5);
    for (int failAt : {1, 2, 3}) {
        int calls = 0;
        try {
            (void)Dal::EvaluateDupireQuoteCurvature(
                [&](Dal::AAD::RecordingScope_* recording, const Vector_<Dal::AAD::Number_>& x) {
                    if (++calls == failAt)
                        THROW("deliberate objective failure");
                    return QuoteSquare(recording, x);
                },
                snapshot, request);
            FAIL() << "callback failure accepted";
        } catch (const Dal::Exception_& error) {
            const std::string context = error.what();
            ASSERT_NE(context.find("stage=objective"), std::string::npos);
            ASSERT_NE(context.find(failAt == 1 ? "base" : "direction=0"), std::string::npos);
            if (failAt > 1) {
                ASSERT_NE(context.find(failAt == 2 ? "plus" : "minus"), std::string::npos);
            }
        }
        ASSERT_TRUE(Dal::AAD::Tape()->multi_);
        ASSERT_EQ(Dal::AAD::Tape()->numAdj_, 4);
        ASSERT_NEAR(Dal::EvaluateDupireQuoteCurvature(QuoteSquare, snapshot, request).HessianProducts()(0, 0), 2.0, 1e-10);
    }
    ASSERT_THROW(
        (void)Dal::EvaluateDupireQuoteCurvature(
            [](Dal::AAD::RecordingScope_*, const Vector_<Dal::AAD::Number_>&) -> Dal::AAD::Number_ { throw std::bad_alloc(); }, snapshot, request),
        std::bad_alloc);
    ASSERT_TRUE(Dal::AAD::Tape()->multi_);
    ASSERT_EQ(Dal::AAD::Tape()->numAdj_, 4);
}

TEST(DupireQuoteCurvatureTest, TestCallerMutationCannotChangeSealedCalibrationRequestOrObjective) {
    const Dal::AAD::FlatIVS_ base(100.0, 0.05, 0.02, 0.2);
    auto snapshot = Dal::CalibrateDupireWithRisk(base, Dal::Test::SmallRiskInputs());
    const auto original = snapshot;
    auto request = Direction({1, 0, 0, 0, 0, 0}, 5e-5);
    Dal::AAD::NativeScalarFunction_ function;
    int calls = 0;
    function = [&](Dal::AAD::RecordingScope_* recording, const Vector_<Dal::AAD::Number_>& x) {
        if (++calls == 1) {
            snapshot = Dal::RecalibrateDupireWithRisk(original, Matrix_<>(3, 2, 0.002));
            request.directions_.Fill(0.0);
            request.steps_[0] = 0.0;
            request.recordingCapacityBudgetBytes_ = 0;
            function = [](Dal::AAD::RecordingScope_*, const Vector_<Dal::AAD::Number_>&) { return Dal::AAD::Number_(-999.0); };
        }
        return QuoteSquare(recording, x);
    };
    const auto result = Dal::EvaluateDupireQuoteCurvature(function, snapshot, request);
    ASSERT_EQ(calls, 3);
    ASSERT_EQ(result.Value(), 0.0);
    ASSERT_EQ(result.Point(), Vector_<>(6, 0.0));
    ASSERT_EQ(result.Directions()(0, 0), 1.0);
    ASSERT_EQ(result.Steps()[0], 5e-5);
    ASSERT_TRUE(result.Calibration().Matches(original));
    ASSERT_FALSE(result.Calibration().Matches(snapshot));
    ASSERT_NEAR(result.HessianProducts()(0, 0), 2.0, 1e-10);
}

TEST(DupireQuoteCurvatureTest, TestEmptyDirectionsAndIndependentConcurrentOwningResults) {
    const Dal::AAD::FlatIVS_ base(100.0, 0.05, 0.02, 0.2);
    const auto snapshot = Dal::CalibrateDupireWithRisk(base, Dal::Test::SmallRiskInputs());
    Dal::AAD::BumpOverAADRequest_ empty;
    empty.directions_ = Matrix_<>(0, 6);
    const auto firstOrder = Dal::EvaluateDupireQuoteCurvature(QuoteSquare, snapshot, empty);
    ASSERT_EQ(firstOrder.HessianProducts().Rows(), 0);
    ASSERT_EQ(firstOrder.HessianProducts().Cols(), 6);
    ASSERT_EQ(firstOrder.Execution().calibrations_, 1);
    Vector_<std::future<Dal::DupireQuoteCurvatureResult_>> futures;
    for (int index = 0; index < 3; ++index)
        futures.push_back(std::async(std::launch::async, [snapshot, index]() {
            const auto shifted = Dal::RecalibrateDupireWithRisk(snapshot, Matrix_<>(3, 2, 0.001 * (index + 1)));
            const auto mode = Dal::AAD::SetNumResultsForAAD(true, 4);
            return Dal::EvaluateDupireQuoteCurvature(QuoteSquare, shifted, Direction({1, 0, 0, 0, 0, 0}, 5e-5));
        }));
    for (int index = 0; index < 3; ++index) {
        const auto result = futures[index].get();
        const double quote = 0.001 * (index + 1);
        ASSERT_NEAR(result.Value(), quote * quote, 1e-10);
        ASSERT_NEAR(result.Gradient()[0], 2.0 * quote, 1e-10);
        ASSERT_NEAR(result.HessianProducts()(0, 0), 2.0, 1e-10);
        ASSERT_EQ(result.Calibration().Inputs().quoteSpreads_(0, 0), quote);
    }
    ASSERT_EQ(firstOrder.Point(), Vector_<>(6, 0.0));
}

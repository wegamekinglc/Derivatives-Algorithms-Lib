//
// Created by Codex on 2026/10/10.
//

#include <gtest/gtest.h>

#include <array>
#include <future>
#include <limits>
#include <string>

#include <dal-public/src/europeanpderisk.hpp>

using namespace Dal;

TEST(EuropeanPdeRiskTest, TestOwningSmallGridMatchesAcceptedFullFinancialChain) {
    EuropeanPdeRiskRequest_ request;
    request.point_ = {0.05, 0.20, 110.0};
    request.settings_.gridPoints_ = 9;
    request.settings_.ordinarySteps_ = 8;
    const auto result = EvaluateEuropeanPdeRisk(request);
    const std::array<double, 2> prices = {4.153690693968586, 10.770781220697858};
    const std::array<std::array<double, 3>, 2> risks = {
        {{40.40684028445804, 27.8766807705311, -0.0907395605282994}, {-64.1496803210784, 27.87666960963085, 0.8605082862555484}}};
    for (int layer = 0; layer < 2; ++layer) {
        ASSERT_NEAR(result.prices_[layer], prices[layer], 1e-10);
        for (int coordinate = 0; coordinate < 3; ++coordinate)
            ASSERT_NEAR(result.jacobian_(layer, coordinate), risks[layer][coordinate], 1e-9);
    }
    ASSERT_EQ(result.forwardBackwardErrors_.Rows(), 10);
    ASSERT_EQ(result.forwardBackwardErrors_.Cols(), 2);
    ASSERT_EQ(result.transposeBackwardErrors_.Rows(), 10);
    ASSERT_EQ(result.transposeBackwardErrors_.Cols(), 4);
    for (double error : result.forwardBackwardErrors_)
        ASSERT_LE(error, 1e-12);
    for (double error : result.transposeBackwardErrors_)
        ASSERT_LE(error, 1e-12);
    ASSERT_EQ(*result.request_.settings_.spotIndex_, 2);
    ASSERT_DOUBLE_EQ(result.spot_, 100.0);
    ASSERT_EQ(result.grid_.size(), 9);
    ASSERT_EQ(result.execution_.actualSteps_, 10);
    ASSERT_EQ(result.execution_.numericPayloadBytes_, 8 * (17 + 9 + 6 * 10));
    ASSERT_EQ(result.method_, "NativeAADFixedGridEuropeanTheta");
    ASSERT_EQ(result.payoffLabels_, (Vector_<String_>{"Call", "Put"}));
    ASSERT_EQ(result.parameterLabels_, (Vector_<String_>{"Rate", "Volatility", "Strike"}));
}

namespace {
    EuropeanPdeRiskRequest_ SmallRequest() {
        EuropeanPdeRiskRequest_ request;
        request.settings_.gridPoints_ = 9;
        request.settings_.ordinarySteps_ = 8;
        return request;
    }
} // namespace

TEST(EuropeanPdeRiskTest, TestExactNumericPayloadAndZeroShortBudgetsRecover) {
    auto request = SmallRequest();
    const auto expected = EvaluateEuropeanPdeRisk(request);
    const size_t bytes = expected.execution_.numericPayloadBytes_;
    for (size_t cap : {size_t(0), bytes - 1}) {
        request.numericPayloadBudgetBytes_ = cap;
        ASSERT_THROW(static_cast<void>(EvaluateEuropeanPdeRisk(request)), Exception_);
    }
    request.numericPayloadBudgetBytes_ = bytes;
    ASSERT_DOUBLE_EQ(EvaluateEuropeanPdeRisk(request).prices_[0], expected.prices_[0]);
    request.recordingCapacityBudgetBytes_ = 0;
    ASSERT_THROW(static_cast<void>(EvaluateEuropeanPdeRisk(request)), Exception_);
    request.recordingCapacityBudgetBytes_.reset();
    ASSERT_DOUBLE_EQ(EvaluateEuropeanPdeRisk(request).prices_[1], expected.prices_[1]);
}

TEST(EuropeanPdeRiskTest, TestChronologicalDiagnosticsMatchTheirNativeStepAndSeed) {
    using namespace AAD;
    for (int intervals : {8, 4096}) {
        auto request = SmallRequest();
        request.settings_.ordinarySteps_ = intervals;
        const auto result = EvaluateEuropeanPdeRisk(request);
        auto mode = SetNumResultsForAAD(true, 2);
        RecordingScope_ recording;
        std::array<Number_, 3> parameters;
        for (int coordinate = 0; coordinate < 3; ++coordinate)
            recording.RegisterInput(parameters[coordinate], request.point_[coordinate]);
        recording.StartRecording();
        auto native = RecordEuropeanOptions(&recording, request.settings_, parameters, true);
        recording.FinishRecording();
        recording.ClearAdjoints();
        for (size_t layer = 0; layer < 2; ++layer)
            NativeOperations_::SetSeed(native.prices_[layer], 1.0, layer);
        const auto reports = ReverseWithSolveAccuracy(&recording);
        for (int step = 0; step < intervals + 2; ++step) {
            const auto& errors = reports.Report(native.events_[step]).transposeBackwardErrors_;
            for (int layer = 0; layer < 2; ++layer) {
                ASSERT_DOUBLE_EQ(result.forwardBackwardErrors_(step, layer), native.forwardBackwardErrors_(step, layer));
                for (int channel = 0; channel < 2; ++channel)
                    ASSERT_DOUBLE_EQ(result.transposeBackwardErrors_(step, 2 * layer + channel), errors(layer, channel));
            }
        }
        recording.Close();
    }
}

TEST(EuropeanPdeRiskTest, TestActualFreshThreadRecordingPeakExactAndOneByteShort) {
    const auto fresh = [](EuropeanPdeRiskRequest_ request) {
        return std::async(std::launch::async, [request] { return EvaluateEuropeanPdeRisk(request); }).get();
    };
    auto request = SmallRequest();
    const auto accepted = fresh(request);
    const size_t cap = accepted.execution_.peakTapeBytes_ + accepted.execution_.cleanupReserveBytes_;
    ASSERT_GT(accepted.execution_.reverseScratchPeakBytes_, 0);
    request.recordingCapacityBudgetBytes_ = cap;
    ASSERT_DOUBLE_EQ(fresh(request).prices_[0], accepted.prices_[0]);
    request.recordingCapacityBudgetBytes_ = cap - 1;
    ASSERT_THROW(fresh(request), Exception_);
}

TEST(EuropeanPdeRiskTest, TestActiveRecordingAndLegacyGraphRejectedWithoutMutation) {
    using namespace AAD;
    Clear(*Tape());
    auto mode = SetNumResultsForAAD(true, 3);
    {
        RecordingScope_ scope;
        Number_ input;
        scope.RegisterInput(input, 2.0);
        scope.StartRecording();
        Number_ square = input * input;
        scope.FinishRecording();
        scope.ClearAdjoints();
        NativeOperations_::SetSeed(square, 7.0, 2);
        const size_t nodes = Tape()->nodes_.OccupiedSlots();
        ASSERT_THROW(static_cast<void>(EvaluateEuropeanPdeRisk(SmallRequest())), Exception_);
        ASSERT_EQ(Tape()->nodes_.OccupiedSlots(), nodes);
        ASSERT_EQ(Tape()->numAdj_, 3);
        ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(square, 2), 7.0);
        scope.Reverse();
        ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(input, 2), 28.0);
        scope.Close();
    }
    {
        Number_ input = 3.0;
        Number_ square = input * input;
        NativeOperations_::SetSeed(square, 2.0, 1);
        const size_t nodes = Tape()->nodes_.OccupiedSlots();
        ASSERT_THROW(static_cast<void>(EvaluateEuropeanPdeRisk(SmallRequest())), Exception_);
        ASSERT_EQ(Tape()->nodes_.OccupiedSlots(), nodes);
        ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(square, 1), 2.0);
        PropagateToStart(*Tape());
        ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(input, 1), 12.0);
        Clear(*Tape());
    }
    ASSERT_NO_THROW(static_cast<void>(EvaluateEuropeanPdeRisk(SmallRequest())));
    ASSERT_TRUE(Tape()->multi_);
    ASSERT_EQ(Tape()->numAdj_, 3);
}

TEST(EuropeanPdeRiskTest, TestInputAndPostRecordingFailuresRestoreModeAndRecover) {
    using namespace AAD;
    Clear(*Tape());
    auto mode = SetNumResultsForAAD(false, 1);
    const auto valid = SmallRequest();
    for (int coordinate = 0; coordinate < 3; ++coordinate) {
        auto request = valid;
        request.point_[coordinate] = std::numeric_limits<double>::quiet_NaN();
        ASSERT_THROW(static_cast<void>(EvaluateEuropeanPdeRisk(request)), Exception_);
    }
    {
        auto request = valid;
        request.point_[2] = 100.0;
        ASSERT_THROW(static_cast<void>(EvaluateEuropeanPdeRisk(request)), Exception_);
    }
    {
        auto request = valid;
        request.point_[1] = std::numeric_limits<double>::max();
        ASSERT_THROW(static_cast<void>(EvaluateEuropeanPdeRisk(request)), Exception_);
    }
    ASSERT_FALSE(Tape()->multi_);
    ASSERT_EQ(Tape()->numAdj_, 1);
    ASSERT_EQ(Tape()->nodes_.OccupiedSlots(), 0);
    ASSERT_NO_THROW(static_cast<void>(EvaluateEuropeanPdeRisk(valid)));
    ASSERT_FALSE(Tape()->multi_);
    ASSERT_EQ(Tape()->numAdj_, 1);
}

TEST(EuropeanPdeRiskTest, TestErrorLimitsRejectBeforeBudgetsAndPreserveMode) {
    using namespace AAD;
    Clear(*Tape());
    auto mode = SetNumResultsForAAD(false, 1);
    for (bool forward : {true, false}) {
        auto request = SmallRequest();
        auto& limit = forward ? request.settings_.accuracy_.forwardBackwardErrorLimit_ : request.settings_.accuracy_.transposeBackwardErrorLimit_;
        for (double boundary : {0.0, 1.0}) {
            limit = boundary;
            ASSERT_NO_THROW(static_cast<void>(ResolveEuropeanThetaSettings(request.settings_)));
        }
        limit = 2.0;
        ASSERT_THROW(static_cast<void>(ResolveEuropeanThetaSettings(request.settings_)), Exception_);
        request.numericPayloadBudgetBytes_ = 0;
        request.recordingCapacityBudgetBytes_ = 0;
        try {
            static_cast<void>(EvaluateEuropeanPdeRisk(request));
            FAIL() << "Invalid solve-error limit was accepted";
        } catch (const Exception_& error) {
            ASSERT_NE(std::string(error.what()).find(forward ? "forward_backward_error_limit" : "transpose_backward_error_limit"), std::string::npos);
        }
        ASSERT_FALSE(Tape()->multi_);
        ASSERT_EQ(Tape()->numAdj_, 1);
        ASSERT_EQ(Tape()->nodes_.OccupiedSlots(), 0);
    }
}

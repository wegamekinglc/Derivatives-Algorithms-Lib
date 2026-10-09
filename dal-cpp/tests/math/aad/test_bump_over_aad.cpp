//
// Created by Codex on 2026/10/09.
//

#include <gtest/gtest.h>

#include <cmath>
#include <future>
#include <limits>
#include <string>

#include <dal/math/aad/bumpoveraad.hpp>
#include <dal/math/aad/linearsolve.hpp>
#include <dal/math/aad/native.hpp>

using namespace Dal;
using namespace Dal::AAD;

namespace {
    BumpOverAADRequest_ Direction(const Vector_<>& values, double step) {
        BumpOverAADRequest_ request;
        request.directions_ = Matrix_<>(1, static_cast<int>(values.size()), 0.0);
        for (size_t column = 0; column < values.size(); ++column)
            request.directions_(0, static_cast<int>(column)) = values[column];
        request.steps_ = {step};
        return request;
    }

    Number_ SquaredKernel(RecordingScope_*, const Vector_<Number_>& x) { return x[0] * x[0]; }

    Number_ SoftAbsolute(RecordingScope_*, const Vector_<Number_>& x) {
        const Number_ magnitude = abs(x[0]);
        const Number_ decay = exp(-magnitude);
        return magnitude + log(1.0 + decay * decay);
    }
} // namespace

TEST(AADBumpOverAADTest, TestIndependentGammaCrossGammaAndSignedHessianProducts) {
    BumpOverAADRequest_ request;
    request.directions_ = Matrix_<>(3, 3, 0.0);
    request.directions_(0, 0) = 1.0;
    request.directions_(1, 1) = 1.0;
    request.directions_(2, 0) = 2.0;
    request.directions_(2, 1) = -3.0;
    request.directions_(2, 2) = 0.5;
    request.steps_ = {1e-3, 2e-3, 5e-4};
    const Vector_<> point{2.0, -1.0, 4.0};
    const auto result = EvaluateBumpOverAAD(
        [](RecordingScope_*, const Vector_<Number_>& x) -> Number_ { return 3.0 * x[0] * x[0] + 2.0 * x[0] * x[1] + 5.0 * x[1] * x[1] + 7.0 * x[2]; },
        point, request);
    ASSERT_DOUBLE_EQ(result.Value(), 41.0);
    ASSERT_EQ(result.Gradient(), (Vector_<>{10.0, -6.0, 7.0}));
    const double expected[3][3] = {{6.0, 2.0, 0.0}, {2.0, 10.0, 0.0}, {6.0, -26.0, 0.0}};
    ASSERT_EQ(result.HessianProducts().Rows(), 3);
    ASSERT_EQ(result.HessianProducts().Cols(), 3);
    for (int row = 0; row < 3; ++row)
        for (int column = 0; column < 3; ++column)
            ASSERT_NEAR(result.HessianProducts()(row, column), expected[row][column], 1e-10);
    ASSERT_EQ(result.Point(), point);
    ASSERT_EQ(result.Steps(), request.steps_);
    ASSERT_EQ(result.Execution().method_, "BumpOverNativeAAD");
    ASSERT_EQ(result.Execution().gradientEvaluations_, 7);
    ASSERT_EQ(result.Execution().reverseSweeps_, 7);
    ASSERT_EQ(result.Execution().numericPayloadBytes_, 28 * sizeof(double));
    ASSERT_EQ(BumpOverAADPayloadBytes(3, 3), 28 * sizeof(double));
    ASSERT_FALSE(NativeOperations_::Capabilities().higherOrder_);
    request.steps_[0] = 99.0;
    request.directions_(0, 0) = -99.0;
    ASSERT_EQ(result.Steps()[0], 1e-3);
    ASSERT_EQ(result.Directions()(0, 0), 1.0);
}

TEST(AADBumpOverAADTest, TestFreshRecordedSolveCurvatureAgainstRationalFormula) {
    BumpOverAADRequest_ request;
    request.directions_ = Matrix_<>(1, 2, 0.0);
    request.directions_(0, 0) = 1.0;
    request.directions_(0, 1) = -2.0;
    request.steps_ = {1e-4};
    const auto result = EvaluateBumpOverAAD(
        [](RecordingScope_* recording, const Vector_<Number_>& x) -> Number_ {
            SquareMatrix_<Number_> matrix(1);
            Matrix_<Number_> rhs(1, 1);
            matrix(0, 0) = x[0];
            rhs(0, 0) = x[1];
            const auto solution = LinearSolve(recording, matrix, rhs);
            return solution(0, 0) * solution(0, 0);
        },
        {2.0, 3.0}, request);
    ASSERT_DOUBLE_EQ(result.Value(), 2.25);
    ASSERT_EQ(result.Gradient(), (Vector_<>{-2.25, 1.5}));
    ASSERT_NEAR(result.HessianProducts()(0, 0), 6.375, 1e-7);
    ASSERT_NEAR(result.HessianProducts()(0, 1), -2.5, 1e-7);
}

TEST(AADBumpOverAADTest, TestQuarticCentralStepConvergenceWithoutSymmetryCorrection) {
    Vector_<> errors;
    for (double step : {0.2, 0.1, 0.05}) {
        const auto result = EvaluateBumpOverAAD([](RecordingScope_*, const Vector_<Number_>& x) -> Number_ { return x[0] * x[0] * x[0] * x[0]; },
                                                {2.0}, Direction({1.0}, step));
        ASSERT_DOUBLE_EQ(result.Value(), 16.0);
        ASSERT_DOUBLE_EQ(result.Gradient()[0], 32.0);
        ASSERT_NEAR(result.HessianProducts()(0, 0), 48.0 + 4.0 * step * step, 1e-10);
        errors.push_back(result.HessianProducts()(0, 0) - 48.0);
    }
    ASSERT_NEAR(errors[0] / errors[1], 4.0, 1e-8);
    ASSERT_NEAR(errors[1] / errors[2], 4.0, 1e-8);
}

TEST(AADBumpOverAADTest, TestSmoothMixedKernelAgainstAnalyticHessian) {
    const auto result = EvaluateBumpOverAAD([](RecordingScope_*, const Vector_<Number_>& x) -> Number_ { return exp(x[0] * x[1]) + log(x[0]); },
                                            {2.0, 0.3}, Direction({1.25, -0.5}, 1e-4));
    const double exponential = std::exp(0.6);
    ASSERT_NEAR(result.Value(), exponential + std::log(2.0), 1e-10);
    ASSERT_NEAR(result.Gradient()[0], 0.3 * exponential + 0.5, 1e-10);
    ASSERT_NEAR(result.Gradient()[1], 2.0 * exponential, 1e-10);
    const double xx = 0.09 * exponential - 0.25, xy = 1.6 * exponential, yy = 4.0 * exponential;
    ASSERT_NEAR(result.HessianProducts()(0, 0), 1.25 * xx - 0.5 * xy, 1e-7);
    ASSERT_NEAR(result.HessianProducts()(0, 1), 1.25 * xy - 0.5 * yy, 1e-7);
}

TEST(AADBumpOverAADTest, TestRepresentableCentralQuotientsAtExtremeScales) {
    const double maximum = std::numeric_limits<double>::max();
    const auto large = EvaluateBumpOverAAD(
        [maximum](RecordingScope_*, const Vector_<Number_>& x) -> Number_ { return (0.5 * maximum) * x[0] * x[0]; }, {0.0}, Direction({1.0}, 1.0));
    ASSERT_DOUBLE_EQ(large.HessianProducts()(0, 0), maximum);
    const auto tinyStep = EvaluateBumpOverAAD(SquaredKernel, {0.0}, Direction({1.0}, std::numeric_limits<double>::denorm_min()));
    ASSERT_DOUBLE_EQ(tinyStep.HessianProducts()(0, 0), 2.0);
    const auto hugeStep = EvaluateBumpOverAAD(SoftAbsolute, {0.0}, Direction({1.0}, maximum));
    const double expected = 1.0 / maximum;
    ASSERT_NE(expected, 0.0);
    ASSERT_NEAR(hugeStep.HessianProducts()(0, 0), expected, 4.0 * std::numeric_limits<double>::denorm_min());
    ASSERT_THROW((void)EvaluateBumpOverAAD([maximum](RecordingScope_*, const Vector_<Number_>& x) -> Number_ { return maximum * x[0] * x[0]; }, {0.0},
                                           Direction({1.0}, 1e-100)),
                 Exception_);
    ASSERT_THROW((void)EvaluateBumpOverAAD([](RecordingScope_* recording, const Vector_<Number_>& x)
                                               -> Number_ { return std::numeric_limits<double>::min() * SoftAbsolute(recording, x); },
                                           {0.0}, Direction({1.0}, maximum)),
                 Exception_);
}

TEST(AADBumpOverAADTest, TestCompleteAdmissionBeforeAnyCallback) {
    int calls = 0;
    const NativeScalarFunction_ function = [&calls](RecordingScope_*, const Vector_<Number_>& x) -> Number_ {
        ++calls;
        return x[0] * x[0];
    };
    ASSERT_THROW((void)EvaluateBumpOverAAD({}, {1.0}, Direction({1.0}, 0.1)), Exception_);
    for (double invalid : {0.0, -1.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()})
        ASSERT_THROW((void)EvaluateBumpOverAAD(function, {1.0}, Direction({1.0}, invalid)), Exception_);
    for (double invalid : {std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
        ASSERT_THROW((void)EvaluateBumpOverAAD(function, {invalid}, Direction({1.0}, 0.1)), Exception_);
        ASSERT_THROW((void)EvaluateBumpOverAAD(function, {1.0}, Direction({invalid}, 0.1)), Exception_);
    }
    ASSERT_THROW((void)EvaluateBumpOverAAD(function, {1.0}, Direction({0.0}, 0.1)), Exception_);
    ASSERT_THROW((void)EvaluateBumpOverAAD(function, {1.0}, Direction({1.0, 2.0}, 0.1)), Exception_);
    auto missingStep = Direction({1.0}, 0.1);
    missingStep.steps_.clear();
    ASSERT_THROW((void)EvaluateBumpOverAAD(function, {1.0}, missingStep), Exception_);
    auto laterInvalid = Direction({1.0}, 0.1);
    laterInvalid.directions_ = Matrix_<>(2, 1, 1.0);
    laterInvalid.steps_ = {0.1, 0.0};
    ASSERT_THROW((void)EvaluateBumpOverAAD(function, {1.0}, laterInvalid), Exception_);
    BumpOverAADRequest_ negativeRows;
    negativeRows.directions_ = Matrix_<>(-1, 0);
    ASSERT_THROW((void)EvaluateBumpOverAAD(function, {}, negativeRows), Exception_);
    ASSERT_THROW((void)EvaluateBumpOverAAD(function, {1e300}, Direction({1.0}, 1e-3)), Exception_);
    ASSERT_THROW((void)EvaluateBumpOverAAD(function, {1.0, 1e300}, Direction({1.0, 1.0}, 1e-3)), Exception_);
    ASSERT_THROW((void)EvaluateBumpOverAAD(function, {std::numeric_limits<double>::max()}, Direction({1.0}, std::numeric_limits<double>::max())),
                 Exception_);
    ASSERT_THROW(
        (void)EvaluateBumpOverAAD(function, {0.0}, Direction({std::numeric_limits<double>::denorm_min()}, std::numeric_limits<double>::denorm_min())),
        Exception_);
    auto tooSmall = Direction({1.0}, 0.1);
    tooSmall.numericPayloadBudgetBytes_ = 6 * sizeof(double) - 1;
    ASSERT_THROW((void)EvaluateBumpOverAAD(function, {1.0}, tooSmall), Exception_);
    ASSERT_THROW((void)BumpOverAADPayloadBytes(std::numeric_limits<size_t>::max(), 0), Exception_);
    ASSERT_THROW((void)BumpOverAADPayloadBytes(1, std::numeric_limits<size_t>::max()), Exception_);
    ASSERT_EQ(calls, 0);
}

TEST(AADBumpOverAADTest, TestExactPayloadAndTapeBudgetsAndRecovery) {
    auto request = Direction({1.0}, 0.1);
    request.numericPayloadBudgetBytes_ = 6 * sizeof(double);
    const auto reference = EvaluateBumpOverAAD(SquaredKernel, {2.0}, request);
    ASSERT_EQ(reference.Execution().numericPayloadBytes_, 6 * sizeof(double));
    ASSERT_GT(reference.Execution().peakTapeBytes_, 0);
    ASSERT_GT(reference.Execution().cleanupReserveBytes_, 0);
    request.recordingCapacityBudgetBytes_ = reference.Execution().peakTapeBytes_ + reference.Execution().cleanupReserveBytes_;
    const auto exact = EvaluateBumpOverAAD(SquaredKernel, {2.0}, request);
    ASSERT_EQ(exact.Execution().peakTapeBytes_ + exact.Execution().cleanupReserveBytes_, *request.recordingCapacityBudgetBytes_);
    --*request.recordingCapacityBudgetBytes_;
    int calls = 0;
    ASSERT_THROW((void)EvaluateBumpOverAAD(
                     [&calls](RecordingScope_*, const Vector_<Number_>& x) -> Number_ {
                         ++calls;
                         return x[0] * x[0];
                     },
                     {2.0}, request),
                 Exception_);
    ASSERT_EQ(calls, 0);
    request.recordingCapacityBudgetBytes_.reset();
    ASSERT_NEAR(EvaluateBumpOverAAD(SquaredKernel, {3.0}, request).HessianProducts()(0, 0), 2.0, 1e-10);
}

TEST(AADBumpOverAADTest, TestEmptyRequestsAliasesConstantsAndCallerModes) {
    for (const auto [multi, width] : {std::pair{false, size_t{1}}, std::pair{true, size_t{4}}}) {
        const auto mode = SetNumResultsForAAD(multi, width);
        for (bool alias : {false, true}) {
            const auto result =
                EvaluateBumpOverAAD([alias](RecordingScope_*, const Vector_<Number_>& x) -> Number_ { return alias ? x[0] : Number_(7.0); }, {2.0},
                                    Direction({1.0}, 0.1));
            ASSERT_DOUBLE_EQ(result.Value(), alias ? 2.0 : 7.0);
            ASSERT_DOUBLE_EQ(result.Gradient()[0], alias ? 1.0 : 0.0);
            ASSERT_DOUBLE_EQ(result.HessianProducts()(0, 0), 0.0);
            ASSERT_EQ(Tape()->multi_, multi);
            ASSERT_EQ(Tape()->numAdj_, width);
        }
        BumpOverAADRequest_ noDirections;
        noDirections.directions_ = Matrix_<>(0, 2);
        const auto gradient =
            EvaluateBumpOverAAD([](RecordingScope_*, const Vector_<Number_>& x) -> Number_ { return x[0] - x[1]; }, {2.0, 3.0}, noDirections);
        ASSERT_DOUBLE_EQ(gradient.Value(), -1.0);
        ASSERT_EQ(gradient.Gradient(), (Vector_<>{1.0, -1.0}));
        ASSERT_EQ(gradient.HessianProducts().Rows(), 0);
        ASSERT_EQ(gradient.HessianProducts().Cols(), 2);
        ASSERT_EQ(gradient.Execution().gradientEvaluations_, 1);
        const auto constant = EvaluateBumpOverAAD([](RecordingScope_*, const Vector_<Number_>&) -> Number_ { return Number_(5.0); }, {}, {});
        ASSERT_DOUBLE_EQ(constant.Value(), 5.0);
        ASSERT_TRUE(constant.Gradient().empty());
        ASSERT_EQ(constant.Execution().numericPayloadBytes_, sizeof(double));
        ASSERT_EQ(Tape()->multi_, multi);
        ASSERT_EQ(Tape()->numAdj_, width);
    }
}

TEST(AADBumpOverAADTest, TestEveryCallbackPhaseFailureAndPrematureClosureRecover) {
    const auto mode = SetNumResultsForAAD(true, 4);
    const auto request = Direction({1.0}, 0.1);
    for (int failAt : {1, 2, 3}) {
        int calls = 0;
        try {
            (void)EvaluateBumpOverAAD(
                [&](RecordingScope_*, const Vector_<Number_>& x) -> Number_ {
                    if (++calls == failAt)
                        THROW("callback failure");
                    return x[0] * x[0];
                },
                {2.0}, request);
            FAIL() << "Expected callback failure";
        } catch (const Exception_& error) {
            const std::string message = error.what();
            const std::string phase = failAt == 1 ? "base" : failAt == 2 ? "plus" : "minus";
            ASSERT_NE(message.find(phase), std::string::npos);
        }
        ASSERT_EQ(calls, failAt);
        ASSERT_TRUE(Tape()->multi_);
        ASSERT_EQ(Tape()->numAdj_, 4);
        ASSERT_NEAR(EvaluateBumpOverAAD(SquaredKernel, {3.0}, request).HessianProducts()(0, 0), 2.0, 1e-10);
    }
    ASSERT_THROW((void)EvaluateBumpOverAAD(
                     [](RecordingScope_* recording, const Vector_<Number_>& x) -> Number_ {
                         recording->Close();
                         return x[0];
                     },
                     {2.0}, request),
                 Exception_);
    ASSERT_THROW(
        (void)EvaluateBumpOverAAD(
            [](RecordingScope_*, const Vector_<Number_>&) -> Number_ { return Number_(std::numeric_limits<double>::infinity()); }, {2.0}, request),
        Exception_);
    ASSERT_THROW((void)EvaluateBumpOverAAD([](RecordingScope_*, const Vector_<Number_>& x) -> Number_ { return sqrt(x[0]); }, {0.0}, request),
                 Exception_);
    ASSERT_NEAR(EvaluateBumpOverAAD(SquaredKernel, {4.0}, request).HessianProducts()(0, 0), 2.0, 1e-10);
    ASSERT_TRUE(Tape()->multi_);
    ASSERT_EQ(Tape()->numAdj_, 4);
}

TEST(AADBumpOverAADTest, TestNestedRejectionPreservesOuterGraph) {
    const auto mode = SetNumResultsForAAD(false, 1);
    RecordingScope_ recording;
    Number_ input;
    recording.RegisterInput(input, 2.0);
    recording.StartRecording();
    Number_ output = input * input;
    recording.FinishRecording();
    const auto nodes = Tape()->nodes_.OccupiedSlots();
    ASSERT_THROW((void)EvaluateBumpOverAAD(SquaredKernel, {2.0}, Direction({1.0}, 0.1)), Exception_);
    ASSERT_EQ(Tape()->nodes_.OccupiedSlots(), nodes);
    recording.ClearAdjoints();
    NativeOperations_::SetSeed(output, 1.0);
    recording.Reverse();
    ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(input), 4.0);
    recording.Close();
}

TEST(AADBumpOverAADTest, TestCallerMutationCannotChangeAdmittedInputsFunctionOrSettings) {
    Vector_<> point{2.0};
    auto request = Direction({1.0}, 0.1);
    NativeScalarFunction_ function;
    int calls = 0;
    function = [&](RecordingScope_*, const Vector_<Number_>& x) -> Number_ {
        if (++calls == 1) {
            point[0] = 999.0;
            request.directions_(0, 0) = 0.0;
            request.steps_[0] = 0.0;
            request.recordingCapacityBudgetBytes_ = 0;
            function = [](RecordingScope_*, const Vector_<Number_>&) -> Number_ { return Number_(-999.0); };
        }
        return x[0] * x[0];
    };
    const auto result = EvaluateBumpOverAAD(function, point, request);
    ASSERT_EQ(calls, 3);
    ASSERT_DOUBLE_EQ(result.Value(), 4.0);
    ASSERT_EQ(result.Gradient(), (Vector_<>{4.0}));
    ASSERT_NEAR(result.HessianProducts()(0, 0), 2.0, 1e-10);
    ASSERT_EQ(result.Point(), (Vector_<>{2.0}));
    ASSERT_EQ(result.Directions()(0, 0), 1.0);
    ASSERT_EQ(result.Steps(), (Vector_<>{0.1}));
}

TEST(AADBumpOverAADTest, TestIndependentConcurrentCallersAndDetachedResults) {
    Vector_<std::future<BumpOverAADResult_>> futures;
    for (int i = 0; i < 4; ++i)
        futures.push_back(std::async(std::launch::async, [i]() {
            return EvaluateBumpOverAAD([](RecordingScope_*, const Vector_<Number_>& x) -> Number_ { return x[0] * x[0] + 3.0 * x[0] * x[1]; },
                                       {double(i + 1), 2.0}, Direction({1.0, -2.0}, 1e-3));
        }));
    Vector_<BumpOverAADResult_> results;
    for (auto& future : futures)
        results.push_back(future.get());
    (void)EvaluateBumpOverAAD(SquaredKernel, {-7.0}, Direction({1.0}, 0.1));
    for (size_t i = 0; i < results.size(); ++i) {
        const double point = double(i + 1);
        ASSERT_DOUBLE_EQ(results[i].Value(), point * point + 6.0 * point);
        ASSERT_EQ(results[i].Gradient(), (Vector_<>{2.0 * point + 6.0, 3.0 * point}));
        ASSERT_NEAR(results[i].HessianProducts()(0, 0), -4.0, 1e-10);
        ASSERT_NEAR(results[i].HessianProducts()(0, 1), 3.0, 1e-10);
    }
}

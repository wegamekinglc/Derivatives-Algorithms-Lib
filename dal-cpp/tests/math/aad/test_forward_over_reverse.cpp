//
// Created by Codex on 2026/10/10.
//

#include <gtest/gtest.h>

#include <cmath>
#include <future>
#include <limits>
#include <new>
#include <string>

#include <dal/math/aad/bumpoveraad.hpp>
#include <dal/math/aad/forwardoverreverse.hpp>
#include <dal/math/aad/linearsolve.hpp>
#include <dal/math/aad/native.hpp>

using namespace Dal;
using namespace Dal::AAD;

namespace {
    ForwardOverReverseRequest_ Direction(const Vector_<>& values) {
        ForwardOverReverseRequest_ request;
        request.directions_ = Matrix_<>(1, static_cast<int>(values.size()));
        for (size_t column = 0; column < values.size(); ++column)
            request.directions_(0, static_cast<int>(column)) = values[column];
        return request;
    }

    ForwardOverReverseNumber_ Squared(RecordingScope_*, const Vector_<ForwardOverReverseNumber_>& x) { return x[0] * x[0]; }
} // namespace

TEST(AADForwardOverReverseTest, TestQuarticProductIsIndependentOfAnOuterStep) {
    Dal::AAD::ForwardOverReverseRequest_ request;
    request.directions_ = Dal::Matrix_<>(1, 1, 1.0);
    const auto result = Dal::AAD::EvaluateForwardOverReverse(
        [](Dal::AAD::RecordingScope_*, const Dal::Vector_<Dal::AAD::ForwardOverReverseNumber_>& x) { return x[0] * x[0] * x[0] * x[0]; }, {2.0},
        request);
    ASSERT_DOUBLE_EQ(result.Value(), 16.0);
    ASSERT_DOUBLE_EQ(result.Gradient()[0], 32.0);
    ASSERT_NEAR(result.HessianProducts()(0, 0), 48.0, 1e-11);
    ASSERT_DOUBLE_EQ(result.DirectionalDerivatives()[0], 32.0);
    ASSERT_EQ(result.Execution().callbackEvaluations_, 1);
    ASSERT_EQ(result.Execution().recordings_, 1);
    ASSERT_EQ(result.Execution().reverseSweeps_, 2);
}

TEST(AADForwardOverReverseTest, TestSignedCrossProductsWorkCountsAndOwningResults) {
    ForwardOverReverseRequest_ request;
    request.directions_ = Matrix_<>(3, 3, 0.0);
    request.directions_(0, 0) = 1.0;
    request.directions_(1, 1) = 1.0;
    request.directions_(2, 0) = 2.0;
    request.directions_(2, 1) = -3.0;
    request.directions_(2, 2) = 0.5;
    Vector_<> point{2.0, -1.0, 4.0};
    int calls = 0;
    const auto result = EvaluateForwardOverReverse(
        [&calls](RecordingScope_*, const Vector_<ForwardOverReverseNumber_>& x) {
            ++calls;
            return 3.0 * x[0] * x[0] + 2.0 * x[0] * x[1] + 5.0 * x[1] * x[1] + 7.0 * x[2];
        },
        point, request);
    ASSERT_EQ(calls, 3);
    ASSERT_DOUBLE_EQ(result.Value(), 41.0);
    ASSERT_EQ(result.Gradient(), (Vector_<>{10.0, -6.0, 7.0}));
    ASSERT_EQ(result.DirectionalDerivatives(), (Vector_<>{10.0, -6.0, 41.5}));
    const double expected[3][3] = {{6.0, 2.0, 0.0}, {2.0, 10.0, 0.0}, {6.0, -26.0, 0.0}};
    ASSERT_EQ(result.HessianProducts().Rows(), 3);
    ASSERT_EQ(result.HessianProducts().Cols(), 3);
    for (int row = 0; row < 3; ++row)
        for (int column = 0; column < 3; ++column)
            ASSERT_DOUBLE_EQ(result.HessianProducts()(row, column), expected[row][column]);
    ASSERT_EQ(result.Execution().method_, "NativeForwardOverReversePrototype");
    ASSERT_EQ(result.Execution().recordings_, 3);
    ASSERT_EQ(result.Execution().reverseSweeps_, 4);
    ASSERT_EQ(result.Execution().numericPayloadBytes_, 28 * sizeof(double));
    request.directions_(0, 0) = -99.0;
    point[0] = 99.0;
    ASSERT_DOUBLE_EQ(result.Directions()(0, 0), 1.0);
    ASSERT_DOUBLE_EQ(result.Point()[0], 2.0);
    ASSERT_TRUE(ForwardOverReverseCapabilities().smoothDirectionalProducts_);
    ASSERT_TRUE(ForwardOverReverseCapabilities().prototype_);
    ASSERT_FALSE(ForwardOverReverseCapabilities().independentNesting_);
    ASSERT_FALSE(ForwardOverReverseCapabilities().reverseEvents_);
    ASSERT_FALSE(ForwardOverReverseCapabilities().nonsmoothOperators_);
    ASSERT_FALSE(NativeOperations_::Capabilities().higherOrder_);
}

TEST(AADForwardOverReverseTest, TestExpLogCompositionAgainstAnalyticMixedHessian) {
    const auto result =
        EvaluateForwardOverReverse([](RecordingScope_*, const Vector_<ForwardOverReverseNumber_>& x) { return exp(x[0] * x[1]) + log(x[0]); },
                                   {2.0, 0.3}, Direction({1.25, -0.5}));
    const double e = std::exp(0.6);
    ASSERT_NEAR(result.Value(), e + std::log(2.0), 1e-12);
    ASSERT_NEAR(result.Gradient()[0], 0.3 * e + 0.5, 1e-12);
    ASSERT_NEAR(result.Gradient()[1], 2.0 * e, 1e-12);
    ASSERT_NEAR(result.HessianProducts()(0, 0), 1.25 * (0.09 * e - 0.25) - 0.5 * 1.6 * e, 1e-12);
    ASSERT_NEAR(result.HessianProducts()(0, 1), 1.25 * 1.6 * e - 0.5 * 4.0 * e, 1e-12);
}

TEST(AADForwardOverReverseTest, TestUnaryPrimitivesAgainstIndependentDerivatives) {
    using Unary = std::function<ForwardOverReverseNumber_(const ForwardOverReverseNumber_&)>;
    const double x = 1.3, direction = -0.75;
    const double density = std::exp(-0.5 * x * x) / std::sqrt(2.0 * std::acos(-1.0));
    const double erfcPrime = -2.0 * std::exp(-x * x) / std::sqrt(std::acos(-1.0));
    const Unary functions[] = {[](const auto& a) { return exp(a); },  [](const auto& a) { return log(a); },  [](const auto& a) { return sqrt(a); },
                               [](const auto& a) { return erfc(a); }, [](const auto& a) { return NCDF(a); }, [](const auto& a) { return NPDF(a); }};
    const double values[] = {std::exp(x), std::log(x), std::sqrt(x), std::erfc(x), 0.5 * std::erfc(-x / std::sqrt(2.0)), density};
    const double first[] = {std::exp(x), 1.0 / x, 0.5 / std::sqrt(x), erfcPrime, density, -x * density};
    const double second[] = {std::exp(x), -1.0 / (x * x), -0.25 / (x * std::sqrt(x)), -2.0 * x * erfcPrime, -x * density, (x * x - 1.0) * density};
    for (size_t i = 0; i < 6; ++i) {
        SCOPED_TRACE(i);
        const auto result =
            EvaluateForwardOverReverse([&](RecordingScope_*, const auto& a) { return functions[i](a[0]); }, {x}, Direction({direction}));
        ASSERT_NEAR(result.Value(), values[i], 1e-12);
        ASSERT_NEAR(result.Gradient()[0], first[i], 1e-12);
        ASSERT_NEAR(result.DirectionalDerivatives()[0], direction * first[i], 1e-12);
        ASSERT_NEAR(result.HessianProducts()(0, 0), direction * second[i], 1e-12);
    }
}

TEST(AADForwardOverReverseTest, TestZeroTangentsAndPassiveIntegerPowersRetainCurvature) {
    for (double exponent : {0.0, 1.0, 2.0, 3.0, 4.0}) {
        const auto result =
            EvaluateForwardOverReverse([exponent](RecordingScope_*, const auto& x) { return pow(x[0], exponent); }, {0.0}, Direction({1.0}));
        ASSERT_DOUBLE_EQ(result.Value(), exponent == 0.0 ? 1.0 : 0.0);
        ASSERT_DOUBLE_EQ(result.Gradient()[0], exponent == 1.0 ? 1.0 : 0.0);
        ASSERT_DOUBLE_EQ(result.DirectionalDerivatives()[0], exponent == 1.0 ? 1.0 : 0.0);
        ASSERT_DOUBLE_EQ(result.HessianProducts()(0, 0), exponent == 2.0 ? 2.0 : 0.0);
    }
    const auto negative = EvaluateForwardOverReverse([](RecordingScope_*, const auto& x) { return pow(x[0], -2.0); }, {-2.0}, Direction({3.0}));
    ASSERT_DOUBLE_EQ(negative.Value(), 0.25);
    ASSERT_DOUBLE_EQ(negative.Gradient()[0], 0.25);
    ASSERT_DOUBLE_EQ(negative.HessianProducts()(0, 0), 1.125);
    const auto zero = EvaluateForwardOverReverse(Squared, {2.0}, Direction({0.0}));
    ASSERT_DOUBLE_EQ(zero.Gradient()[0], 4.0);
    ASSERT_DOUBLE_EQ(zero.HessianProducts()(0, 0), 0.0);
    const auto tiny = EvaluateForwardOverReverse(Squared, {0.0}, Direction({1e-200}));
    ASSERT_DOUBLE_EQ(tiny.DirectionalDerivatives()[0], 0.0);
    ASSERT_DOUBLE_EQ(tiny.HessianProducts()(0, 0), 2e-200);
}

TEST(AADForwardOverReverseTest, TestActivePowersAgainstIndependentMixedDerivatives) {
    ForwardOverReverseRequest_ request;
    request.directions_ = Matrix_<>(2, 2, 0.0);
    request.directions_(0, 0) = 1.0;
    request.directions_(1, 1) = 1.0;
    const auto result = EvaluateForwardOverReverse([](RecordingScope_*, const auto& x) { return pow(x[0], x[1]); }, {2.0, 3.0}, request);
    const double l = std::log(2.0), mixed = 4.0 * (1.0 + 3.0 * l);
    ASSERT_DOUBLE_EQ(result.Value(), 8.0);
    ASSERT_DOUBLE_EQ(result.Gradient()[0], 12.0);
    ASSERT_NEAR(result.Gradient()[1], 8.0 * l, 1e-12);
    ASSERT_DOUBLE_EQ(result.HessianProducts()(0, 0), 12.0);
    ASSERT_NEAR(result.HessianProducts()(0, 1), mixed, 1e-12);
    ASSERT_NEAR(result.HessianProducts()(1, 0), mixed, 1e-12);
    ASSERT_NEAR(result.HessianProducts()(1, 1), 8.0 * l * l, 1e-12);
    const auto passive = EvaluateForwardOverReverse([](RecordingScope_*, const auto& x) { return pow(2.0, x[0]); }, {3.0}, Direction({-2.0}));
    ASSERT_DOUBLE_EQ(passive.Value(), 8.0);
    ASSERT_NEAR(passive.Gradient()[0], 8.0 * l, 1e-12);
    ASSERT_NEAR(passive.HessianProducts()(0, 0), -16.0 * l * l, 1e-12);
}

TEST(AADForwardOverReverseTest, TestDivisionAndAliasedCompoundAssignment) {
    const auto quotient = EvaluateForwardOverReverse([](RecordingScope_*, const auto& x) { return x[0] / x[1]; }, {2.0, 3.0}, Direction({2.0, -3.0}));
    ASSERT_NEAR(quotient.Gradient()[0], 1.0 / 3.0, 1e-12);
    ASSERT_NEAR(quotient.Gradient()[1], -2.0 / 9.0, 1e-12);
    ASSERT_NEAR(quotient.HessianProducts()(0, 0), 1.0 / 3.0, 1e-12);
    ASSERT_NEAR(quotient.HessianProducts()(0, 1), -2.0 / 3.0, 1e-12);
    const auto alias = EvaluateForwardOverReverse(
        [](RecordingScope_*, const auto& x) {
            auto y = x[0];
            y *= y;
            y += x[0];
            y -= x[0];
            y /= 2.0;
            y *= 2.0;
            y += 3.0;
            y -= 3.0;
            return +y;
        },
        {0.0}, Direction({1.0}));
    ASSERT_DOUBLE_EQ(alias.Value(), 0.0);
    ASSERT_DOUBLE_EQ(alias.Gradient()[0], 0.0);
    ASSERT_DOUBLE_EQ(alias.HessianProducts()(0, 0), 2.0);
    const auto reciprocal = EvaluateForwardOverReverse([](RecordingScope_*, const auto& x) { return 6.0 / x[0] + (2.0 - x[0]) - (-x[0] / 2.0); },
                                                       {2.0}, Direction({1.0}));
    ASSERT_DOUBLE_EQ(reciprocal.Value(), 4.0);
    ASSERT_DOUBLE_EQ(reciprocal.Gradient()[0], -2.0);
    ASSERT_DOUBLE_EQ(reciprocal.HessianProducts()(0, 0), 1.5);
}

TEST(AADForwardOverReverseTest, TestBlackScholesGammaVannaVolgaAgainstAnalyticFormula) {
    const double spot = 103.0, strike = 100.0, vol = 0.24, time = 1.7, rate = 0.03, q = 0.01;
    const double rootTime = std::sqrt(time), discounted = std::exp(-q * time), discount = std::exp(-rate * time);
    const double d1 = (std::log(spot / strike) + (rate - q + 0.5 * vol * vol) * time) / (vol * rootTime), d2 = d1 - vol * rootTime;
    const double density = std::exp(-0.5 * d1 * d1) / std::sqrt(2.0 * std::acos(-1.0));
    const double delta = discounted * 0.5 * std::erfc(-d1 / std::sqrt(2.0));
    const double vega = spot * discounted * density * rootTime;
    const double gamma = discounted * density / (spot * vol * rootTime), vanna = -discounted * density * d2 / vol;
    const double volga = vega * d1 * d2 / vol;
    ForwardOverReverseRequest_ request;
    request.directions_ = Matrix_<>(3, 2, 0.0);
    request.directions_(0, 0) = 1.0;
    request.directions_(1, 1) = 1.0;
    request.directions_(2, 0) = -2.0;
    request.directions_(2, 1) = 0.5;
    const auto kernel = [=](RecordingScope_*, const auto& x) {
        const auto first = (log(x[0] / strike) + (rate - q + 0.5 * x[1] * x[1]) * time) / (x[1] * rootTime);
        return x[0] * discounted * NCDF(first) - strike * discount * NCDF(first - x[1] * rootTime);
    };
    const auto result = EvaluateForwardOverReverse(kernel, {spot, vol}, request);
    BumpOverAADRequest_ gradientOnly;
    gradientOnly.directions_ = Matrix_<>(0, 2);
    const auto native = EvaluateBumpOverAAD(kernel, {spot, vol}, gradientOnly);
    const double price = spot * delta - strike * discount * 0.5 * std::erfc(-d2 / std::sqrt(2.0));
    ASSERT_NEAR(result.Value(), price, 1e-11);
    ASSERT_NEAR(result.Gradient()[0], delta, 1e-11);
    ASSERT_NEAR(result.Gradient()[1], vega, 1e-10);
    ASSERT_NEAR(result.Gradient()[0], native.Gradient()[0], 1e-12);
    ASSERT_NEAR(result.Gradient()[1], native.Gradient()[1], 1e-12);
    const double expected[3][2] = {{gamma, vanna}, {vanna, volga}, {-2.0 * gamma + 0.5 * vanna, -2.0 * vanna + 0.5 * volga}};
    for (int row = 0; row < 3; ++row)
        for (int column = 0; column < 2; ++column)
            ASSERT_NEAR(result.HessianProducts()(row, column), expected[row][column], 1e-10);
}

TEST(AADForwardOverReverseTest, TestAllocationFailurePreservesTypeAndCallerMode) {
    const auto mode = SetNumResultsForAAD(true, 4);
    ASSERT_THROW((void)EvaluateForwardOverReverse([](RecordingScope_*, const auto&) -> ForwardOverReverseNumber_ { throw std::bad_alloc(); }, {2.0},
                                                  Direction({1.0})),
                 std::bad_alloc);
    ASSERT_TRUE(Tape()->multi_);
    ASSERT_EQ(Tape()->numAdj_, 4);
    ASSERT_DOUBLE_EQ(EvaluateForwardOverReverse(Squared, {3.0}, Direction({1.0})).HessianProducts()(0, 0), 2.0);
}

TEST(AADForwardOverReverseTest, TestAdmissionRejectsBeforeAnyCallback) {
    int calls = 0;
    const NativeDirectionalFunction_ function = [&calls](RecordingScope_*, const auto& x) {
        ++calls;
        return x[0] * x[0];
    };
    ASSERT_THROW((void)EvaluateForwardOverReverse({}, {1.0}, Direction({1.0})), Exception_);
    ASSERT_THROW((void)EvaluateForwardOverReverse(function, {1.0}, Direction({1.0, 2.0})), Exception_);
    for (double invalid : {std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
        ASSERT_THROW((void)EvaluateForwardOverReverse(function, {invalid}, Direction({1.0})), Exception_);
        auto lateInvalid = Direction({1.0});
        lateInvalid.directions_ = Matrix_<>(2, 1, 1.0);
        lateInvalid.directions_(1, 0) = invalid;
        ASSERT_THROW((void)EvaluateForwardOverReverse(function, {1.0}, lateInvalid), Exception_);
    }
    ForwardOverReverseRequest_ negativeRows;
    negativeRows.directions_ = Matrix_<>(-1, 0);
    ASSERT_THROW((void)EvaluateForwardOverReverse(function, {}, negativeRows), Exception_);
    auto tooSmall = Direction({1.0});
    tooSmall.numericPayloadBudgetBytes_ = 6 * sizeof(double) - 1;
    ASSERT_THROW((void)EvaluateForwardOverReverse(function, {1.0}, tooSmall), Exception_);
    ASSERT_THROW((void)ForwardOverReversePayloadBytes(std::numeric_limits<size_t>::max(), 0), Exception_);
    ASSERT_THROW((void)ForwardOverReversePayloadBytes(1, std::numeric_limits<size_t>::max()), Exception_);
    ASSERT_EQ(calls, 0);
}

TEST(AADForwardOverReverseTest, TestExactNumericAndTapeBudgetsRecover) {
    auto request = Direction({1.0});
    request.numericPayloadBudgetBytes_ = 6 * sizeof(double);
    const auto reference = EvaluateForwardOverReverse(Squared, {2.0}, request);
    ASSERT_EQ(reference.Execution().numericPayloadBytes_, 6 * sizeof(double));
    ASSERT_GT(reference.Execution().peakTapeBytes_, 0);
    ASSERT_GT(reference.Execution().cleanupReserveBytes_, 0);
    request.recordingCapacityBudgetBytes_ = reference.Execution().peakTapeBytes_ + reference.Execution().cleanupReserveBytes_;
    const auto exact = EvaluateForwardOverReverse(Squared, {2.0}, request);
    ASSERT_EQ(exact.Execution().peakTapeBytes_ + exact.Execution().cleanupReserveBytes_, *request.recordingCapacityBudgetBytes_);
    --*request.recordingCapacityBudgetBytes_;
    int calls = 0;
    ASSERT_THROW((void)EvaluateForwardOverReverse(
                     [&calls](RecordingScope_* scope, const auto& x) {
                         ++calls;
                         return Squared(scope, x);
                     },
                     {2.0}, request),
                 Exception_);
    ASSERT_EQ(calls, 0);
    request.recordingCapacityBudgetBytes_.reset();
    ASSERT_DOUBLE_EQ(EvaluateForwardOverReverse(Squared, {3.0}, request).HessianProducts()(0, 0), 2.0);
}

TEST(AADForwardOverReverseTest, TestEmptyDirectionsInputsAliasesAndCallerModes) {
    for (const auto [multi, width] : {std::pair{false, size_t{1}}, std::pair{true, size_t{4}}}) {
        const auto mode = SetNumResultsForAAD(multi, width);
        for (bool alias : {false, true}) {
            const auto result = EvaluateForwardOverReverse(
                [alias](RecordingScope_*, const auto& x) { return alias ? x[0] : ForwardOverReverseNumber_(7.0); }, {2.0}, Direction({1.0}));
            ASSERT_DOUBLE_EQ(result.Value(), alias ? 2.0 : 7.0);
            ASSERT_DOUBLE_EQ(result.Gradient()[0], alias ? 1.0 : 0.0);
            ASSERT_DOUBLE_EQ(result.DirectionalDerivatives()[0], alias ? 1.0 : 0.0);
            ASSERT_DOUBLE_EQ(result.HessianProducts()(0, 0), 0.0);
        }
        ForwardOverReverseRequest_ empty;
        empty.directions_ = Matrix_<>(0, 2);
        const auto gradient = EvaluateForwardOverReverse([](RecordingScope_*, const auto& x) { return x[0] - x[1]; }, {2.0, 3.0}, empty);
        ASSERT_DOUBLE_EQ(gradient.Value(), -1.0);
        ASSERT_EQ(gradient.Gradient(), (Vector_<>{1.0, -1.0}));
        ASSERT_TRUE(gradient.DirectionalDerivatives().empty());
        ASSERT_EQ(gradient.HessianProducts().Rows(), 0);
        ASSERT_EQ(gradient.HessianProducts().Cols(), 2);
        ASSERT_EQ(gradient.Execution().callbackEvaluations_, 1);
        ASSERT_EQ(gradient.Execution().reverseSweeps_, 1);
        const auto constant = EvaluateForwardOverReverse([](RecordingScope_*, const auto&) { return ForwardOverReverseNumber_(5.0); }, {}, {});
        ASSERT_DOUBLE_EQ(constant.Value(), 5.0);
        ASSERT_TRUE(constant.Gradient().empty());
        ASSERT_EQ(constant.Execution().numericPayloadBytes_, sizeof(double));
        ASSERT_EQ(Tape()->multi_, multi);
        ASSERT_EQ(Tape()->numAdj_, width);
    }
}

TEST(AADForwardOverReverseTest, TestPrimitiveDomainAndNonfiniteOutputFailuresRecover) {
    const NativeDirectionalFunction_ invalid[] = {
        [](RecordingScope_*, const auto& x) { return log(x[0]); },
        [](RecordingScope_*, const auto& x) { return sqrt(x[0]); },
        [](RecordingScope_*, const auto& x) { return pow(x[0], -1.0); },
        [](RecordingScope_*, const auto& x) { return pow(x[0], 0.5); },
        [](RecordingScope_*, const auto& x) { return pow(x[0], x[0]); },
        [](RecordingScope_*, const auto& x) { return pow(0.0, x[0]); },
        [](RecordingScope_*, const auto& x) { return x[0] / 0.0; },
        [](RecordingScope_*, const auto& x) { return 1.0 / x[0]; },
        [](RecordingScope_*, const auto& x) { return x[0] / x[0]; },
        [](RecordingScope_*, const auto&) { return ForwardOverReverseNumber_(std::numeric_limits<double>::infinity()); },
        [](RecordingScope_*, const auto& x) { return x[0] + std::numeric_limits<double>::quiet_NaN(); }};
    for (const auto& function : invalid) {
        ASSERT_THROW((void)EvaluateForwardOverReverse(function, {0.0}, Direction({1.0})), Exception_);
        ASSERT_DOUBLE_EQ(EvaluateForwardOverReverse(Squared, {3.0}, Direction({1.0})).HessianProducts()(0, 0), 2.0);
    }
    ASSERT_THROW((void)EvaluateForwardOverReverse([](RecordingScope_*, const auto& x) { return exp(x[0]); }, {1000.0}, Direction({1.0})), Exception_);
    const double maximum = std::numeric_limits<double>::max();
    ASSERT_THROW(
        (void)EvaluateForwardOverReverse([maximum](RecordingScope_*, const auto& x) { return maximum * x[0] * x[0]; }, {0.0}, Direction({1.0})),
        Exception_);
    ASSERT_DOUBLE_EQ(EvaluateForwardOverReverse(Squared, {0.0}, Direction({1.0})).HessianProducts()(0, 0), 2.0);
}

TEST(AADForwardOverReverseTest, TestCallbackFailuresInEveryDirectionRestoreCallerMode) {
    const auto mode = SetNumResultsForAAD(true, 4);
    ForwardOverReverseRequest_ request;
    request.directions_ = Matrix_<>(3, 1, 1.0);
    for (int failAt : {1, 2, 3}) {
        int calls = 0;
        try {
            (void)EvaluateForwardOverReverse(
                [&](RecordingScope_* scope, const auto& x) {
                    if (++calls == failAt)
                        THROW("callback failure");
                    return Squared(scope, x);
                },
                {2.0}, request);
            FAIL() << "Expected callback failure";
        } catch (const Exception_& error) {
            ASSERT_NE(std::string(error.what()).find("direction=" + std::to_string(failAt - 1)), std::string::npos);
        }
        ASSERT_EQ(calls, failAt);
        ASSERT_TRUE(Tape()->multi_);
        ASSERT_EQ(Tape()->numAdj_, 4);
        ASSERT_DOUBLE_EQ(EvaluateForwardOverReverse(Squared, {3.0}, request).HessianProducts()(2, 0), 2.0);
    }
}

TEST(AADForwardOverReverseTest, TestCheckpointRestoreAndPrematureCloseRejectBeforeRoots) {
    const auto mode = SetNumResultsForAAD(true, 4);
    for (int operation : {0, 1, 2}) {
        ASSERT_THROW((void)EvaluateForwardOverReverse(
                         [operation](RecordingScope_* scope, const auto& x) {
                             if (operation == 2) {
                                 scope->Close();
                                 return x[0];
                             }
                             const auto checkpoint = scope->MakeCheckpoint();
                             auto result = x[0] * x[0];
                             if (operation == 1)
                                 scope->Restore(checkpoint);
                             return result;
                         },
                         {2.0}, Direction({1.0})),
                     Exception_);
        ASSERT_TRUE(Tape()->multi_);
        ASSERT_EQ(Tape()->numAdj_, 4);
        ASSERT_DOUBLE_EQ(EvaluateForwardOverReverse(Squared, {3.0}, Direction({1.0})).HessianProducts()(0, 0), 2.0);
    }
}

TEST(AADForwardOverReverseTest, TestOpaqueReverseEventsRejectBeforeReverse) {
    ASSERT_THROW((void)EvaluateForwardOverReverse(
                     [](RecordingScope_* scope, const auto& x) {
                         SquareMatrix_<Number_> matrix(1);
                         matrix(0, 0) = 2.0;
                         Matrix_<Number_> rhs(1, 1);
                         rhs(0, 0) = 3.0;
                         const auto solution = LinearSolve(scope, matrix, rhs);
                         (void)solution;
                         return x[0] * x[0];
                     },
                     {2.0}, Direction({1.0})),
                 Exception_);
    ASSERT_DOUBLE_EQ(EvaluateForwardOverReverse(Squared, {3.0}, Direction({1.0})).HessianProducts()(0, 0), 2.0);
}

TEST(AADForwardOverReverseTest, TestNestedAdmissionPreservesOuterGraph) {
    const auto mode = SetNumResultsForAAD(false, 1);
    RecordingScope_ recording;
    Number_ input;
    recording.RegisterInput(input, 2.0);
    recording.StartRecording();
    Number_ output = input * input;
    recording.FinishRecording();
    const auto nodes = Tape()->nodes_.OccupiedSlots();
    ASSERT_THROW((void)EvaluateForwardOverReverse(Squared, {2.0}, Direction({1.0})), Exception_);
    ASSERT_EQ(Tape()->nodes_.OccupiedSlots(), nodes);
    recording.ClearAdjoints();
    NativeOperations_::SetSeed(output, 1.0);
    recording.Reverse();
    ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(input), 4.0);
    recording.Close();
}

TEST(AADForwardOverReverseTest, TestSnapshotsSurviveCallbackMutation) {
    Vector_<> point{2.0};
    auto request = Direction({1.0});
    request.directions_ = Matrix_<>(2, 1, 1.0);
    NativeDirectionalFunction_ function;
    int calls = 0;
    function = [&](RecordingScope_*, const auto& x) {
        if (++calls == 1) {
            point[0] = 999.0;
            request.directions_(1, 0) = -999.0;
            request.recordingCapacityBudgetBytes_ = 0;
            function = [](RecordingScope_*, const auto&) { return ForwardOverReverseNumber_(-999.0); };
        }
        return x[0] * x[0];
    };
    const auto result = EvaluateForwardOverReverse(function, point, request);
    ASSERT_EQ(calls, 2);
    ASSERT_DOUBLE_EQ(result.Value(), 4.0);
    ASSERT_DOUBLE_EQ(result.HessianProducts()(1, 0), 2.0);
    ASSERT_EQ(result.Point(), (Vector_<>{2.0}));
    ASSERT_DOUBLE_EQ(result.Directions()(1, 0), 1.0);
}

TEST(AADForwardOverReverseTest, TestChangedCallbackValueRejectsAndRecovers) {
    auto request = Direction({1.0});
    request.directions_ = Matrix_<>(2, 1, 1.0);
    int calls = 0;
    ASSERT_THROW((void)EvaluateForwardOverReverse([&](RecordingScope_*, const auto& x) { return x[0] * x[0] + double(++calls); }, {2.0}, request),
                 Exception_);
    ASSERT_EQ(calls, 2);
    ASSERT_DOUBLE_EQ(EvaluateForwardOverReverse(Squared, {3.0}, request).HessianProducts()(1, 0), 2.0);
}

TEST(AADForwardOverReverseTest, TestConcurrentIndependentCallersOwnDetachedResults) {
    Vector_<std::future<ForwardOverReverseResult_>> futures;
    for (int i = 0; i < 4; ++i)
        futures.push_back(std::async(std::launch::async, [i]() {
            return EvaluateForwardOverReverse([](RecordingScope_*, const auto& x) { return x[0] * x[0] + 3.0 * x[0] * x[1]; }, {double(i + 1), 2.0},
                                              Direction({1.0, -2.0}));
        }));
    Vector_<ForwardOverReverseResult_> results;
    for (auto& future : futures)
        results.push_back(future.get());
    (void)EvaluateForwardOverReverse(Squared, {-7.0}, Direction({1.0}));
    for (size_t i = 0; i < results.size(); ++i) {
        ASSERT_DOUBLE_EQ(results[i].Value(), double((i + 1) * (i + 1) + 6 * (i + 1)));
        ASSERT_DOUBLE_EQ(results[i].Gradient()[0], double(2 * (i + 1) + 6));
        ASSERT_DOUBLE_EQ(results[i].Gradient()[1], double(3 * (i + 1)));
        ASSERT_DOUBLE_EQ(results[i].HessianProducts()(0, 0), -4.0);
        ASSERT_DOUBLE_EQ(results[i].HessianProducts()(0, 1), 3.0);
    }
}

TEST(AADForwardOverReverseTest, TestSmoothGradientAgreesWithSeparateNativeRecording) {
    const auto kernel = [](RecordingScope_*, const auto& x) { return exp(x[0] * x[1]) + log(x[0]) + NCDF(x[1]) + pow(x[0], 3.0); };
    BumpOverAADRequest_ gradientOnly;
    gradientOnly.directions_ = Matrix_<>(0, 2);
    const auto native = EvaluateBumpOverAAD(kernel, {2.0, 0.3}, gradientOnly);
    const auto mixed = EvaluateForwardOverReverse(kernel, {2.0, 0.3}, Direction({1.0, -2.0}));
    ASSERT_DOUBLE_EQ(mixed.Value(), native.Value());
    ASSERT_NEAR(mixed.Gradient()[0], native.Gradient()[0], 1e-12);
    ASSERT_NEAR(mixed.Gradient()[1], native.Gradient()[1], 1e-12);
}

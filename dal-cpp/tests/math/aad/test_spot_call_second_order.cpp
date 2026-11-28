#include <gtest/gtest.h>

#include <algorithm>
#include <cfenv>
#include <cmath>
#include <cstring>
#include <future>
#include <limits>

#include <dal/math/analytics/detail/spotcallrisk.hpp>

using namespace Dal;
using namespace Dal::AAD;

namespace {
    using SpotCallDetail::FixedSpotCall_;
    using Reason = SecondOrderErrorReason_::Value_;
    using Stage = SecondOrderStage_::Value_;
    namespace Tape = SecondOrderDetail;

    const FixedSpotCall_ CALL(100.0, 1.7, 0.03, 0.01);
    const Vector_<> POINT{103.0, 0.24};

    template <class F_> void CheckError(F_&& f, Reason reason, Stage stage, std::optional<size_t> direction = std::nullopt) {
        try {
            f();
            FAIL() << "fixed Spot call should throw";
        } catch (const SecondOrderError_& error) {
            EXPECT_EQ(error.Reason().Switch(), reason);
            EXPECT_EQ(error.Stage().Switch(), stage);
            EXPECT_EQ(error.DirectionIndex(), direction);
        }
    }

    class FloatingEnvironment_ {
        std::fenv_t saved_{};

    public:
        FloatingEnvironment_() { EXPECT_EQ(std::fegetenv(&saved_), 0); }
        ~FloatingEnvironment_() { EXPECT_EQ(std::fesetenv(&saved_), 0); }
    };

    class CleanupFailure_ {
    public:
        CleanupFailure_() { Tape::SetSecondOrderCleanupFailureForTest(true); }
        ~CleanupFailure_() { Tape::SetSecondOrderCleanupFailureForTest(false); }
    };

    void ExpectSameReplay(const FixedSpotCall_& call, const Vector_<>& point, const Matrix_<>& directions, const char* reason) {
        const auto scalar = EvaluateSecondOrder(
            [&](const auto& x) {
                if (!Tape::activeArena->wide_)
                    Tape::Promote("explicit scalar reference");
                return call.Scalar(x[0], x[1]);
            },
            point, directions);
        const auto actual = call.Evaluate(point, directions);
        EXPECT_EQ(actual.Value(), scalar.Value());
        EXPECT_EQ(actual.Gradient(), scalar.Gradient());
        EXPECT_EQ(actual.DirectionalDerivatives(), scalar.DirectionalDerivatives());
        EXPECT_TRUE(std::equal(actual.HessianProducts().begin(), actual.HessianProducts().end(), scalar.HessianProducts().begin()));
        EXPECT_EQ(actual.Execution().method_, "NativeSpotCallWideReplay");
        EXPECT_EQ(actual.Execution().recordings_, 2U);
        EXPECT_EQ(actual.Execution().reverseSweeps_, 1U);
        EXPECT_EQ(actual.Execution().safePathActivations_, 1U);
        EXPECT_STREQ(Tape::LastSecondOrderProtection(), reason);
    }
} // namespace

TEST(SpotCallSecondOrderTest, OwnsPriceGreeksAndActualRoute) {
    auto x = POINT;
    Matrix_<> v(3, 2, -0.0);
    v(0, 0) = v(1, 1) = 1.0;
    v(2, 0) = -2.0;
    v(2, 1) = 0.5;
    const auto scalar = EvaluateSecondOrder([&](const auto& a) { return CALL.Scalar(a[0], a[1]); }, x, v);
    const auto risk = CALL.Evaluate(x, v);
    x.clear();
    v.Fill(0.0);
    const double pv = risk.Value(), reference = scalar.Value();
    EXPECT_EQ(std::memcmp(&pv, &reference, sizeof(pv)), 0);
    EXPECT_EQ(risk.Point(), POINT);
    EXPECT_TRUE(std::signbit(risk.Directions()(0, 1)));
    EXPECT_EQ(risk.Execution().method_, "NativeSpotCallDoubleWeights");
    EXPECT_EQ(risk.Execution().recordings_, 3U);
    EXPECT_EQ(risk.Execution().reverseSweeps_, 3U);
    EXPECT_EQ(risk.Execution().safePathActivations_, 0U);
    for (int i = 0; i < 2; ++i)
        EXPECT_NEAR(risk.Gradient()[i], scalar.Gradient()[i], 5e-13);
    for (int row = 0; row < 3; ++row) {
        EXPECT_NEAR(risk.DirectionalDerivatives()[row], scalar.DirectionalDerivatives()[row], 5e-13);
        for (int i = 0; i < 2; ++i)
            EXPECT_NEAR(risk.HessianProducts()(row, i), scalar.HessianProducts()(row, i), 5e-13);
    }
}

TEST(SpotCallSecondOrderTest, ValidatesPositiveDomainAndRoundingBeforeTape) {
    const auto before = Tape::LastSecondOrderLogicalPeak();
    const Matrix_<> direction(1, 2, 0.0);
    for (const auto& point : {Vector_<>{0.0, 0.24}, Vector_<>{103.0, -0.0}, Vector_<>{103.0, -0.24}})
        CheckError([&] { (void)CALL.Evaluate(point, direction, {0, 0}); }, Reason::OUTSIDE_DOMAIN, Stage::VALIDATION);
    CheckError([&] { (void)CALL.Evaluate({std::numeric_limits<double>::infinity(), 0.24}, direction); }, Reason::INVALID_INPUT, Stage::VALIDATION);
    CheckError([&] { (void)CALL.Evaluate({103.0}, direction); }, Reason::INVALID_INPUT, Stage::VALIDATION);
    CheckError([] { (void)FixedSpotCall_(100.0, -1.0, 0.0, 0.0); }, Reason::OUTSIDE_DOMAIN, Stage::VALIDATION);
    CheckError([] { (void)FixedSpotCall_(0.0, 1.0, 0.0, 0.0); }, Reason::OUTSIDE_DOMAIN, Stage::VALIDATION);
    {
        const FloatingEnvironment_ restore;
        ASSERT_EQ(std::fesetround(FE_DOWNWARD), 0);
        CheckError([] { (void)FixedSpotCall_(100.0, 1.0, 0.0, 0.0); }, Reason::UNSUPPORTED_OPERATION, Stage::VALIDATION);
        CheckError([&] { (void)CALL.Evaluate(POINT, direction); }, Reason::UNSUPPORTED_OPERATION, Stage::VALIDATION);
        EXPECT_EQ(std::fegetround(), FE_DOWNWARD);
    }
    EXPECT_EQ(Tape::LastSecondOrderLogicalPeak(), before);
    EXPECT_EQ(CALL.Evaluate(POINT, direction).Execution().method_, "NativeSpotCallDoubleWeights");
}

TEST(SpotCallSecondOrderTest, ReplaysAmplifiedCancellationAndNormalTail) {
    const auto normal = CALL.Evaluate(POINT, Matrix_<>(0, 2));
    Matrix_<> cancellation(1, 2);
    cancellation(0, 0) = 0x1p18;
    cancellation(0, 1) = -cancellation(0, 0) * normal.Gradient()[0] / normal.Gradient()[1];
    ExpectSameReplay(CALL, POINT, cancellation, "Spot call derivative cancellation");
    const FixedSpotCall_ tail(1.0, 1.0, 0.0, 0.0);
    Matrix_<> amplified(1, 2, 0.0);
    amplified(0, 1) = 1e308;
    ExpectSameReplay(tail, {1.0, 80.0}, amplified, "Spot call derivative range");
    const auto risk = tail.Evaluate({1.0, 80.0}, amplified);
    EXPECT_NEAR(risk.DirectionalDerivatives()[0] / 1.463270250838e-40, 1.0, 1e-11);
    EXPECT_NEAR(risk.HessianProducts()(0, 1) / -2.926540501676e-39, 1.0, 1e-11);

    Matrix_<> mixed(2, 2, 0.0);
    mixed(0, 0) = 1.0;
    std::copy(cancellation.begin(), cancellation.end(), mixed.Row(1).begin());
    const auto mixedRisk = CALL.Evaluate(POINT, mixed);
    EXPECT_EQ(mixedRisk.Execution().safePathActivations_, 1U);
    EXPECT_EQ(mixedRisk.Execution().recordings_, 3U);
    EXPECT_EQ(mixedRisk.Execution().method_, "NativeSpotCallMixedReplay");
}

TEST(SpotCallSecondOrderTest, IndependentEntryCannotFeedAnAmplifiedActiveDifference) {
    Matrix_<> direction(1, 2);
    direction(0, 0) = 0.7;
    direction(0, 1) = -0.3;
    const auto outer = EvaluateSecondOrder(
        [&](const auto& x) {
            const auto& arena = *Tape::activeArena;
            const auto before = arena.slots_;
            // Regression for the amplified Spot-minus-Spot expression. Only
            // scalar evaluation is callable inside this independent recording.
            CheckError([&] { (void)CALL.Evaluate(POINT, direction); }, Reason::LIFECYCLE, Stage::VALIDATION);
            EXPECT_EQ(arena.slots_, before);
            const SecondOrderNumber_ left = CALL.Scalar(x[0], x[1]), right = CALL.Scalar(x[0], x[1]);
            return 0x1p30 * (left - right);
        },
        POINT, direction);
    EXPECT_EQ(outer.Value(), 0.0);
    EXPECT_EQ(outer.DirectionalDerivatives()[0], 0.0);
    EXPECT_NEAR(outer.HessianProducts()(0, 1), 0.0, 1e-7);
}

TEST(SpotCallSecondOrderTest, PreservesBothWeightsWhenAxesRepeatTheSameSlot) {
    const FixedSpotCall_ linked(1.0, 1.7, 0.03, 0.01, {0, 0});
    const Vector_<> point{1.0};
    const Matrix_<> directions(1, 1, 0.4);
    const auto scalar = EvaluateSecondOrder([&](const auto& x) { return linked.Scalar(x[0], x[0]); }, point, directions);
    const auto actual = linked.Evaluate(point, directions);
    EXPECT_EQ(actual.Execution().method_, "NativeSpotCallDoubleWeights");
    EXPECT_EQ(actual.Execution().safePathActivations_, 0U);
    EXPECT_EQ(actual.Value(), scalar.Value());
    EXPECT_NEAR(actual.Gradient()[0], scalar.Gradient()[0], 1e-14);
    EXPECT_NEAR(actual.DirectionalDerivatives()[0], scalar.DirectionalDerivatives()[0], 1e-14);
    EXPECT_NEAR(actual.HessianProducts()(0, 0), scalar.HessianProducts()(0, 0), 1e-14);
}

TEST(SpotCallSecondOrderTest, EnforcesExactCapsAndPreservesFirstFailureWhenCleanupAlsoFails) {
    for (double seed : {1.0, 0x1p30}) {
        Matrix_<> direction(1, 2, 0.0);
        direction(0, 0) = seed;
        const auto run = [&](const SecondOrderOptions_& options = {}) { return CALL.Evaluate(POINT, direction, options); };
        const auto cold = std::async(std::launch::async, [&] { return run().Execution().peakTapeBytes_; }).get();
        std::async(std::launch::async, [&] {
            CheckError([&] { (void)run({{}, cold - 1}); }, Reason::BUDGET_EXCEEDED, Stage::REVERSE, 0);
            EXPECT_EQ(run({{}, cold}).Execution().peakTapeBytes_, cold);
            const auto hot = run().Execution().peakTapeBytes_;
            EXPECT_EQ(run({{}, hot}).Execution().peakTapeBytes_, hot);
            {
                const CleanupFailure_ inject;
                CheckError([&] { (void)run({{}, hot - 1}); }, Reason::BUDGET_EXCEEDED, Stage::RECORDING, 0);
            }
            EXPECT_EQ(run().Execution().peakTapeBytes_, cold);
            {
                const CleanupFailure_ inject;
                CheckError([&] { (void)run(); }, Reason::LIFECYCLE, Stage::CLEANUP);
            }
            EXPECT_EQ(run().Execution().peakTapeBytes_, cold);
            const auto payload = SecondOrderPayloadBytes(2, direction.Rows());
            CheckError([&] { (void)run({payload - 1, {}}); }, Reason::BUDGET_EXCEEDED, Stage::VALIDATION);
            EXPECT_EQ(run({payload, {}}).Execution().numericPayloadBytes_, payload);
        }).get();
    }
}

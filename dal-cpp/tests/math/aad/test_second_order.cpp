#include <gtest/gtest.h>

#include <cmath>
#include <future>
#include <limits>

#include <dal/math/aad/recording.hpp>
#include <dal/math/aad/secondorder.hpp>

using namespace Dal;
using namespace Dal::AAD;

namespace {
    using Reason = SecondOrderErrorReason_::Value_;
    using Stage = SecondOrderStage_::Value_;

    template <class F_> void CheckError(const F_& evaluate, Reason reason, Stage stage, std::optional<size_t> row = {}) {
        try {
            evaluate();
            FAIL() << "expected a second-order error";
        } catch (const SecondOrderError_& error) {
            EXPECT_EQ(error.Reason().Switch(), reason);
            EXPECT_EQ(error.Stage().Switch(), stage);
            EXPECT_EQ(error.DirectionIndex(), row);
        }
    }

    const SecondOrderFunction_ square = [](const auto& x) -> SecondOrderNumber_ { return x[0] * x[0]; };
} // namespace

TEST(AADSecondOrderTest, ReturnsOwningGradientAndProductsInOneSweepPerDirection) {
    Matrix_<> directions(2, 2, 0.0);
    directions(0, 0) = 1.0;
    directions(1, 0) = -2.0;
    directions(1, 1) = 3.0;
    const auto risk = EvaluateSecondOrder([](const auto& x) -> SecondOrderNumber_ { return x[0] * x[0] * x[1]; }, {2.0, 5.0}, directions);
    directions.Fill(0.0);
    EXPECT_DOUBLE_EQ(risk.Value(), 20.0);
    EXPECT_EQ(risk.Point(), (Vector_<>{2.0, 5.0}));
    EXPECT_EQ(risk.Gradient(), (Vector_<>{20.0, 4.0}));
    EXPECT_EQ(risk.DirectionalDerivatives(), (Vector_<>{20.0, -28.0}));
    EXPECT_DOUBLE_EQ(risk.Directions()(1, 1), 3.0);
    EXPECT_DOUBLE_EQ(risk.HessianProducts()(0, 0), 10.0);
    EXPECT_DOUBLE_EQ(risk.HessianProducts()(0, 1), 4.0);
    EXPECT_DOUBLE_EQ(risk.HessianProducts()(1, 0), -8.0);
    EXPECT_DOUBLE_EQ(risk.HessianProducts()(1, 1), -8.0);
    EXPECT_EQ(risk.Execution().recordings_, 2U);
    EXPECT_EQ(risk.Execution().reverseSweeps_, 2U);
    EXPECT_EQ(risk.Execution().safePathActivations_, 0U);
}

TEST(AADSecondOrderTest, MaterializesScalarProductsBeforeRebindingAndPreservesOperandErrors) {
    const Matrix_<> seed(1, 1, 2.0);
    const auto risk = EvaluateSecondOrder(
        [](const auto& x) -> SecondOrderNumber_ {
            auto scaled = x[0];
            scaled = -2.0 * scaled;
            scaled *= scaled;
            const SecondOrderNumber_ half = scaled * 0.5;
            return half + SecondOrderNumber_(3.0);
        },
        {3.0}, seed);
    EXPECT_DOUBLE_EQ(risk.Value(), 21.0);
    EXPECT_DOUBLE_EQ(risk.Gradient()[0], 12.0);
    EXPECT_DOUBLE_EQ(risk.DirectionalDerivatives()[0], 24.0);
    EXPECT_DOUBLE_EQ(risk.HessianProducts()(0, 0), 8.0);
    EXPECT_EQ(risk.Execution().safePathActivations_, 0U);

    const SecondOrderFunction_ invalid = [](const auto& x) -> SecondOrderNumber_ {
        const double infinity = std::numeric_limits<double>::infinity();
        try {
            const SecondOrderNumber_ product = infinity * x[0];
            return product;
        } catch (const SecondOrderError_&) {
            return x[0];
        }
    };
    CheckError([&] { (void)EvaluateSecondOrder(invalid, {0.0}, seed); }, Reason::NUMERICAL_FAILURE, Stage::RECORDING, 0);
    EXPECT_DOUBLE_EQ(EvaluateSecondOrder(square, {2.0}, seed).Value(), 4.0);
}

TEST(AADSecondOrderTest, RejectsAFailedRightSubtreeAfterPreparingTheLeftSubtree) {
    const Matrix_<> seed(1, 1, 1.0);
    const SecondOrderFunction_ evaluate = [](const auto& x) -> SecondOrderNumber_ {
        try {
            const SecondOrderNumber_ price = (x[0] * x[0] + x[0]) / (x[0] - 2.0);
            return price;
        } catch (const SecondOrderError_&) {
            return x[0];
        }
    };
    CheckError([&] { (void)EvaluateSecondOrder(evaluate, {2.0}, seed); }, Reason::OUTSIDE_DOMAIN, Stage::RECORDING, 0);
    const auto risk = EvaluateSecondOrder(evaluate, {3.0}, seed);
    EXPECT_DOUBLE_EQ(risk.Value(), 12.0);
    EXPECT_DOUBLE_EQ(risk.Gradient()[0], -5.0);
    EXPECT_DOUBLE_EQ(risk.DirectionalDerivatives()[0], -5.0);
    EXPECT_DOUBLE_EQ(risk.HessianProducts()(0, 0), 12.0);
}

TEST(AADSecondOrderTest, RecordsRepeatedSlotsAroundPassiveLeavesInNestedStatements) {
    Matrix_<> direction(1, 2);
    direction(0, 0) = 2.0;
    direction(0, 1) = -1.0;
    const auto risk = EvaluateSecondOrder(
        [](const auto& x) -> SecondOrderNumber_ {
            const SecondOrderNumber_ passive(3.0);
            return (x[0] * passive + x[1]) * (3.0 * (x[0] - passive) + x[1]);
        },
        {1.0, 2.0}, direction);
    EXPECT_DOUBLE_EQ(risk.Value(), -20.0);
    EXPECT_EQ(risk.Gradient(), (Vector_<>{3.0, 1.0}));
    EXPECT_DOUBLE_EQ(risk.DirectionalDerivatives()[0], 5.0);
    EXPECT_DOUBLE_EQ(risk.HessianProducts()(0, 0), 30.0);
    EXPECT_DOUBLE_EQ(risk.HessianProducts()(0, 1), 10.0);
    EXPECT_EQ(risk.Execution().safePathActivations_, 0U);
}

TEST(AADSecondOrderTest, PreservesTermsThroughCancellationAndExtremeAdjoints) {
    for (double c : {-1e16, 1e308}) {
        const Vector_<> point{0.5, -c};
        Matrix_<> seed(1, 2, 0.5);
        seed(0, 1) = c > 0 ? c : 1.0;
        for (bool reversed : {false, true}) {
            for (bool materialized : {false, true}) {
                const auto risk = EvaluateSecondOrder(
                    [&](const auto& x) -> SecondOrderNumber_ {
                        if (materialized) {
                            const SecondOrderNumber_ first = x[0] * x[0] * x[1], second = c * x[0] * x[0];
                            if (reversed)
                                return second + first;
                            return first + second;
                        }
                        if (reversed)
                            return c * x[0] * x[0] + x[0] * x[0] * x[1];
                        return x[0] * x[0] * x[1] + c * x[0] * x[0];
                    },
                    point, seed);
                EXPECT_EQ(risk.Value(), 0.0);
                EXPECT_EQ(risk.Gradient(), (Vector_<>{0.0, 0.25}));
                EXPECT_DOUBLE_EQ(risk.DirectionalDerivatives()[0], 0.25 * seed(0, 1));
                EXPECT_DOUBLE_EQ(risk.HessianProducts()(0, 0), seed(0, 1));
                EXPECT_DOUBLE_EQ(risk.HessianProducts()(0, 1), 0.5);
                EXPECT_EQ(risk.Execution().safePathActivations_, 1U);
                EXPECT_LE(risk.Execution().recordings_, 2U);
            }
        }
    }
    const auto cancelled =
        EvaluateSecondOrder([](const auto& x) -> SecondOrderNumber_ { return (1e16 * x[0] + x[0]) - 1e16 * x[0]; }, {1.0}, Matrix_<>(1, 1, 1.0));
    EXPECT_DOUBLE_EQ(cancelled.Gradient()[0], 1.0);
    EXPECT_DOUBLE_EQ(cancelled.DirectionalDerivatives()[0], 1.0);
}

TEST(AADSecondOrderTest, PreservesRepresentableTailAndSubnormalDirection) {
    const auto tail = EvaluateSecondOrder([](const auto& x) -> SecondOrderNumber_ { return erfc(x[0]); }, {28.0}, Matrix_<>(1, 1, 1e308));
    EXPECT_EQ(tail.Value(), 0.0);
    EXPECT_EQ(tail.Gradient()[0], 0.0);
    EXPECT_NEAR(tail.DirectionalDerivatives()[0], -3.677741895927334e-33, 3.678e-45);
    EXPECT_NEAR(tail.HessianProducts()(0, 0), 2.059535461719307e-31, 2.060e-43);
    EXPECT_EQ(tail.Execution().safePathActivations_, 1U);
    EXPECT_STREQ(SecondOrderDetail::LastSecondOrderProtection(), "exponential underflow");
    const double seed = std::numeric_limits<double>::denorm_min();
    const auto subnormal = EvaluateSecondOrder(square, {1.0}, Matrix_<>(1, 1, seed));
    EXPECT_EQ(subnormal.DirectionalDerivatives()[0], 2.0 * seed);
    EXPECT_EQ(subnormal.HessianProducts()(0, 0), 2.0 * seed);
}

TEST(AADSecondOrderTest, CountsForwardAndReverseProtectionAndBudgetsProtectedScratch) {
    const auto forward =
        EvaluateSecondOrder([](const auto& x) -> SecondOrderNumber_ { return x[0] * 1e8 + x[1] * 1e8; }, {1.0, 1.0}, Matrix_<>(1, 2, 1e8));
    EXPECT_DOUBLE_EQ(forward.DirectionalDerivatives()[0], 2e16);
    EXPECT_EQ(forward.Execution().safePathActivations_, 1U);
    EXPECT_EQ(forward.Execution().recordings_, 2U);
    EXPECT_EQ(forward.Execution().reverseSweeps_, 1U);

    const SecondOrderFunction_ reversePrice = [](const auto& x) -> SecondOrderNumber_ {
        const SecondOrderNumber_ square = x[0] * x[0];
        const SecondOrderNumber_ scaled = square * 1e8;
        return scaled * 1e8;
    };
    const auto reverse = [&](std::optional<size_t> cap) {
        return std::async(std::launch::async, [&, cap] { return EvaluateSecondOrder(reversePrice, {0.0}, Matrix_<>(1, 1, 1e-20), {{}, cap}); }).get();
    };
    const auto protectedRisk = reverse({});
    EXPECT_DOUBLE_EQ(protectedRisk.HessianProducts()(0, 0), 2e-4);
    EXPECT_EQ(protectedRisk.Execution().safePathActivations_, 1U);
    EXPECT_EQ(protectedRisk.Execution().recordings_, 2U);
    EXPECT_EQ(protectedRisk.Execution().reverseSweeps_, 2U);
    const size_t exact = protectedRisk.Execution().peakTapeBytes_;
    EXPECT_DOUBLE_EQ(reverse(exact).HessianProducts()(0, 0), 2e-4);
    CheckError([&] { (void)reverse(exact - 1); }, Reason::BUDGET_EXCEEDED, Stage::REVERSE, 0);
}

TEST(AADSecondOrderTest, ValidatesRequestedZeroDirectionsAndTrueOverflow) {
    const SecondOrderFunction_ logPrice = [](const auto& x) -> SecondOrderNumber_ { return log(x[0]); };
    const auto zero = EvaluateSecondOrder(logPrice, {1e-200}, Matrix_<>(1, 1, -0.0));
    EXPECT_DOUBLE_EQ(zero.Value(), std::log(1e-200));
    EXPECT_DOUBLE_EQ(zero.Gradient()[0], 1e200);
    EXPECT_EQ(zero.DirectionalDerivatives()[0], 0.0);
    EXPECT_EQ(zero.HessianProducts()(0, 0), 0.0);
    EXPECT_FALSE(std::signbit(zero.DirectionalDerivatives()[0]));
    EXPECT_FALSE(std::signbit(zero.HessianProducts()(0, 0)));
    EXPECT_TRUE(std::signbit(zero.Directions()(0, 0)));
    CheckError([&] { (void)EvaluateSecondOrder(logPrice, {1e-200}, Matrix_<>(1, 1, 1.0)); }, Reason::NUMERICAL_FAILURE, Stage::RESULT, 0);
    const auto gradient = EvaluateSecondOrder(square, {-0.0}, Matrix_<>(0, 1));
    EXPECT_TRUE(std::signbit(gradient.Point()[0]));
    EXPECT_DOUBLE_EQ(gradient.Value(), 0.0);
    EXPECT_EQ(gradient.HessianProducts().Rows(), 0);
    EXPECT_EQ(gradient.HessianProducts().Cols(), 1);
    EXPECT_EQ(gradient.Execution().recordings_, 1U);
    EXPECT_EQ(gradient.Execution().reverseSweeps_, 1U);
}

TEST(AADSecondOrderTest, ValidatesDomainsAndKeepsPowerAndNormalOperationsDifferentiable) {
    const auto fixed = EvaluateSecondOrder([](const auto& x) -> SecondOrderNumber_ { return pow(x[0], 3.0); }, {-2.0}, Matrix_<>(1, 1, 2.0));
    EXPECT_DOUBLE_EQ(fixed.Value(), -8.0);
    EXPECT_DOUBLE_EQ(fixed.Gradient()[0], 12.0);
    EXPECT_DOUBLE_EQ(fixed.HessianProducts()(0, 0), -24.0);
    const auto zero = EvaluateSecondOrder([](const auto& x) -> SecondOrderNumber_ { return pow(x[0], 2.0); }, {0.0}, Matrix_<>(1, 1, 1.0));
    EXPECT_DOUBLE_EQ(zero.HessianProducts()(0, 0), 2.0);
    const auto variable = EvaluateSecondOrder([](const auto& x) -> SecondOrderNumber_ { return pow(x[0], x[1]); }, {2.0, 3.0}, Matrix_<>(1, 2, 1.0));
    const double logTwo = std::log(2.0);
    const double cross = 4.0 + 12.0 * logTwo;
    EXPECT_DOUBLE_EQ(variable.Gradient()[1], 8.0 * logTwo);
    EXPECT_DOUBLE_EQ(variable.HessianProducts()(0, 0), 12.0 + cross);
    EXPECT_DOUBLE_EQ(variable.HessianProducts()(0, 1), cross + 8.0 * logTwo * logTwo);
    for (double z : {std::nextafter(-10.0, -11.0), -10.0, std::nextafter(-10.0, 0.0), 10.0}) {
        const auto risk = EvaluateSecondOrder([](const auto& x) -> SecondOrderNumber_ { return NCDF(x[0]) + NPDF(x[0]); }, {z}, Matrix_<>(1, 1, 1.0));
        const double pdf = std::exp(-0.5 * z * z) / std::sqrt(2.0 * std::acos(-1.0));
        EXPECT_NEAR(risk.Gradient()[0], (1.0 - z) * pdf, 1e-13 * pdf);
        EXPECT_NEAR(risk.HessianProducts()(0, 0), (z * z - z - 1.0) * pdf, 1e-12 * pdf);
    }
    CheckError([] { (void)EvaluateSecondOrder([](const auto& x) -> SecondOrderNumber_ { return sqrt(x[0]); }, {0.0}, Matrix_<>(1, 1, 0.0)); },
               Reason::OUTSIDE_DOMAIN, Stage::RECORDING, 0);
}

TEST(AADSecondOrderTest, SeparatesBudgetAdmissionAndGrowthOverlap) {
    const auto longPrice = [](const auto& x) -> SecondOrderNumber_ {
        auto sum = x[0];
        for (int i = 0; i < 12000; ++i)
            sum += x[0] * 0.01;
        return sum;
    };
    const Matrix_<> direction(1, 1, 1.0);
    const auto cold = [&](const SecondOrderOptions_& options) {
        return std::async(std::launch::async,
                          [&, options] {
                              EXPECT_EQ(NativeThread::retainedTape, nullptr);
                              auto value = EvaluateSecondOrder(longPrice, {1.0}, direction, options);
                              EXPECT_EQ(NativeThread::retainedTape, nullptr);
                              return value;
                          })
            .get();
    };
    const auto first = cold({});
    EXPECT_EQ(first.Execution().numericPayloadBytes_, SecondOrderPayloadBytes(1, 1));
    EXPECT_GT(first.Execution().peakTapeBytes_, first.Execution().retainedTapeBytes_);
    const auto exact = cold({first.Execution().numericPayloadBytes_, first.Execution().peakTapeBytes_});
    EXPECT_DOUBLE_EQ(exact.Value(), first.Value());
    CheckError([&] { (void)cold({{}, first.Execution().peakTapeBytes_ - 1}); }, Reason::BUDGET_EXCEEDED, Stage::RECORDING, 0);
    CheckError([&] { (void)cold({first.Execution().numericPayloadBytes_ - 1, {}}); }, Reason::BUDGET_EXCEEDED, Stage::VALIDATION);
    CheckError([&] { (void)cold({{}, 0}); }, Reason::BUDGET_EXCEEDED, Stage::RECORDING, 0);
    CheckError([] { (void)SecondOrderPayloadBytes(0, 0); }, Reason::INVALID_INPUT, Stage::VALIDATION);
    CheckError([] { (void)EvaluateSecondOrder(square, {1.0}, Matrix_<>()); }, Reason::INVALID_INPUT, Stage::VALIDATION);
    const auto warm = EvaluateSecondOrder(longPrice, {1.0}, direction);
    EXPECT_GT(SecondOrderDetail::LastSecondOrderLogicalPeak(), 64U * 1024U);
    EXPECT_LE(warm.Execution().retainedTapeBytes_, 8U * 1024U * 1024U);
    const auto tiny = EvaluateSecondOrder(square, {1.0}, direction);
    EXPECT_LE(SecondOrderDetail::LastSecondOrderLogicalPeak(), 64U * 1024U);
    EXPECT_LE(tiny.Execution().retainedTapeBytes_, 256U * 1024U);
    const auto constrained = EvaluateSecondOrder(square, {1.0}, direction, {{}, 4096});
    EXPECT_LE(constrained.Execution().peakTapeBytes_, 4096U);
    EXPECT_LE(constrained.Execution().retainedTapeBytes_, 4096U);
}

TEST(AADSecondOrderTest, RejectsStaleHandlesBeforeReadingSlots) {
    std::optional<SecondOrderNumber_> old;
    const auto capture = [&](const auto& x) {
        old = x[0];
        return x[0];
    };
    const Matrix_<> seed(1, 1, 1.0);
    std::async(std::launch::async, [&] { (void)EvaluateSecondOrder(capture, {1.0}, seed); }).get();
    const auto rejected = [&] { return EvaluateSecondOrder([&](const auto& x) -> SecondOrderNumber_ { return *old + x[0]; }, {1.0}, seed); };
    CheckError([&] { (void)rejected(); }, Reason::LIFECYCLE, Stage::RECORDING, 0);
    const auto current = EvaluateSecondOrder(
        [&](const auto& x) {
            old = x[0];
            std::async(std::launch::async, [&] { CheckError([&] { (void)rejected(); }, Reason::LIFECYCLE, Stage::RECORDING, 0); }).get();
            return x[0];
        },
        {2.0}, seed);
    EXPECT_DOUBLE_EQ(current.Value(), 2.0);
    old.reset();
    CheckError(
        [&] {
            (void)EvaluateSecondOrder(
                [&](const auto& x) -> SecondOrderNumber_ {
                    if (old)
                        return *old + x[0];
                    return capture(x);
                },
                {1.0}, Matrix_<>(2, 1, 1.0));
        },
        Reason::LIFECYCLE, Stage::RECORDING, 1);
    EXPECT_DOUBLE_EQ(EvaluateSecondOrder(square, {3.0}, seed).Value(), 9.0);
}

TEST(AADSecondOrderTest, PreservesLegacyGraphAndMutuallyExcludesRecording) {
    Clear(*Tape());
    Number_ legacy = 2.0;
    PutOnTape(legacy);
    const auto legacySize = Tape()->nodes_.OccupiedSlots();
    CheckError([] { (void)EvaluateSecondOrder(square, {1.0}, Matrix_<>(0, 1)); }, Reason::LIFECYCLE, Stage::VALIDATION);
    EXPECT_EQ(Tape()->nodes_.OccupiedSlots(), legacySize);
    Clear(*Tape());
    RecordingScope_ first;
    Number_ x;
    first.RegisterInput(x, 2.0);
    first.StartRecording();
    Number_ price = x * x;
    const Matrix_<> seed(1, 1, 1.0);
    CheckError([&] { (void)EvaluateSecondOrder(square, {1.0}, seed); }, Reason::LIFECYCLE, Stage::VALIDATION);
    first.FinishRecording();
    Adjoint(price) = 1.0;
    first.Reverse();
    EXPECT_DOUBLE_EQ(AdjointValue(x), 4.0);
    first.Close();
    const auto result = EvaluateSecondOrder(
        [&](const auto& arguments) -> SecondOrderNumber_ {
            CheckError([&] { (void)EvaluateSecondOrder(square, {1.0}, seed); }, Reason::LIFECYCLE, Stage::VALIDATION);
            EXPECT_THROW(RecordingScope_ illegal, Exception_);
            return arguments[0] * arguments[0];
        },
        {2.0}, seed);
    EXPECT_DOUBLE_EQ(result.Gradient()[0], 4.0);
}

TEST(AADSecondOrderTest, AppliesTheSmallRetentionTierAtTheExactLogicalBoundary) {
    std::async(std::launch::async, [] {
        const auto evaluate = [](int nodes) {
            return EvaluateSecondOrder(
                [=](const auto& x) {
                    auto sum = x[0];
                    for (int i = 0; i < nodes; ++i)
                        sum += x[1];
                    return SecondOrderNumber_(sum + 1.0);
                },
                {1.0, 0.0}, Matrix_<>(1, 2, 1.0));
        };
        const auto large = evaluate(10000);
        ASSERT_GT(large.Execution().retainedTapeBytes_, 256U * 1024U);
        // Two input slots, one single-edge statement, and 1090 two-edge
        // statements account for exactly 64 KiB with implicit output slots.
        const auto above = evaluate(1091);
        EXPECT_GT(SecondOrderDetail::LastSecondOrderLogicalPeak(), 64U * 1024U);
        EXPECT_EQ(above.Execution().retainedTapeBytes_, large.Execution().retainedTapeBytes_);
        const auto boundary = evaluate(1090);
        EXPECT_EQ(SecondOrderDetail::LastSecondOrderLogicalPeak(), 64U * 1024U);
        EXPECT_EQ(boundary.Execution().retainedTapeBytes_, 0U);
        EXPECT_DOUBLE_EQ(evaluate(10000).Value(), 2.0);
    }).get();
}

TEST(AADSecondOrderTest, PreservesCallbackExceptionsWhenCleanupAlsoFails) {
    struct UserFailure_ {};
    const Matrix_<> seed(1, 1, 1.0);
    SecondOrderDetail::SetSecondOrderCleanupFailureForTest(true);
    CheckError([&] { (void)EvaluateSecondOrder(square, {1.0}, seed); }, Reason::LIFECYCLE, Stage::CLEANUP);
    EXPECT_THROW((void)EvaluateSecondOrder([](const auto&) -> SecondOrderNumber_ { throw UserFailure_{}; }, {1.0}, seed), UserFailure_);
    EXPECT_THROW((void)EvaluateSecondOrder([](const auto&) -> SecondOrderNumber_ { throw std::bad_alloc{}; }, {1.0}, seed), std::bad_alloc);
    SecondOrderDetail::SetSecondOrderCleanupFailureForTest(false);
    EXPECT_DOUBLE_EQ(EvaluateSecondOrder(square, {2.0}, seed).Value(), 4.0);
}

TEST(AADSecondOrderTest, ExclusiveDuringCallableCopyAndDestructionAndResultFailure) {
    struct Reenter_ {
        std::function<void()> check_;
        bool copied_ = false;

        explicit Reenter_(std::function<void()> check) : check_(std::move(check)) {}
        Reenter_(const Reenter_& source) : check_(source.check_), copied_(true) { check_(); }
        Reenter_(Reenter_&&) = default;
        ~Reenter_() {
            if (copied_ && check_)
                check_();
        }
        SecondOrderNumber_ operator()(const Vector_<SecondOrderNumber_>& x) const { return x[0]; }
    };

    const Matrix_<> seed(1, 1, 1.0);
    size_t nested = 0;
    const SecondOrderFunction_ price = Reenter_([&] {
        ++nested;
        CheckError([&] { (void)EvaluateSecondOrder(square, {1.0}, seed); }, Reason::LIFECYCLE, Stage::VALIDATION);
        EXPECT_THROW(RecordingScope_ illegal, Exception_);
    });
    EXPECT_DOUBLE_EQ(EvaluateSecondOrder(price, {2.0}, seed).Value(), 2.0);
    EXPECT_EQ(nested, 2U);
    SecondOrderDetail::SetSecondOrderResultFailureForTest(true);
    EXPECT_THROW((void)EvaluateSecondOrder(price, {2.0}, seed), std::bad_alloc);
    SecondOrderDetail::SetSecondOrderResultFailureForTest(false);
    EXPECT_EQ(nested, 4U);
    EXPECT_DOUBLE_EQ(EvaluateSecondOrder(price, {3.0}, seed).Value(), 3.0);
}

TEST(AADSecondOrderTest, RejectsAHandleAfterPoisonRecoveryWithSlotAndGenerationReuse) {
    std::async(std::launch::async, [] {
        const Matrix_<> seed(1, 1, 1.0);
        SecondOrderNumber_ stale;
        const SecondOrderFunction_ savesInput = [&](const auto& x) {
            stale = x[0];
            return x[0];
        };
        SecondOrderDetail::SetSecondOrderCleanupFailureForTest(true);
        CheckError([&] { (void)EvaluateSecondOrder(savesInput, {1.0}, seed); }, Reason::LIFECYCLE, Stage::CLEANUP);
        SecondOrderDetail::SetSecondOrderCleanupFailureForTest(false);
        const SecondOrderFunction_ readsStale = [&](const auto& x) -> SecondOrderNumber_ { return stale + x[0]; };
        CheckError([&] { (void)EvaluateSecondOrder(readsStale, {1.0}, seed); }, Reason::LIFECYCLE, Stage::RECORDING, 0);
        EXPECT_DOUBLE_EQ(EvaluateSecondOrder(square, {2.0}, seed).Value(), 4.0);
    }).get();
}

TEST(AADSecondOrderTest, BatchAppendPreservesEdgeOrderAndEarlierRootAcrossReplay) {
    Matrix_<> directions(2, 2);
    directions(0, 0) = 1.0;
    directions(0, 1) = -2.0;
    directions(1, 0) = -2.0;
    directions(1, 1) = 1.0;

    for (bool forceProtection : {false, true}) {
        const SecondOrderFunction_ price = [forceProtection](const auto& x) {
            const SecondOrderNumber_ passive = SecondOrderNumber_(2.0) + 0.0;
            const SecondOrderNumber_ root = (x[0] + x[0]) * (x[0] + passive) + x[1] * (x[0] - passive);
            const SecondOrderNumber_ unused = x[1] * (forceProtection ? 0x1p60 : 4.0);
            (void)unused;
            return root;
        };
        const auto result = EvaluateSecondOrder(price, {2.0, 3.0}, directions);
        EXPECT_DOUBLE_EQ(result.Value(), 16.0);
        EXPECT_EQ(result.Gradient(), (Vector_<>{15.0, 0.0}));
        EXPECT_EQ(result.DirectionalDerivatives(), (Vector_<>{15.0, -30.0}));
        EXPECT_DOUBLE_EQ(result.HessianProducts()(0, 0), 2.0);
        EXPECT_DOUBLE_EQ(result.HessianProducts()(0, 1), 1.0);
        EXPECT_DOUBLE_EQ(result.HessianProducts()(1, 0), -7.0);
        EXPECT_DOUBLE_EQ(result.HessianProducts()(1, 1), -2.0);
        EXPECT_EQ(result.Execution().callbackEvaluations_, forceProtection ? 4U : 2U);
        EXPECT_EQ(result.Execution().recordings_, forceProtection ? 4U : 2U);
        EXPECT_EQ(result.Execution().reverseSweeps_, 2U);
        EXPECT_EQ(result.Execution().safePathActivations_, forceProtection ? 2U : 0U);
    }
}

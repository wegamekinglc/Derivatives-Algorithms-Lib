#include <gtest/gtest.h>

#include <cmath>
#include <future>
#include <limits>

#include <dal/math/aad/recording.hpp>
#include <dal/math/aad/sharedsecondorder.hpp>

using namespace Dal;
using namespace Dal::AAD;

namespace {
    using Reason = SecondOrderErrorReason_::Value_;
    using Stage = SecondOrderStage_::Value_;

    template <class F_> void CheckError(const F_& function, Reason reason, Stage stage, std::optional<size_t> row = {}) {
        try {
            function();
            FAIL() << "expected a second-order error";
        } catch (const SecondOrderError_& err) {
            EXPECT_EQ(err.Reason().Switch(), reason);
            EXPECT_EQ(err.Stage().Switch(), stage);
            EXPECT_EQ(err.DirectionIndex(), row);
        }
    }

    const auto square = [](const auto& x) { return x[0] * x[0]; };
    const auto largeGraph = [](const auto& x) {
        using Number = std::decay_t<decltype(x[0])>;
        Number sum = x[0];
        for (int i = 0; i < 400; ++i)
            sum += x[0] * x[0];
        return sum;
    };

    struct CleanupFlags_ {
        CleanupFlags_(bool shared, bool pointwise) {
            SharedSecondOrderDetail::SetCleanupFailureForTest(shared);
            SecondOrderDetail::SetSecondOrderCleanupFailureForTest(pointwise);
        }
        ~CleanupFlags_() {
            SharedSecondOrderDetail::SetCleanupFailureForTest(false);
            SecondOrderDetail::SetSecondOrderCleanupFailureForTest(false);
        }
    };

    template <class A_, class B_> void SameRisk(const A_& a, const B_& b) {
        ASSERT_EQ(a.Value(), b.Value());
        ASSERT_EQ(a.Gradient(), b.Gradient());
        ASSERT_EQ(a.DirectionalDerivatives(), b.DirectionalDerivatives());
        for (int row = 0; row < a.Directions().Rows(); ++row)
            for (int col = 0; col < a.Directions().Cols(); ++col)
                EXPECT_EQ(a.HessianProducts()(row, col), b.HessianProducts()(row, col));
    }

    struct KernelLifetime_ {
        int* copies_;
        int* destructions_;
        Vector_<>* sourcePoint_;
        bool owned_ = false;

        KernelLifetime_(int* copies, int* destructions, Vector_<>* point)
            : copies_(copies), destructions_(destructions), sourcePoint_(point) {}

        KernelLifetime_(const KernelLifetime_& source)
            : copies_(source.copies_), destructions_(source.destructions_), sourcePoint_(source.sourcePoint_), owned_(true) {
            ++*copies_;
            (*sourcePoint_)[0] = 100.0;
            RejectNested();
        }

        ~KernelLifetime_() {
            if (owned_) {
                ++*destructions_;
                RejectNested();
            }
        }

        static void RejectNested() {
            const Matrix_<> seed(3, 1, 1.0);
            CheckError([&] { (void)EvaluateSharedSecondOrder(square, {2.0}, seed); }, Reason::LIFECYCLE, Stage::VALIDATION);
            CheckError([&] { (void)EvaluateSecondOrder(square, {2.0}, seed); }, Reason::LIFECYCLE, Stage::VALIDATION);
        }

        template <class T_> T_ operator()(const Vector_<T_>& x) const { return x[0] * 0x1p56; }
    };
} // namespace

TEST(AADSharedSecondOrderTest, TestOwningRequestSharesOneGraphAcrossDirectionBlocks) {
    Vector_<> point{2.0, 3.0};
    Matrix_<> directions(4, 2, 0.0);
    directions(0, 0) = 1.0;
    directions(1, 1) = -1.0;
    directions(2, 0) = 2.0;
    directions(2, 1) = 3.0;
    directions(3, 1) = -0.0;
    const auto kernel = [](const auto& x) { return x[0] * x[0] * x[1]; };
    const auto risk = EvaluateSharedSecondOrder(kernel, point, directions);
    point.Fill(0.0);
    directions.Fill(0.0);

    ASSERT_DOUBLE_EQ(risk.Value(), 12.0);
    ASSERT_EQ(risk.Point(), (Vector_<>{2.0, 3.0}));
    ASSERT_EQ(risk.Gradient(), (Vector_<>{12.0, 4.0}));
    ASSERT_EQ(risk.DirectionalDerivatives(), (Vector_<>{12.0, -4.0, 36.0, 0.0}));
    ASSERT_DOUBLE_EQ(risk.HessianProducts()(0, 0), 6.0);
    ASSERT_DOUBLE_EQ(risk.HessianProducts()(0, 1), 4.0);
    ASSERT_DOUBLE_EQ(risk.HessianProducts()(1, 0), -4.0);
    ASSERT_DOUBLE_EQ(risk.HessianProducts()(2, 0), 24.0);
    ASSERT_DOUBLE_EQ(risk.HessianProducts()(2, 1), 8.0);
    ASSERT_TRUE(std::signbit(risk.Directions()(3, 1)));
    ASSERT_FALSE(std::signbit(risk.HessianProducts()(3, 1)));
    ASSERT_EQ(risk.Execution().callbackEvaluations_, 1U);
    ASSERT_EQ(risk.Execution().sharedRecordings_, 1U);
    ASSERT_EQ(risk.Execution().sharedTangentSweeps_, 2U);
    ASSERT_EQ(risk.Execution().sharedReverseSweeps_, 2U);
    ASSERT_EQ(risk.Execution().replayDirections_, 0U);
}

TEST(AADSharedSecondOrderTest, TestReplayStartsOnlyForProtectionAndPreservesPointwiseRetention) {
    std::async(std::launch::async, [] {
        const Vector_<> point{2.0};
        const Matrix_<> regular(6, 1, 1.0);
        const auto nativeCold = EvaluateSecondOrder(largeGraph, point, regular).Execution();
        const auto nativeHot = EvaluateSecondOrder(largeGraph, point, regular).Execution();
        ASSERT_GT(nativeCold.peakTapeBytes_, nativeHot.peakTapeBytes_);
        ASSERT_GT(EvaluateSharedSecondOrder(largeGraph, point, regular).Execution().retainedTapeBytes_, 0U);

        for (int row : {0, 5}) {
            auto forced = regular;
            forced(row, 0) = 0x1p56;
            const auto risk = EvaluateSharedSecondOrder(square, point, forced);
            const auto& work = risk.Execution();
            EXPECT_EQ(work.replayDirections_, 6U);
            EXPECT_EQ(work.sharedTangentSweeps_, row == 0 ? 0U : 1U);
            EXPECT_EQ(work.sharedReverseSweeps_, row == 0 ? 0U : 1U);
            EXPECT_EQ(work.pointwiseReverseSweeps_, 6U);
        EXPECT_EQ(work.pointwiseSafePathActivations_, 6U);
            EXPECT_EQ(work.callbackEvaluations_, work.pointwiseRecordings_ + 1);
            EXPECT_EQ(work.retainedTapeBytes_, 0U);
            EXPECT_DOUBLE_EQ(risk.HessianProducts()(row, 0), 0x1p57);
            EXPECT_DOUBLE_EQ(risk.HessianProducts()((row + 1) % 6, 0), 2.0);
            EXPECT_EQ(EvaluateSecondOrder(largeGraph, point, regular).Execution().peakTapeBytes_, nativeHot.peakTapeBytes_);
        }

        const auto forceRecording = [](const auto& x) { return x[0] * 0x1p56; };
        const auto graphReplay = EvaluateSharedSecondOrder(forceRecording, point, regular);
        EXPECT_EQ(graphReplay.Execution().sharedRecordings_, 1U);
        EXPECT_EQ(graphReplay.Execution().sharedTangentSweeps_, 0U);
        EXPECT_EQ(graphReplay.Execution().callbackEvaluations_, 13U);
        EXPECT_EQ(graphReplay.Execution().pointwiseSafePathActivations_, 6U);
        EXPECT_DOUBLE_EQ(graphReplay.Gradient()[0], 0x1p56);
        CheckError([&] { (void)EvaluateSharedSecondOrder(forceRecording, point, regular, {{}, 512}); },
                   Reason::BUDGET_EXCEEDED, Stage::RECORDING, 0);
        EXPECT_EQ(EvaluateSecondOrder(largeGraph, point, regular).Execution().peakTapeBytes_, nativeHot.peakTapeBytes_);
    }).get();
}

TEST(AADSharedSecondOrderTest, TestHotInputAdmissionAndExactColdAndHotCaps) {
    std::async(std::launch::async, [] {
        const Vector_<> point{2.0};
        const Matrix_<> seed(3, 1, 1.0);
        const auto payload = SecondOrderPayloadBytes(point.size(), seed.Rows());
        const auto cold = EvaluateSharedSecondOrder(largeGraph, point, seed, {payload, {}}).Execution().peakTapeBytes_;
        const auto hot = EvaluateSharedSecondOrder(largeGraph, point, seed).Execution().peakTapeBytes_;
        ASSERT_EQ(hot, cold + sizeof(SharedSecondOrderNumber_));
        EXPECT_EQ(EvaluateSharedSecondOrder(largeGraph, point, seed, {{}, hot}).Execution().peakTapeBytes_, hot);
        EXPECT_EQ(EvaluateSharedSecondOrder(largeGraph, point, seed, {{}, hot - 1}).Execution().peakTapeBytes_, cold);
        EXPECT_EQ(EvaluateSharedSecondOrder(largeGraph, point, seed, {{}, cold}).Execution().peakTapeBytes_, cold);
        CheckError([&] { (void)EvaluateSharedSecondOrder(largeGraph, point, seed, {{}, cold - 1}); },
                   Reason::BUDGET_EXCEEDED, Stage::REVERSE);
        CheckError([&] { (void)EvaluateSharedSecondOrder(largeGraph, point, seed, {payload - 1, {}}); },
                   Reason::BUDGET_EXCEEDED, Stage::VALIDATION);
        CheckError([&] { (void)EvaluateSharedSecondOrder(square, point, Matrix_<>(1, 1, 1.0)); },
                   Reason::INVALID_INPUT, Stage::VALIDATION);
    }).get();
}

TEST(AADSharedSecondOrderTest, TestStickyDomainAndHandleErrorsDoNotReplay) {
    const Matrix_<> seed(3, 1, 1.0);
    const auto caughtDomain = [](const auto& x) {
        using Number = std::decay_t<decltype(x[0])>;
        try {
            const Number unused = log(x[0]);
            (void)unused;
        } catch (const SecondOrderError_&) {
        }
        return x[0];
    };
    CheckError([&] { (void)EvaluateSharedSecondOrder(caughtDomain, {-1.0}, seed); }, Reason::OUTSIDE_DOMAIN, Stage::RECORDING);
    const auto caughtGrowth = [](const auto& x) {
        using Number = std::decay_t<decltype(x[0])>;
        try {
            const Number unused = x[0] * x[0];
            (void)unused;
        } catch (const SecondOrderError_&) {
        }
        return x[0];
    };
    CheckError([&] { (void)EvaluateSharedSecondOrder(caughtGrowth, {2.0}, seed, {{}, 64}); }, Reason::BUDGET_EXCEEDED, Stage::RECORDING);
    std::optional<SharedSecondOrderNumber_> previous;
    const auto capture = [&](const auto& x) {
        using Number = std::decay_t<decltype(x[0])>;
        if constexpr (std::is_same_v<Number, SharedSecondOrderNumber_>) {
            if (previous)
                return Number(*previous + x[0]);
            previous = x[0];
        }
        return x[0];
    };
    ASSERT_DOUBLE_EQ(EvaluateSharedSecondOrder(capture, {2.0}, seed).Value(), 2.0);
    const auto reject = [&] {
        CheckError([&] { (void)EvaluateSharedSecondOrder(capture, {2.0}, seed); }, Reason::LIFECYCLE, Stage::RECORDING);
    };
    reject();
    std::async(std::launch::async, reject).get();
    ASSERT_DOUBLE_EQ(EvaluateSharedSecondOrder(square, {2.0}, seed).Value(), 4.0);
}

TEST(AADSharedSecondOrderTest, TestOwnedKernelCopyAndDestructionStayInsideExclusion) {
    Vector_<> point{2.0};
    const Matrix_<> seed(3, 1, 1.0);
    int copies = 0, destructions = 0;
    const KernelLifetime_ kernel(&copies, &destructions, &point);
    const auto risk = EvaluateSharedSecondOrder(kernel, point, seed);
    ASSERT_EQ(copies, 1);
    ASSERT_EQ(destructions, 1);
    ASSERT_EQ(point[0], 100.0);
    ASSERT_EQ(risk.Point()[0], 2.0);
    ASSERT_DOUBLE_EQ(risk.Value(), 0x1p57);
    ASSERT_EQ(risk.Execution().replayDirections_, 3U);
    ASSERT_EQ(risk.Execution().callbackEvaluations_, 7U);
}

TEST(AADSharedSecondOrderTest, TestCleanupPreservesPrimaryExceptionAndRecoversBothRoutes) {
    struct UserFailure_ {};
    const Matrix_<> seed(3, 1, 1.0);
    const auto fail = [](const auto& x) {
        throw UserFailure_{};
        return x[0];
    };
    const auto replay = [](const auto& x) { return x[0] * 0x1p56; };
    {
        const CleanupFlags_ flags(true, true);
        CheckError([&] { (void)EvaluateSharedSecondOrder(square, {2.0}, seed); }, Reason::LIFECYCLE, Stage::CLEANUP);
        EXPECT_THROW((void)EvaluateSharedSecondOrder(fail, {2.0}, seed), UserFailure_);
        CheckError([&] { (void)EvaluateSharedSecondOrder(replay, {2.0}, seed); }, Reason::LIFECYCLE, Stage::CLEANUP);
        CheckError([&] { (void)EvaluateSharedSecondOrder(replay, {2.0}, seed, {{}, 512}); },
                   Reason::BUDGET_EXCEEDED, Stage::RECORDING, 0);
    }
    ASSERT_DOUBLE_EQ(EvaluateSharedSecondOrder(square, {2.0}, seed).Value(), 4.0);
    ASSERT_DOUBLE_EQ(EvaluateSecondOrder(square, {2.0}, seed).Value(), 4.0);
}

TEST(AADSharedSecondOrderTest, TestTailCancellationAndTrueOverflowMatchPointwise) {
    const Matrix_<> seed(3, 1, 1.0);
    const auto tail = [](const auto& x) { return erfc(x[0]) * 1e308; };
    SameRisk(EvaluateSharedSecondOrder(tail, {28.0}, seed), EvaluateSecondOrder(tail, {28.0}, seed));
    const auto cancellation = [](const auto& x) { return (x[0] + 1e16) - 1e16; };
    SameRisk(EvaluateSharedSecondOrder(cancellation, {0.5}, seed), EvaluateSecondOrder(cancellation, {0.5}, seed));
    const auto overflow = [](const auto& x) { return x[0] * 1e308; };
    CheckError([&] { (void)EvaluateSharedSecondOrder(overflow, {2.0}, seed); }, Reason::NUMERICAL_FAILURE, Stage::RECORDING, 0);
}

TEST(AADSharedSecondOrderTest, TestReplayPreservesProductResidualBeforeCancellation) {
    const Vector_<> point{0x1p-6, 0.0};
    const auto kernel = [](const auto& x) { return (log(x[0]) - (0x1p27 + 1.0) * x[1]) * 0x1p-12; };
    for (int scale : {0, -15}) {
        Matrix_<> directions(2, 2, 0.0);
        directions(0, 0) = std::ldexp(0x1p48, scale);
        directions(0, 1) = std::ldexp(0x1p27 - 1.0, scale);
        directions(1, 1) = -0.0;
        const auto result = EvaluateSharedSecondOrder(kernel, point, directions);
        EXPECT_DOUBLE_EQ(result.Value(), std::log(point[0]) * 0x1p-12);
        EXPECT_EQ(result.Gradient(), (Vector_<>{0x1p-6, -(0x1p27 + 1.0) * 0x1p-12}));
        EXPECT_EQ(result.DirectionalDerivatives(), (Vector_<>{std::ldexp(0x1p-12, scale), 0.0}));
        EXPECT_DOUBLE_EQ(result.HessianProducts()(0, 0), -directions(0, 0));
        EXPECT_DOUBLE_EQ(result.HessianProducts()(0, 1), 0.0);
        EXPECT_DOUBLE_EQ(result.HessianProducts()(1, 0), 0.0);
        EXPECT_DOUBLE_EQ(result.HessianProducts()(1, 1), 0.0);
        EXPECT_TRUE(std::signbit(result.Directions()(1, 1)));
        EXPECT_FALSE(std::signbit(result.DirectionalDerivatives()[1]));

        const auto& work = result.Execution();
        EXPECT_EQ(work.callbackEvaluations_, 4U);
        EXPECT_EQ(work.sharedRecordings_, 1U);
        EXPECT_EQ(work.sharedTangentSweeps_, 1U);
        EXPECT_EQ(work.sharedReverseSweeps_, 0U);
        EXPECT_EQ(work.replayDirections_, 2U);
        EXPECT_EQ(work.pointwiseRecordings_, 3U);
        EXPECT_EQ(work.pointwiseReverseSweeps_, 2U);
        EXPECT_EQ(work.pointwiseSafePathActivations_, 1U);
        EXPECT_EQ(work.retainedTapeBytes_, 0U);
    }
}

TEST(AADSharedSecondOrderTest, TestCancellationReplayUsesResidualAcrossScales) {
    constexpr double factor = 0x1p27 + 1.0;
    const auto check = [&](int k, int pointExponent, int priceExponent, double sign) {
        SCOPED_TRACE(k);
        const Vector_<> point{std::ldexp(1.0, pointExponent), 0.0};
        const double priceScale = std::ldexp(1.0, priceExponent);
        const auto kernel = [=](const auto& x) { return (log(x[0]) - factor * x[1]) * priceScale; };
        Matrix_<> directions(2, 2, 0.0);
        directions(0, 0) = sign * std::ldexp(1.0, k + pointExponent);
        directions(0, 1) = sign * std::ldexp(0x1p27 - 1.0, k - 54);
        directions(1, 1) = -0.0;
        const auto result = EvaluateSharedSecondOrder(kernel, point, directions);
        EXPECT_DOUBLE_EQ(result.Value(), std::log(point[0]) * priceScale);
        EXPECT_EQ(result.Gradient(), (Vector_<>{std::ldexp(priceScale, -pointExponent), -factor * priceScale}));
        EXPECT_EQ(result.DirectionalDerivatives(), (Vector_<>{sign * std::ldexp(priceScale, k - 54), 0.0}));
        EXPECT_DOUBLE_EQ(result.HessianProducts()(0, 0), -sign * std::ldexp(priceScale, k - pointExponent));
        EXPECT_DOUBLE_EQ(result.HessianProducts()(0, 1), 0.0);
        EXPECT_DOUBLE_EQ(result.HessianProducts()(1, 0), 0.0);
        EXPECT_DOUBLE_EQ(result.HessianProducts()(1, 1), 0.0);
        EXPECT_TRUE(std::signbit(result.Directions()(1, 1)));
        EXPECT_FALSE(std::signbit(result.DirectionalDerivatives()[1]));

        const auto& work = result.Execution();
        EXPECT_EQ(work.callbackEvaluations_, 4U);
        EXPECT_EQ(work.sharedRecordings_, 1U);
        EXPECT_EQ(work.sharedTangentSweeps_, 1U);
        EXPECT_EQ(work.sharedReverseSweeps_, 0U);
        EXPECT_EQ(work.replayDirections_, 2U);
        EXPECT_EQ(work.pointwiseRecordings_, 3U);
        EXPECT_EQ(work.pointwiseReverseSweeps_, 2U);
        EXPECT_EQ(work.pointwiseSafePathActivations_, 1U);
        EXPECT_EQ(work.retainedTapeBytes_, 0U);
    };
    for (int k : {27, 28, 29})
        check(k, -24, 24 - k, 1.0);
    for (int k : {-600, -80, 0, 40})
        check(k, 0, 0, -1.0);
}

TEST(AADSharedSecondOrderTest, TestReverseProductPreservesTangentResidual) {
    const Vector_<> point{0x1p-24, 0.0, 0.0};
    const double baseLog = std::log(point[0]);
    const auto kernel = [baseLog](const auto& x) {
        return x[2] * (log(x[0]) - baseLog) - x[2] * ((0x1p27 + 1.0) * x[1]);
    };
    Matrix_<> directions(2, 3, 0.0);
    directions(0, 0) = 0x1p24;
    directions(0, 1) = (0x1p27 - 1.0) * 0x1p-6;
    directions(1, 2) = -0.0;
    const auto result = EvaluateSharedSecondOrder(kernel, point, directions);

    EXPECT_EQ(result.Value(), 0.0);
    EXPECT_EQ(result.Gradient(), (Vector_<>{0.0, 0.0, 0.0}));
    EXPECT_EQ(result.DirectionalDerivatives(), (Vector_<>{0.0, 0.0}));
    EXPECT_EQ(result.HessianProducts()(0, 0), 0.0);
    EXPECT_EQ(result.HessianProducts()(0, 1), 0.0);
    EXPECT_EQ(result.HessianProducts()(0, 2), 0x1p-6);
    EXPECT_FALSE(std::signbit(result.Value()));
    EXPECT_TRUE(std::signbit(result.Directions()(1, 2)));
    EXPECT_FALSE(std::signbit(result.DirectionalDerivatives()[0]));
    EXPECT_FALSE(std::signbit(result.DirectionalDerivatives()[1]));
    for (int col = 0; col < directions.Cols(); ++col) {
        EXPECT_FALSE(std::signbit(result.Gradient()[col]));
        EXPECT_EQ(result.HessianProducts()(1, col), 0.0);
        EXPECT_FALSE(std::signbit(result.HessianProducts()(1, col)));
    }
    EXPECT_FALSE(std::signbit(result.HessianProducts()(0, 0)));
    EXPECT_FALSE(std::signbit(result.HessianProducts()(0, 1)));
    const auto& work = result.Execution();
    EXPECT_EQ(work.callbackEvaluations_, 1U);
    EXPECT_EQ(work.sharedRecordings_, 1U);
    EXPECT_EQ(work.sharedTangentSweeps_, 2U);
    EXPECT_EQ(work.sharedReverseSweeps_, 2U);
    EXPECT_EQ(work.replayDirections_, 0U);
    EXPECT_EQ(work.pointwiseRecordings_, 0U);
    EXPECT_EQ(work.pointwiseReverseSweeps_, 0U);
    EXPECT_EQ(work.pointwiseSafePathActivations_, 0U);
}

TEST(AADSharedSecondOrderTest, TestReverseResidualThroughInternalRepeatedOperand) {
    const Vector_<> point{0x1p-24, 0.0, 0.5};
    const double baseLog = std::log(point[0]);
    const auto kernel = [baseLog](const auto& x) {
        const auto scale = x[2] * x[2] - 0.25;
        return scale * (log(x[0]) - baseLog) - scale * ((0x1p27 + 1.0) * x[1]);
    };
    Matrix_<> directions(2, 3, 0.0);
    directions(0, 0) = 0x1p24 - 0x1p-28;
    directions(0, 1) = (0x1p27 - 1.0) * 0x1p-6;
    const auto result = EvaluateSharedSecondOrder(kernel, point, directions);

    EXPECT_EQ(result.Value(), 0.0);
    EXPECT_EQ(result.Gradient(), (Vector_<>{0.0, 0.0, 0.0}));
    EXPECT_EQ(result.DirectionalDerivatives(), (Vector_<>{0.0, 0.0}));
    EXPECT_EQ(result.HessianProducts()(0, 0), 0.0);
    EXPECT_EQ(result.HessianProducts()(0, 1), 0.0);
    EXPECT_EQ(result.HessianProducts()(0, 2), -3.0 * 0x1p-6);
    for (int col = 0; col < directions.Cols(); ++col) {
        EXPECT_EQ(result.HessianProducts()(1, col), 0.0);
        EXPECT_FALSE(std::signbit(result.HessianProducts()(1, col)));
    }
    const auto& work = result.Execution();
    EXPECT_EQ(work.callbackEvaluations_, 1U);
    EXPECT_EQ(work.sharedRecordings_, 1U);
    EXPECT_EQ(work.sharedTangentSweeps_, 2U);
    EXPECT_EQ(work.sharedReverseSweeps_, 2U);
    EXPECT_EQ(work.replayDirections_, 0U);
    EXPECT_EQ(work.pointwiseRecordings_, 0U);
}

TEST(AADSharedSecondOrderTest, TestCurvatureUsesOrdinaryAdjointResidual) {
    constexpr double spot = 0x1p-24;
    const double baseLog = std::log(spot);
    const auto kernel = [baseLog](const auto& x) {
        using Number = std::decay_t<decltype(x[0])>;
        const Number a = log(x[0]) - baseLog;
        const Number q = a * a;
        const Number l = 0x1p27 * q;
        const Number r = (0x1p27 + 1.0) * q;
        return 0x1p21 * l - ((0x1p27 - 1.0) * 0x1p-6) * r;
    };
    Matrix_<> directions(2, 1);
    directions(0, 0) = 16.0;
    directions(1, 0) = -0.0;
    const auto result = EvaluateSharedSecondOrder(kernel, {spot}, directions);

    EXPECT_EQ(result.Value(), 0.0);
    EXPECT_EQ(result.Gradient(), (Vector_<>{0.0}));
    EXPECT_EQ(result.DirectionalDerivatives(), (Vector_<>{0.0, 0.0}));
    EXPECT_EQ(result.HessianProducts()(0, 0), 0x1p47);
    EXPECT_EQ(result.HessianProducts()(1, 0), 0.0);
    EXPECT_TRUE(std::signbit(result.Directions()(1, 0)));
    EXPECT_FALSE(std::signbit(result.DirectionalDerivatives()[1]));
    EXPECT_FALSE(std::signbit(result.HessianProducts()(1, 0)));
    const auto& work = result.Execution();
    EXPECT_EQ(work.callbackEvaluations_, 1U);
    EXPECT_EQ(work.sharedRecordings_, 1U);
    EXPECT_EQ(work.sharedTangentSweeps_, 2U);
    EXPECT_EQ(work.sharedReverseSweeps_, 2U);
    EXPECT_EQ(work.replayDirections_, 0U);
    EXPECT_EQ(work.pointwiseSafePathActivations_, 0U);
}

TEST(AADSharedSecondOrderTest, TestCurvatureCarriesAdjointLowLimbIntoHvpAccumulation) {
    const auto kernel = [](const auto& x) {
        using Number = std::decay_t<decltype(x[0])>;
        const Number left = x[0] * x[0];
        const Number right = (0x1p27 + 1.0) * Number(x[0] * x[0]);
        return left - ((0x1p27 - 1.0) * 0x1p-54) * right;
    };
    Matrix_<> directions(2, 1);
    directions(0, 0) = 0.5;
    directions(1, 0) = -0.5;
    const auto result = EvaluateSharedSecondOrder(kernel, {0.0}, directions);

    EXPECT_EQ(result.Value(), 0.0);
    EXPECT_EQ(result.Gradient(), (Vector_<>{0.0}));
    EXPECT_EQ(result.DirectionalDerivatives(), (Vector_<>{0.0, 0.0}));
    EXPECT_EQ(result.HessianProducts()(0, 0), 0x1p-54);
    EXPECT_EQ(result.HessianProducts()(1, 0), -0x1p-54);
    EXPECT_EQ(result.Execution().sharedRecordings_, 1U);
    EXPECT_EQ(result.Execution().sharedTangentSweeps_, 2U);
    EXPECT_EQ(result.Execution().sharedReverseSweeps_, 2U);
    EXPECT_EQ(result.Execution().replayDirections_, 0U);
}

TEST(AADSharedSecondOrderTest, TestSeedProtectionCoversEveryNonzeroDirection) {
    const auto kernel = [](const auto& x) { return (x[0] + x[1]) - x[2]; };
    for (int large : {0, 1}) {
        SCOPED_TRACE(large);
        const int small = 1 - large;
        Matrix_<> directions(2, 3, 0.0);
        directions(large, 0) = 0x1p56;
        directions(small, 0) = directions(small, 2) = 1.0;
        directions(small, 1) = 0x1p-54;
        const auto result = EvaluateSharedSecondOrder(kernel, {0.0, 0.0, 0.0}, directions);

        EXPECT_EQ(result.Value(), 0.0);
        EXPECT_EQ(result.Gradient(), (Vector_<>{1.0, 1.0, -1.0}));
        EXPECT_EQ(result.DirectionalDerivatives()[large], 0x1p56);
        EXPECT_EQ(result.DirectionalDerivatives()[small], 0x1p-54);
        for (int row = 0; row < directions.Rows(); ++row)
            for (int col = 0; col < directions.Cols(); ++col) {
                EXPECT_EQ(result.HessianProducts()(row, col), 0.0);
                EXPECT_FALSE(std::signbit(result.HessianProducts()(row, col)));
            }
        const auto& work = result.Execution();
        EXPECT_EQ(work.callbackEvaluations_, 5U);
        EXPECT_EQ(work.sharedRecordings_, 1U);
        EXPECT_EQ(work.sharedTangentSweeps_, static_cast<size_t>(large));
        EXPECT_EQ(work.sharedReverseSweeps_, 0U);
        EXPECT_EQ(work.replayDirections_, 2U);
        EXPECT_EQ(work.pointwiseRecordings_, 4U);
        EXPECT_EQ(work.pointwiseReverseSweeps_, 2U);
        EXPECT_EQ(work.pointwiseSafePathActivations_, 2U);
    }
}

TEST(AADSharedSecondOrderTest, TestReplayGradientIsIndependentOfZeroDirectionPosition) {
    const auto kernel = [](const auto& x) {
        using Number = std::decay_t<decltype(x[0])>;
        const Number l = 0x1p27 * x[0];
        const Number r = (0x1p27 + 1.0) * x[0];
        return 0x1p21 * l - ((0x1p27 - 1.0) * 0x1p-6) * r;
    };
    for (int zero : {0, 1}) {
        SCOPED_TRACE(zero);
        Matrix_<> directions(2, 1);
        directions(zero, 0) = -0.0;
        directions(1 - zero, 0) = 0x1p56;
        const auto result = EvaluateSharedSecondOrder(kernel, {0.0}, directions);

        EXPECT_EQ(result.Value(), 0.0);
        EXPECT_EQ(result.Gradient(), (Vector_<>{0x1p-6}));
        EXPECT_EQ(result.DirectionalDerivatives()[1 - zero], 0x1p50);
        EXPECT_EQ(result.DirectionalDerivatives()[zero], 0.0);
        EXPECT_EQ(result.HessianProducts()(0, 0), 0.0);
        EXPECT_EQ(result.HessianProducts()(1, 0), 0.0);
        EXPECT_TRUE(std::signbit(result.Directions()(zero, 0)));
        EXPECT_FALSE(std::signbit(result.DirectionalDerivatives()[zero]));
        EXPECT_FALSE(std::signbit(result.HessianProducts()(zero, 0)));
        const auto& work = result.Execution();
        EXPECT_EQ(work.callbackEvaluations_, 4U);
        EXPECT_EQ(work.sharedRecordings_, 1U);
        EXPECT_EQ(work.sharedTangentSweeps_, static_cast<size_t>(zero == 0));
        EXPECT_EQ(work.sharedReverseSweeps_, static_cast<size_t>(zero == 0));
        EXPECT_EQ(work.replayDirections_, 2U);
        EXPECT_EQ(work.pointwiseRecordings_, 3U);
        EXPECT_EQ(work.pointwiseReverseSweeps_, 2U);
        EXPECT_EQ(work.pointwiseSafePathActivations_, 1U);
    }
}

TEST(AADSharedSecondOrderTest, TestOrdinaryRoundingAndExactCancellationStayShared) {
    constexpr double factor = 0x1p27 + 1.0;
    constexpr double seed = (0x1p27 - 1.0) * 0x1p-54;
    const auto kernel = [factor](const auto& x) { return factor * x[0] - factor * x[1]; };
    Matrix_<> directions(3, 2, 0.0);
    directions(0, 0) = seed;
    directions(1, 0) = directions(1, 1) = seed;
    directions(2, 0) = -seed;
    const auto result = EvaluateSharedSecondOrder(kernel, {0.0, 0.0}, directions);
    EXPECT_DOUBLE_EQ(result.Value(), 0.0);
    EXPECT_EQ(result.Gradient(), (Vector_<>{factor, -factor}));
    EXPECT_EQ(result.DirectionalDerivatives(), (Vector_<>{1.0, 0.0, -1.0}));
    for (int row = 0; row < directions.Rows(); ++row)
        for (int col = 0; col < directions.Cols(); ++col)
            EXPECT_DOUBLE_EQ(result.HessianProducts()(row, col), 0.0);
    const auto& work = result.Execution();
    EXPECT_EQ(work.callbackEvaluations_, 1U);
    EXPECT_EQ(work.sharedRecordings_, 1U);
    EXPECT_EQ(work.sharedTangentSweeps_, 1U);
    EXPECT_EQ(work.sharedReverseSweeps_, 1U);
    EXPECT_EQ(work.replayDirections_, 0U);
    EXPECT_EQ(work.pointwiseRecordings_, 0U);

    const auto difference = [factor](const auto& x) { return x[0] - factor * x[1]; };
    Matrix_<> close(2, 2, 0.0);
    close(0, 0) = 0x1.0000000000001p+0;
    close(0, 1) = seed;
    const auto corrected = EvaluateSharedSecondOrder(difference, {0.0, 0.0}, close);
    EXPECT_EQ(corrected.DirectionalDerivatives(), (Vector_<>{5.0 * 0x1p-54, 0.0}));
    EXPECT_EQ(corrected.Execution().sharedTangentSweeps_, 2U);
    EXPECT_EQ(corrected.Execution().sharedReverseSweeps_, 2U);
    EXPECT_EQ(corrected.Execution().replayDirections_, 0U);
}

TEST(AADSharedSecondOrderTest, TestCaughtRecordingErrorStopsLaterOperationsBeforeTheyAppend) {
    bool attemptedLater = false;
    std::exception_ptr first;
    const auto kernel = [&](const auto& x) {
        using T_ = std::decay_t<decltype(x[0])>;
        const T_ prior = x[0] * x[0];
        if constexpr (std::is_same_v<T_, SharedSecondOrderNumber_>) {
            auto* const tape = SharedSecondOrderDetail::active;
            const auto before = tape->size_;
            EXPECT_EQ(before, 1U);
            try {
                const T_ invalid = log(x[0]);
                (void)invalid;
            } catch (const SecondOrderError_&) {
                first = std::current_exception();
            }
            EXPECT_TRUE(first);
            attemptedLater = true;
            try {
                const T_ later = x[0] * x[0];
                (void)later;
                ADD_FAILURE() << "a failed graph accepted a later operation";
            } catch (const SecondOrderError_&) {
                EXPECT_EQ(std::current_exception(), first);
            }
            EXPECT_EQ(tape->size_, before);
        }
        return prior;
    };
    const Matrix_<> directions(2, 1, 1.0);
    try {
        (void)EvaluateSharedSecondOrder(kernel, {-1.0}, directions);
        ADD_FAILURE() << "a failed graph published its result";
    } catch (const SecondOrderError_& error) {
        EXPECT_EQ(std::current_exception(), first);
        EXPECT_EQ(error.Reason().Switch(), Reason::OUTSIDE_DOMAIN);
        EXPECT_EQ(error.Stage().Switch(), Stage::RECORDING);
    }
    EXPECT_TRUE(attemptedLater);
    EXPECT_DOUBLE_EQ(EvaluateSharedSecondOrder(square, {-1.0}, directions).Value(), 1.0);
}

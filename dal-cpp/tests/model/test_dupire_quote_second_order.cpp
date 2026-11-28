#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cfenv>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>

#if defined(__x86_64__) || defined(_M_X64)
#include <xmmintrin.h>
#endif

#include <dal/model/detail/dupirequoteplan.hpp>
#include <dal/model/detail/dupirethomas.hpp>
#include <dal/model/dupirequotesecondorder.hpp>
#include <dal/model/ivs.hpp>

#include "../../benchmarks/tape_perf/dupirereference.hpp"

using namespace Dal;
using namespace Dal::AAD;

namespace {
    using Reason = SecondOrderErrorReason_::Value_;
    using Stage = SecondOrderStage_::Value_;
    namespace Reference = TapePerfDupire::Reference;

    class FlatBase_ final : public IVS_ {
        bool changed_;

    public:
        explicit FlatBase_(bool changed = false) : IVS_(100.0, 0.05, 0.02), changed_(changed) {}
        double ImpliedVol(double strike, double time) const override {
            return changed_ && strike == 80.0 && time == 0.5 ? std::nextafter(0.20, 1.0) : 0.20;
        }
    };

    DupireCalibrationSnapshot_ Snapshot(bool subtractSpread = false, bool changedBase = false, double middleSpread = 0.0) {
        Matrix_<> spreads(3, 2, 0.0);
        spreads(0, 0) = subtractSpread ? 0.208 - 0.20 : 0.008;
        spreads(0, 1) = 0.012;
        spreads(1, 0) = middleSpread;
        spreads(1, 1) = 0.004;
        spreads(2, 0) = 0.006;
        spreads(2, 1) = 0.009;
        return CalibrateDupireWithRisk(
            FlatBase_(changedBase),
            {{75.0, 105.0, 135.0}, {0.4, 1.2}, spreads, {60.0, 70.0, 80.0, 90.0, 100.0, 110.0, 120.0, 130.0, 140.0}, 10.0, {0.5, 1.0}, 0.5});
    }

    DupireEuropeanCall_ Call() { return {105.0, 1.0, "EQ", Ccy_("USD")}; }

    class RestoreFloatingEnvironment_ {
        std::fenv_t environment_{};
        const int status_ = std::fegetenv(&environment_);
#if defined(__x86_64__) || defined(_M_X64)
        const unsigned mxcsr_ = _mm_getcsr();
#endif

    public:
        ~RestoreFloatingEnvironment_() {
            if (Saved())
                (void)std::fesetenv(&environment_);
#if defined(__x86_64__) || defined(_M_X64)
            _mm_setcsr(mxcsr_);
#endif
        }

        [[nodiscard]] bool Saved() const { return status_ == 0; }
    };

    template <class F_> void CheckValidation(F_&& fn, Reason reason, std::string_view location) {
        try {
            fn();
            FAIL() << "expected typed validation failure";
        } catch (const SecondOrderError_& error) {
            EXPECT_EQ(error.Reason().Switch(), reason);
            EXPECT_EQ(error.Stage().Switch(), Stage::VALIDATION);
            EXPECT_FALSE(error.DirectionIndex());
            EXPECT_NE(std::string(error.what()).find(location), std::string::npos) << error.what();
        }
    }
} // namespace

TEST(DupireQuoteSecondOrderTest, CanonicalizesQuoteZeroButRetainsDirectionZero) {
    const auto positive = Snapshot(false, false, 0.0);
    const auto negative = Snapshot(false, false, -0.0);
    const auto positivePlan = PrepareDupireEuropeanCallQuoteRisk(positive, Call(), {0.8, 81, 64});
    const auto negativePlan = PrepareDupireEuropeanCallQuoteRisk(negative, Call(), {0.8, 81, 64});
    for (int rows : {1, 3}) {
        Matrix_<> directions(rows, 6, 0.0);
        directions(0, 3) = 1.0;
        directions(0, 2) = -0.0;
        const auto before = EvaluateDupireQuoteSecondOrder(positivePlan, positivePlan.InputAxis(), directions);
        const auto after = EvaluateDupireQuoteSecondOrder(negativePlan, negativePlan.InputAxis(), directions);
        const auto& a = before.Risk();
        const auto& b = after.Risk();
        EXPECT_EQ(a.Value(), b.Value());
        EXPECT_EQ(a.Gradient(), b.Gradient());
        EXPECT_EQ(a.DirectionalDerivatives(), b.DirectionalDerivatives());
        EXPECT_TRUE(std::equal(a.HessianProducts().begin(), a.HessianProducts().end(), b.HessianProducts().begin()));
        for (const auto* risk : {&a, &b}) {
            const auto& backendPoint = risk->Pointwise() ? risk->Pointwise()->Point() : risk->Shared()->Point();
            EXPECT_EQ(risk->Point()[2], 0.0);
            EXPECT_FALSE(std::signbit(risk->Point()[2]));
            EXPECT_EQ(backendPoint[2], 0.0);
            EXPECT_FALSE(std::signbit(backendPoint[2]));
            EXPECT_TRUE(std::signbit(risk->Directions()(0, 2)));
        }
    }
    EXPECT_FALSE(std::signbit(positive.Inputs().quoteSpreads_(1, 0)));
    EXPECT_TRUE(std::signbit(negative.Inputs().quoteSpreads_(1, 0)));
}

TEST(DupireQuoteSecondOrderTest, RejectsSubnormalSpreadsBeforeRecordingWithOrWithoutDaz) {
    const RestoreFloatingEnvironment_ restore;
    ASSERT_TRUE(restore.Saved());
    ASSERT_EQ(std::fesetround(FE_TONEAREST), 0);
#if defined(__x86_64__) || defined(_M_X64)
    constexpr unsigned DAZ = 1u << 6;
    _mm_setcsr(_mm_getcsr() & ~DAZ);
#endif
    constexpr double smallest = std::numeric_limits<double>::denorm_min();
    const std::array<double, 2> neighbors{smallest, -smallest};
    const std::array<DupireCalibrationSnapshot_, 2> snapshots{Snapshot(false, false, neighbors[0]), Snapshot(false, false, neighbors[1])};
    for (size_t i = 0; i < snapshots.size(); ++i)
        ASSERT_EQ(std::memcmp(&snapshots[i].Inputs().quoteSpreads_.Data()[2], &neighbors[i], sizeof(double)), 0);

    const auto reject = [&] {
        const auto nativeBefore = SecondOrderDetail::LastSecondOrderLogicalPeak();
        const auto sharedBefore = SharedSecondOrderDetail::LastLogicalPeak();
        for (const auto& snapshot : snapshots)
            CheckValidation([&] { (void)PrepareDupireEuropeanCallQuoteRisk(snapshot, Call(), {0.8, 81, 64}); }, Reason::OUTSIDE_DOMAIN, "quote:2");
        EXPECT_EQ(SecondOrderDetail::LastSecondOrderLogicalPeak(), nativeBefore);
        EXPECT_EQ(SharedSecondOrderDetail::LastLogicalPeak(), sharedBefore);
        EXPECT_EQ(std::fegetround(), FE_TONEAREST);
    };
    reject();
#if defined(__x86_64__) || defined(_M_X64)
    const unsigned beforeDaz = _mm_getcsr();
    {
        SCOPED_TRACE("MXCSR.DAZ enabled");
        const RestoreFloatingEnvironment_ restoreDaz;
        ASSERT_TRUE(restoreDaz.Saved());
        _mm_setcsr(beforeDaz | DAZ);
        ASSERT_NE(_mm_getcsr() & DAZ, 0U);
        reject();
        EXPECT_EQ(_mm_getcsr(), beforeDaz | DAZ);
    }
    EXPECT_EQ(_mm_getcsr(), beforeDaz);
#endif
}

TEST(DupireQuoteSecondOrderTest, RejectsOtherRoundingModesBeforeRecordingAndKeepsThePlan) {
    const RestoreFloatingEnvironment_ restore;
    ASSERT_TRUE(restore.Saved());
    const auto snapshot = Snapshot();
    const auto prepare = [&] { return PrepareDupireEuropeanCallQuoteRisk(snapshot, Call(), {0.8, 81, 64}); };
    const auto plan = prepare();
    const auto nativeBefore = SecondOrderDetail::LastSecondOrderLogicalPeak();
    const auto sharedBefore = SharedSecondOrderDetail::LastLogicalPeak();
    for (const auto& [mode, name] :
         {std::pair{FE_DOWNWARD, "FE_DOWNWARD"}, std::pair{FE_UPWARD, "FE_UPWARD"}, std::pair{FE_TOWARDZERO, "FE_TOWARDZERO"}}) {
        ASSERT_EQ(std::fesetround(mode), 0);
        CheckValidation([&] { (void)prepare(); }, Reason::UNSUPPORTED_OPERATION,
                        std::string("PrepareDupireEuropeanCallQuoteRisk requires FE_TONEAREST, current mode is ") + name);
        EXPECT_EQ(std::fegetround(), mode);
        for (int rows : {0, 1, 3}) {
            const Matrix_<> direction(rows, 6, 1.0);
            CheckValidation([&] { (void)EvaluateDupireQuoteSecondOrder(plan, plan.InputAxis(), direction, {0, 0}); }, Reason::UNSUPPORTED_OPERATION,
                            std::string("EvaluateDupireQuoteSecondOrder requires FE_TONEAREST, current mode is ") + name);
            EXPECT_EQ(std::fegetround(), mode);
        }
        EXPECT_EQ(SecondOrderDetail::LastSecondOrderLogicalPeak(), nativeBefore);
        EXPECT_EQ(SharedSecondOrderDetail::LastLogicalPeak(), sharedBefore);
    }
    ASSERT_EQ(std::fesetround(FE_TONEAREST), 0);
    for (int rows : {0, 1, 3}) {
        const auto risk = EvaluateDupireQuoteSecondOrder(plan, plan.InputAxis(), Matrix_<>(rows, 6, 1.0));
        EXPECT_EQ(risk.Risk().Value(), DupireSecondOrderDetail::PlanAccess_::Margins(plan).price_);
        EXPECT_EQ(std::fegetround(), FE_TONEAREST);
    }
}

TEST(DupireQuoteSecondOrderTest, PublicEntryReturnsOwningPointwiseAndSharedGreeks) {
    const auto run = [](int rows) {
        const auto plan = PrepareDupireEuropeanCallQuoteRisk(Snapshot(), Call(), {0.8, 81, 64});
        auto axis = plan.InputAxis();
        Matrix_<> directions(rows, 6, 0.0);
        directions(0, 3) = 1.0;
        if (rows == 3) {
            directions(1, 2) = 1.0;
            const std::array<double, 6> mixed{0.5, -0.25, -0.5, 1.0, 0.25, -0.5};
            std::copy(mixed.begin(), mixed.end(), directions.Row(2).begin());
        }
        auto result = EvaluateDupireQuoteSecondOrder(plan, axis, directions);
        directions.Fill(0.0);
        axis.clear();
        return result;
    };
    for (int rows : {1, 3}) {
        const auto result = run(rows);
        const auto& risk = result.Risk();
        EXPECT_NEAR(risk.Value(), Reference::PRICE, 2e-7);
        EXPECT_EQ(result.Call().underlyingId_, "EQ");
        EXPECT_EQ(result.QuoteUnit(), "decimal-vol");
        EXPECT_EQ(result.CalibrationAlgorithm(), "DupireCentral1e-4FixedBaseBand/v1");
        ASSERT_EQ(result.InputAxis().size(), 6U);
        EXPECT_EQ(result.InputAxis()[3].id_, "quote:3");
        EXPECT_EQ(result.InputAxis()[3].strike_, 105.0);
        EXPECT_EQ(result.InputAxis()[3].maturity_, 1.2);
        EXPECT_EQ(risk.Point()[0], 0.008);
        ASSERT_EQ(risk.HessianProducts().Rows(), rows);
        EXPECT_EQ(risk.Directions()(0, 3), 1.0);
        for (int i = 0; i < 6; ++i)
            EXPECT_NEAR(risk.Gradient()[i], Reference::VEGA[i], 2e-6);
        for (int row = 0; row < rows; ++row) {
            double directional = 0.0;
            for (int col = 0; col < 6; ++col) {
                directional += Reference::VEGA[col] * risk.Directions()(row, col);
                EXPECT_NEAR(risk.HessianProducts()(row, col), Reference::PRODUCTS[row][col], 1e-5);
            }
            EXPECT_NEAR(risk.DirectionalDerivatives()[row], directional, 2e-6);
        }
        if (rows == 1) {
            ASSERT_TRUE(risk.Pointwise());
            EXPECT_EQ(risk.Pointwise()->Execution().recordings_, 1U);
            EXPECT_EQ(risk.Pointwise()->Execution().reverseSweeps_, 1U);
            EXPECT_EQ(risk.Pointwise()->Execution().safePathActivations_, 0U);
        } else {
            ASSERT_TRUE(risk.Shared());
            EXPECT_EQ(risk.Shared()->Execution().sharedRecordings_, 1U);
            EXPECT_EQ(risk.Shared()->Execution().sharedReverseSweeps_, 1U);
            EXPECT_EQ(risk.Shared()->Execution().replayDirections_, 0U);
        }
    }
}

TEST(DupireQuoteSecondOrderTest, FixedSingleDirectionFitsReducedTapeBudget) {
    const auto plan = PrepareDupireEuropeanCallQuoteRisk(Snapshot(), Call(), {0.8, 81, 64});
    Matrix_<> direction(1, 6, 0.0);
    direction(0, 3) = 1.0;
    const auto result = EvaluateDupireQuoteSecondOrder(plan, plan.InputAxis(), direction, {{}, 2'800'000});
    const auto* pointwise = result.Risk().Pointwise();
    ASSERT_TRUE(pointwise);
    EXPECT_NEAR(pointwise->HessianProducts()(0, 3), Reference::PRODUCTS[0][3], 1e-5);
    EXPECT_EQ(pointwise->Execution().safePathActivations_, 0U);
}

TEST(DupireQuoteSecondOrderTest, ThomasRepeatsFactorsWithAnActiveBoundary) {
    using namespace DupireSecondOrderDetail::Thomas;
    using Number = SecondOrderNumber_;
    const Vector_<> point{0.125, -0.25};
    Matrix_<> directions(1, 2);
    directions(0, 0) = 0.75;
    directions(0, 1) = -1.25;
    size_t matrices = 0;
    const auto value = EvaluateSecondOrder(
        [&](const Vector_<Number>& q) {
            auto& arena = *Tape::activeArena;
            using Work = Rollback_<2, 2, 1>;
            const Tape::Scratch_ storage(arena, sizeof(Work));
            Work work;
            return work.Evaluate(arena, {0.5, 1.125}, {1.375, 1.875}, 0, [&](size_t step, const auto& make) {
                if (step != 0)
                    return;
                ++matrices;
                make([&](size_t i) -> std::array<Number, 3> {
                    return i == 0 ? std::array<Number, 3>{Number(0.0), 2.0 + q[0] * q[0], -0.25 + q[1] / 8.0}
                                  : std::array<Number, 3>{-0.375 + q[0] / 8.0, 2.0 + q[1] * q[1], q[0] * q[1] - 0.3125};
                });
            });
        },
        point, directions);
    EXPECT_EQ(matrices, 1U);
    EXPECT_NEAR(value.Value(), 0.28842254199326208848, 1e-15);
    EXPECT_NEAR(value.Gradient()[0], -0.01003406493082375118, 1e-15);
    EXPECT_NEAR(value.Gradient()[1], -0.04897250151653391421, 1e-15);
    EXPECT_NEAR(value.DirectionalDerivatives()[0], 0.05369007819754957938, 1e-15);
    EXPECT_NEAR(value.HessianProducts()(0, 0), -0.07969156263874146794, 1e-15);
    EXPECT_NEAR(value.HessianProducts()(0, 1), 0.11936567741780483185, 1e-15);
    EXPECT_EQ(value.Execution().recordings_, 1U);
    EXPECT_EQ(value.Execution().reverseSweeps_, 1U);
    EXPECT_EQ(value.Execution().safePathActivations_, 0U);
}

TEST(DupireQuoteSecondOrderTest, ThomasReplaysAnExactlyCancelledBoundaryTangent) {
    using namespace DupireSecondOrderDetail::Thomas;
    using Number = SecondOrderNumber_;
    const auto value = EvaluateSecondOrder(
        [](const Vector_<Number>& q) -> Number {
            auto& arena = *Tape::activeArena;
            if (arena.wide_) {
                const Number first = (4.0 - q[0]) * 0.5;
                return (first + 0.5 * q[0]) * 0.5;
            }
            using Work = Rollback_<2, 2, 1>;
            const Tape::Scratch_ storage(arena, sizeof(Work));
            Work work;
            return work.Evaluate(arena, {1.0, 4.0}, {1.0, -0.5}, 1, [&](size_t step, const auto& make) {
                if (step == 0)
                    make([&](size_t i) -> std::array<Number, 3> { return {Number(0.0), Number(2.0), i == 0 ? Number(0.0) : q[0]}; });
            });
        },
        {0.125}, Matrix_<>(1, 1, 1.0));
    EXPECT_EQ(value.Value(), 1.0);
    EXPECT_EQ(value.Gradient()[0], 0.0);
    EXPECT_EQ(value.DirectionalDerivatives()[0], 0.0);
    EXPECT_EQ(value.HessianProducts()(0, 0), 0.0);
    EXPECT_EQ(value.Execution().recordings_, 2U);
    EXPECT_EQ(value.Execution().reverseSweeps_, 1U);
    EXPECT_EQ(value.Execution().safePathActivations_, 1U);
    EXPECT_STREQ(Tape::LastSecondOrderProtection(), "Dupire Thomas derivative cancellation");
}

TEST(DupireQuoteSecondOrderTest, ThomasTangentEliminationReplaysBeforeLosingTheProductResidual) {
    using namespace DupireSecondOrderDetail::Thomas;
    using Number = SecondOrderNumber_;
    constexpr double epsilon = 0x1p-27, coupling = 1.0 + epsilon;
    for (double scale : {1.0, 0x1p-18, -0x1p18}) {
        Matrix_<> directions(1, 2);
        directions(0, 0) = scale * (1.0 - epsilon);
        directions(0, 1) = scale;
        const auto value = EvaluateSecondOrder(
            [coupling](const Vector_<Number>& q) -> Number {
                auto& arena = *Tape::activeArena;
                const Number diagonal = 1.0 + q[0];
                if (arena.wide_) {
                    const Number first = 1.0 / diagonal;
                    return 0.5 * (q[1] + coupling * first);
                }
                using Work = Rollback_<2, 1, 1>;
                const Tape::Scratch_ storage(arena, sizeof(Work));
                Work work;
                return work.Evaluate(arena, {1.0, 0.0}, {1.0}, 1, [&](size_t, const auto& make) {
                    make([&](size_t i) -> std::array<Number, 3> {
                        return i == 0 ? std::array<Number, 3>{Number(0), diagonal, Number(0)}
                                      : std::array<Number, 3>{Number(-coupling), Number(2), Number(-q[1])};
                    });
                });
            },
            {0.0, 0.0}, directions);
        EXPECT_EQ(value.Value(), coupling * 0.5);
        EXPECT_EQ(value.Gradient()[0], -coupling * 0.5);
        EXPECT_EQ(value.Gradient()[1], 0.5);
        EXPECT_EQ(value.DirectionalDerivatives()[0], scale * 0x1p-55);
        EXPECT_EQ(value.HessianProducts()(0, 0), scale);
        EXPECT_EQ(value.HessianProducts()(0, 1), 0.0);
        EXPECT_EQ(value.Execution().callbackEvaluations_, 2U);
        EXPECT_EQ(value.Execution().recordings_, 2U);
        EXPECT_EQ(value.Execution().reverseSweeps_, 1U);
        EXPECT_EQ(value.Execution().safePathActivations_, 1U);
        EXPECT_STREQ(Tape::LastSecondOrderProtection(), "Dupire Thomas tangent elimination cancellation");
        std::cout << "thomas_substitution,tangent_elimination," << std::hexfloat << scale << ',' << value.DirectionalDerivatives()[0] << std::defaultfloat
                  << ',' << value.Execution().recordings_ << ',' << value.Execution().reverseSweeps_ << ',' << value.Execution().safePathActivations_ << '\n';
    }
}

TEST(DupireQuoteSecondOrderTest, ThomasBackSubstitutionAndBothTransposeSubstitutionsProtectResiduals) {
    using namespace DupireSecondOrderDetail::Thomas;
    using Number = SecondOrderNumber_;
    constexpr double eps = 0x1p-27, coupling = 1.0 + eps;
    struct Case_ {
        bool transpose_, firstRow_;
        const char* reason_;
    };
    for (const auto& probe :
         {Case_{false, true, "Dupire Thomas tangent back substitution cancellation"},
          Case_{true, false, "Dupire Thomas transpose forward substitution cancellation"},
          Case_{true, true, "Dupire Thomas transpose back substitution cancellation"}}) {
        for (double scale : {1.0, 0x1p-18, -0x1p18}) {
            SCOPED_TRACE(probe.reason_);
            SCOPED_TRACE(scale);
            Matrix_<> directions(1, 2);
            directions(0, 0) = scale;
            directions(0, 1) = -scale * (1.0 - eps);
            const auto result = EvaluateSecondOrder(
                [&](const Vector_<Number>& q) -> Number {
                    auto& arena = *Tape::activeArena;
                    if (arena.wide_)
                        return 0.5 * (q[0] + coupling * q[1]);
                    Factor_<2> factor;
                    const bool lower = probe.transpose_ && probe.firstRow_;
                    factor.Set(arena, 0, {Number(0), Number(probe.firstRow_ ? 2.0 : 1.0), Number(lower ? 0.0 : -coupling)});
                    factor.Set(arena, 1, {Number(lower ? -coupling : 0.0), Number(probe.firstRow_ ? 1.0 : 2.0), Number(0)});
                    Row_<2> rhs{directions(0, 0), directions(0, 1)};
                    if (!probe.firstRow_)
                        std::swap(rhs[0], rhs[1]);
                    if (probe.transpose_)
                        factor.Transpose(&rhs);
                    else
                        factor.Forward<true>(&rhs);
                    ADD_FAILURE() << "unsafe substitution completed with " << rhs[probe.firstRow_ ? 0 : 1];
                    return Number(0.0);
                },
                {0.0, 0.0}, directions);
            EXPECT_EQ(result.Value(), 0.0);
            EXPECT_EQ(result.Gradient(), (Vector_<>{0.5, 0.5 * coupling}));
            EXPECT_EQ(result.DirectionalDerivatives()[0], scale * 0x1p-55);
            for (int i = 0; i < 2; ++i) {
                EXPECT_EQ(result.HessianProducts()(0, i), 0.0);
                EXPECT_FALSE(std::signbit(result.HessianProducts()(0, i)));
            }
            EXPECT_EQ(result.Execution().recordings_, 2U);
            EXPECT_EQ(result.Execution().reverseSweeps_, 1U);
            EXPECT_EQ(result.Execution().safePathActivations_, 1U);
            EXPECT_STREQ(Tape::LastSecondOrderProtection(), probe.reason_);
            std::cout << "thomas_substitution," << probe.reason_ << ',' << std::hexfloat << scale << ',' << result.DirectionalDerivatives()[0]
                      << std::defaultfloat << ',' << result.Execution().recordings_ << ',' << result.Execution().reverseSweeps_ << ','
                      << result.Execution().safePathActivations_ << '\n';
        }
    }
}

TEST(DupireQuoteSecondOrderTest, ThomasTransposeReplayPreservesAnOtherwiseZeroHessianProduct) {
    using namespace DupireSecondOrderDetail::Thomas;
    using Number = SecondOrderNumber_;
    constexpr double eps = 0x1p-27, coupling = 1.0 + eps;
    for (double scale : {1.0, 0x1p-18, -0x1p18}) {
        Matrix_<> directions(1, 3, 0.0);
        directions(0, 0) = scale * (1.0 - eps);
        directions(0, 1) = scale;
        const auto value = EvaluateSecondOrder(
            [coupling](const Vector_<Number>& q) -> Number {
                auto& arena = *Tape::activeArena;
                if (arena.wide_) {
                    const Number denominator = 2.0 + 2.0 * (coupling * q[0] - q[1]);
                    return q[2] / denominator;
                }
                using Work = Rollback_<2, 1, 1>;
                const Tape::Scratch_ storage(arena, sizeof(Work));
                Work work;
                return work.Evaluate(arena, {0.0, 0.0}, {1.0}, 1, [&](size_t, const auto& make) {
                    make([&](size_t i) -> std::array<Number, 3> {
                        return i == 0 ? std::array<Number, 3>{Number(0), Number(1), Number(-coupling)}
                                      : std::array<Number, 3>{2.0 * q[0], 2.0 - 2.0 * q[1], Number(-q[2])};
                    });
                });
            },
            {0.0, 0.0, 0.0}, directions);
        EXPECT_EQ(value.Value(), 0.0);
        EXPECT_EQ(value.Gradient(), (Vector_<>{0.0, 0.0, 0.5}));
        EXPECT_EQ(value.DirectionalDerivatives()[0], 0.0);
        EXPECT_EQ(value.HessianProducts()(0, 0), 0.0);
        EXPECT_EQ(value.HessianProducts()(0, 1), 0.0);
        EXPECT_EQ(value.HessianProducts()(0, 2), scale * 0x1p-55);
        EXPECT_EQ(value.Execution().callbackEvaluations_, 2U);
        EXPECT_EQ(value.Execution().recordings_, 2U);
        EXPECT_EQ(value.Execution().reverseSweeps_, 1U);
        EXPECT_EQ(value.Execution().safePathActivations_, 1U);
        EXPECT_STREQ(Tape::LastSecondOrderProtection(), "Dupire Thomas transpose forward substitution cancellation");
        std::cout << "thomas_substitution,transpose_hvp," << std::hexfloat << scale << ',' << value.HessianProducts()(0, 2) << std::defaultfloat << ','
                  << value.Execution().recordings_ << ',' << value.Execution().reverseSweeps_ << ',' << value.Execution().safePathActivations_ << '\n';
    }
}

TEST(DupireQuoteSecondOrderTest, ThomasMatchesAllPrimalStepsAndPointwiseWork) {
    using namespace DupireSecondOrderDetail;
    const auto plan = PrepareDupireEuropeanCallQuoteRisk(Snapshot(), Call(), {0.8, 81, 64});
    const auto& kernel = PlanAccess_::Kernel(plan);
    const auto& point = PlanAccess_::Point(plan);
    Kernel_::PrimalTrace_ expected, actual;
    const auto price = kernel.Rollback(kernel.Calibrate(point), nullptr, &expected);
    int row = 0;
    for (const std::array<double, 6>& seed : {std::array<double, 6>{0, 0, 0, 1, 0, 0}, {0, 0, 1, 0, 0, 0}, {0.5, -0.25, -0.5, 1, 0.25, -0.5}}) {
        Matrix_<> direction(1, 6);
        std::copy(seed.begin(), seed.end(), direction.Row(0).begin());
        std::pair<size_t, size_t> scalarWork, thomasWork;
        const auto count = [] {
            const auto& arena = *SecondOrderDetail::activeArena;
            return std::pair<size_t, size_t>(arena.statements_, arena.edges_);
        };
        Kernel_::PrimalTrace_ scalarTrace;
        const auto scalar = EvaluateSecondOrder([&](const auto& x) {
            const auto result = kernel.Rollback(kernel.Calibrate(x), nullptr, &scalarTrace);
            scalarWork = count();
            return result;
        }, point, direction);
        const auto prototype = EvaluateSecondOrder([&](const auto& x) {
            const auto result = kernel.RollbackSingleDirection(x, &actual);
            thomasWork = count();
            return result;
        }, point, direction);
        ASSERT_EQ(prototype.Execution().safePathActivations_, 0U) << SecondOrderDetail::LastSecondOrderProtection();
        EXPECT_EQ(prototype.Value(), price);
        for (size_t step = 0; step < actual.size(); ++step) {
            EXPECT_EQ(std::memcmp(actual[step].data(), expected[step].data(), sizeof(actual[step])), 0) << step;
            EXPECT_EQ(std::memcmp(actual[step].data(), scalarTrace[step].data(), sizeof(actual[step])), 0) << step;
        }
        EXPECT_LT(thomasWork.first, scalarWork.first);
        EXPECT_LT(thomasWork.second, scalarWork.second);
        std::cout << "thomas_work," << scalarWork.first << ',' << scalarWork.second << ',' << thomasWork.first << ',' << thomasWork.second << '\n';
        double directionalReference = 0.0;
        double gradientError = 0.0, hvpError = 0.0;
        for (int i = 0; i < 6; ++i) {
            directionalReference += seed[i] * Reference::VEGA[i];
            EXPECT_NEAR(prototype.Gradient()[i], Reference::VEGA[i], 2e-6);
            EXPECT_NEAR(prototype.HessianProducts()(0, i), Reference::PRODUCTS[row][i], 2e-6);
            gradientError = std::max(gradientError, std::abs(prototype.Gradient()[i] - scalar.Gradient()[i]));
            hvpError = std::max(hvpError, std::abs(prototype.HessianProducts()(0, i) - scalar.HessianProducts()(0, i)));
        }
        EXPECT_NEAR(prototype.DirectionalDerivatives()[0], directionalReference, 2e-6);
        const auto directionalError = std::abs(prototype.DirectionalDerivatives()[0] - scalar.DirectionalDerivatives()[0]);
        std::cout << "thomas_error," << gradientError << ',' << directionalError << ',' << hvpError << '\n';
        ++row;
    }
}

TEST(DupireQuoteSecondOrderTest, ThomasRangeProtectionAndBudgetRecovery) {
    using namespace DupireSecondOrderDetail;
    const auto plan = PrepareDupireEuropeanCallQuoteRisk(Snapshot(), Call(), {0.8, 81, 64});
    const auto& kernel = PlanAccess_::Kernel(plan);
    const auto& point = PlanAccess_::Point(plan);
    const auto evaluate = [&](const Matrix_<>& direction, const SecondOrderOptions_& options = {}) {
        return EvaluateDupireQuoteSecondOrder(plan, plan.InputAxis(), direction, options);
    };
    Matrix_<> direction(1, 6, 0.0);
    direction(0, 3) = 1.0;
    const auto regular = evaluate(direction);
    const auto warm = evaluate(direction);
    const auto cap = warm.Risk().Pointwise()->Execution().peakTapeBytes_;
    EXPECT_EQ(evaluate(direction, {{}, cap}).Risk().Value(), regular.Risk().Value());
    try {
        (void)evaluate(direction, {{}, cap - 1});
        FAIL() << "one byte below the measured hot cap";
    } catch (const SecondOrderError_& error) {
        EXPECT_EQ(error.Reason().Switch(), Reason::BUDGET_EXCEEDED);
        EXPECT_EQ(error.DirectionIndex(), 0U);
    }
    EXPECT_EQ(evaluate(direction).Risk().Value(), regular.Risk().Value());
    for (double scale : {0x1p-56, 0x1p56}) {
        direction(0, 3) = scale;
        const auto scalarWide = EvaluateSecondOrder([&](const auto& x) {
            if (!SecondOrderDetail::activeArena->wide_)
                SecondOrderDetail::Promote("test oracle uses original scalar wide arithmetic");
            return kernel(nullptr, x);
        }, point, direction);
        const auto protectedResult = evaluate(direction);
        const auto& ad = *protectedResult.Risk().Pointwise();
        EXPECT_EQ(ad.Execution().recordings_, 2U);
        EXPECT_EQ(ad.Execution().reverseSweeps_, 1U);
        EXPECT_EQ(ad.Execution().safePathActivations_, 1U);
        EXPECT_STREQ(SecondOrderDetail::LastSecondOrderProtection(), "Dupire Thomas direction range");
        EXPECT_EQ(ad.Value(), scalarWide.Value());
        EXPECT_EQ(ad.Gradient(), scalarWide.Gradient());
        EXPECT_EQ(ad.DirectionalDerivatives(), scalarWide.DirectionalDerivatives());
        EXPECT_NEAR(ad.DirectionalDerivatives()[0] / scale, Reference::VEGA[3], 2e-6);
        for (int i = 0; i < 6; ++i) {
            EXPECT_EQ(ad.HessianProducts()(0, i), scalarWide.HessianProducts()(0, i));
            EXPECT_NEAR(ad.HessianProducts()(0, i) / scale, Reference::PRODUCTS[0][i], 2e-6);
        }
    }
    direction(0, 3) = 0x1p1023;
    EXPECT_THROW((void)evaluate(direction), SecondOrderError_);
    direction(0, 3) = 1.0;
    EXPECT_EQ(evaluate(direction).Risk().Value(), kernel(nullptr, point));
}

TEST(DupireQuoteSecondOrderTest, ThomasChecksFiniteSeedScaleAndFixedPlanCancellation) {
    using namespace DupireSecondOrderDetail;
    const auto plan = PrepareDupireEuropeanCallQuoteRisk(Snapshot(), Call(), {0.8, 81, 64});
    const double cancellation = -Reference::VEGA[3] / Reference::VEGA[2];
    const auto check = [&](double scale, double earlySeed, size_t sweeps, size_t protections) {
        SCOPED_TRACE(scale);
        SCOPED_TRACE(earlySeed);
        Matrix_<> direction(1, 6, 0.0);
        direction(0, 2) = earlySeed * scale;
        direction(0, 3) = scale;
        const auto result = EvaluateDupireQuoteSecondOrder(plan, plan.InputAxis(), direction);
        const auto& ad = *result.Risk().Pointwise();
        EXPECT_EQ(ad.Value(), PlanAccess_::Margins(plan).price_);
        EXPECT_NEAR(ad.DirectionalDerivatives()[0] / scale, Reference::VEGA[3] + earlySeed * Reference::VEGA[2], 2e-6);
        for (int i = 0; i < 6; ++i) {
            EXPECT_NEAR(ad.Gradient()[i], Reference::VEGA[i], 2e-6);
            EXPECT_NEAR(ad.HessianProducts()(0, i) / scale, Reference::PRODUCTS[0][i] + earlySeed * Reference::PRODUCTS[1][i], 2e-6);
        }
        const auto& work = ad.Execution();
        EXPECT_EQ(work.recordings_, 1 + protections);
        EXPECT_EQ(work.reverseSweeps_, sweeps);
        EXPECT_EQ(work.safePathActivations_, protections);
        const char* reason = SecondOrderDetail::LastSecondOrderProtection();
        std::cout << "thomas_seed_admission," << std::hexfloat << scale << ',' << earlySeed << std::defaultfloat << ',' << work.recordings_ << ','
                  << work.reverseSweeps_ << ',' << work.safePathActivations_ << ',' << reason << '\n';
    };
    check(1.0, cancellation, 1, 1);
    EXPECT_STREQ(SecondOrderDetail::LastSecondOrderProtection(), "Dupire Thomas tangent back substitution cancellation");
    check(0x1p-20, 0.0, 1, 0);
    check(-0x1p20, 0.0, 2, 1);
    EXPECT_STREQ(SecondOrderDetail::LastSecondOrderProtection(), "narrow addition lost term");
    check(std::nextafter(0x1p-20, 0.0), 0.0, 1, 1);
    EXPECT_STREQ(SecondOrderDetail::LastSecondOrderProtection(), "Dupire Thomas direction range");
    check(1.0, std::nextafter(0x1p-20, 0.0), 1, 1);
    EXPECT_STREQ(SecondOrderDetail::LastSecondOrderProtection(), "Dupire Thomas direction range");
}

TEST(DupireQuoteSecondOrderTest, RejectsNonregisteredInputsAndMisidentifiedAxesBeforeRecording) {
    const auto calibration = Snapshot();
    const auto call = Call();
    CheckValidation([&] { (void)PrepareDupireEuropeanCallQuoteRisk(Snapshot(true), call, {0.8, 81, 64}); }, Reason::OUTSIDE_DOMAIN, "quote:0");
    CheckValidation([&] { (void)PrepareDupireEuropeanCallQuoteRisk(Snapshot(false, true), call, {0.8, 81, 64}); }, Reason::OUTSIDE_DOMAIN, "80");
    CheckValidation([&] { (void)PrepareDupireEuropeanCallQuoteRisk(calibration, call, {0.1, 81, 64}); }, Reason::OUTSIDE_DOMAIN, "logSpotHalfWidth");
    CheckValidation([&] { (void)PrepareDupireEuropeanCallQuoteRisk(calibration, call, {0.8, 80, 64}); }, Reason::INVALID_INPUT, "spaceNodes");
    const auto plan = PrepareDupireEuropeanCallQuoteRisk(calibration, call, {0.8, 81, 64});
    Matrix_<> directions(0, 6);
    auto axis = plan.InputAxis();
    std::swap(axis[1], axis[2]);
    CheckValidation([&] { (void)EvaluateDupireQuoteSecondOrder(plan, axis, directions, {0, 0}); }, Reason::INVALID_INPUT, "column 1");
    axis = plan.InputAxis();
    axis[3].maturity_ = 1.25;
    CheckValidation([&] { (void)EvaluateDupireQuoteSecondOrder(plan, axis, directions, {0, 0}); }, Reason::INVALID_INPUT, "column 3");
    const auto empty = EvaluateDupireQuoteSecondOrder(plan, plan.InputAxis(), directions);
    ASSERT_TRUE(empty.Risk().Pointwise());
    EXPECT_EQ(empty.Risk().HessianProducts().Rows(), 0);
    EXPECT_EQ(empty.Risk().HessianProducts().Cols(), 6);
    axis = plan.InputAxis();
    axis[1].id_ = axis[0].id_;
    CheckValidation([&] { (void)EvaluateDupireQuoteSecondOrder(plan, axis, directions); }, Reason::INVALID_INPUT, "column 1");
    axis = plan.InputAxis();
    axis[0].id_ = "QUOTE:0";
    CheckValidation([&] { (void)EvaluateDupireQuoteSecondOrder(plan, axis, directions); }, Reason::INVALID_INPUT, "column 0");
}

TEST(DupireQuoteSecondOrderTest, PrivateQualificationRejectsBadBoundaryAndUnresolvedMargins) {
    using namespace DupireSecondOrderDetail;
    const double epsilon = std::numeric_limits<double>::epsilon();
    CheckValidation([] { (void)Kernel_(120.0, 0.1); }, Reason::OUTSIDE_DOMAIN, "right discounted boundary margin is nonpositive at PDE time step 0");
    double minimum = 1.0;
    CheckValidation([&] { CertifyCalibration({epsilon, 2.0 * epsilon}, "Dupire numerator", 2, 1, minimum); }, Reason::NUMERICAL_FAILURE,
                    "calibration strike index 2, maturity index 1");
    Margins_ margins;
    CheckValidation([&] { CertifyRow({-1.0, epsilon}, {std::nextafter(2.0, 3.0)}, {-1.0, epsilon}, 4, 7, margins); }, Reason::NUMERICAL_FAILURE,
                    "PDE step 4, space index 7");
}

TEST(DupireQuoteSecondOrderTest, IncludesBothCalibrationAndPricingCurvature) {
    using namespace DupireSecondOrderDetail;
    const auto plan = PrepareDupireEuropeanCallQuoteRisk(Snapshot(), Call(), {0.8, 81, 64});
    const auto& kernel = PlanAccess_::Kernel(plan);
    const auto& quotes = PlanAccess_::Point(plan);
    const auto local = kernel.Calibrate(quotes);
    const int strikes = static_cast<int>(LOCAL_STRIKES.size()), times = static_cast<int>(LOCAL_TIMES.size());
    Vector_<> flatVols(strikes * times);
    Matrix_<> jacobian(strikes * times, quotes.size());
    const Matrix_<> noDirections(0, static_cast<int>(quotes.size()));
    for (int t = 0; t < times; ++t)
        for (int k = 0; k < strikes; ++k) {
            const int cell = t * strikes + k;
            flatVols[cell] = local[t][k];
            const auto calibrated = EvaluateSecondOrder([&](const auto& x) { return kernel.Calibrate(x)[t][k]; }, quotes, noDirections);
            std::copy(calibrated.Gradient().begin(), calibrated.Gradient().end(), jacobian.Row(cell).begin());
        }

    const auto pricing = [&](const Vector_<SecondOrderNumber_>& vols) {
        Surface_<SecondOrderNumber_> grid;
        for (int t = 0; t < times; ++t)
            for (int k = 0; k < strikes; ++k)
                grid[t][k] = vols[t * strikes + k];
        return kernel.Rollback(grid);
    };
    const auto priceVega = EvaluateSecondOrder(pricing, flatVols, Matrix_<>(0, flatVols.size())).Gradient();
    Matrix_<> direction(1, quotes.size(), 0.0);
    direction(0, 3) = 1.0;
    const auto fixedCalibration = [&](const Vector_<SecondOrderNumber_>& x) {
        Vector_<SecondOrderNumber_> linearVols(flatVols.begin(), flatVols.end());
        for (int cell = 0; cell < jacobian.Rows(); ++cell)
            for (int col = 0; col < jacobian.Cols(); ++col)
                linearVols[cell] += jacobian(cell, col) * (x[col] - quotes[col]);
        return pricing(linearVols);
    };
    const auto fixedPricing = [&](const Vector_<SecondOrderNumber_>& x) {
        const auto calibrated = kernel.Calibrate(x);
        SecondOrderNumber_ linearPrice(0.0);
        for (int t = 0; t < times; ++t)
            for (int k = 0; k < strikes; ++k)
                linearPrice += priceVega[t * strikes + k] * (calibrated[t][k] - local[t][k]);
        return linearPrice;
    };
    const auto fromPricing = EvaluateSecondOrder(fixedCalibration, quotes, direction);
    const auto fromCalibration = EvaluateSecondOrder(fixedPricing, quotes, direction);
    const auto full = EvaluateDupireQuoteSecondOrder(plan, plan.InputAxis(), direction);
    EXPECT_GT(std::abs(fromPricing.HessianProducts()(0, 3)), 1.0);
    EXPECT_GT(std::abs(fromCalibration.HessianProducts()(0, 3)), 1.0);
    std::cout << "thomas_curvature,quote:3," << fromPricing.HessianProducts()(0, 3) << ',' << fromCalibration.HessianProducts()(0, 3) << ','
              << full.Risk().HessianProducts()(0, 3) << '\n';
    for (int col = 0; col < direction.Cols(); ++col)
        EXPECT_NEAR(full.Risk().HessianProducts()(0, col), fromPricing.HessianProducts()(0, col) + fromCalibration.HessianProducts()(0, col), 1e-5);
}

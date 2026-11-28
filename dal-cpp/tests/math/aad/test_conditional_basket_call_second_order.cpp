#include <gtest/gtest.h>

#include <cmath>
#include <cstring>
#include <future>
#include <sstream>

#include <dal/math/analytics/detail/conditionalbasketcallrisk.hpp>

using namespace Dal;
using namespace Dal::AAD;

namespace {
    using ConditionalBasketDetail::FixedConditionalBasketCall_;
    namespace Tape = SecondOrderDetail;

    bool SameBits(double left, double right) { return std::memcmp(&left, &right, sizeof(double)) == 0; }

    void CheckReplay(const FixedConditionalBasketCall_::Nodes_& nodes,
                     const Vector_<>& point,
                     const Matrix_<>& direction,
                     double expected,
                     const char* reason) {
        const FixedConditionalBasketCall_ plan(0.5, 0.0, nodes, point);
        FixedConditionalBasketCall_::Trace_ actualTrace, referenceTrace;
        const auto wide = EvaluateSecondOrder(
            [&](const auto& x) -> SecondOrderNumber_ {
                if (!Tape::activeArena->wide_)
                    Tape::Promote("forced scalar basket reference");
                const SecondOrderNumber_ driftA = -0.5 * x[2] * x[2];
                const SecondOrderNumber_ projectionB = x[3] * x[4];
                const SecondOrderNumber_ driftB = -0.5 * projectionB * projectionB;
                SecondOrderNumber_ sum(0.0);
                for (size_t i = 0; i < nodes.size(); ++i) {
                    const auto& node = nodes[i];
                    const SecondOrderNumber_ exercise = 1.0 - x[0] * exp(driftA + x[2] * node.z_);
                    const SecondOrderNumber_ forward = x[1] * exp(driftB + projectionB * node.z_);
                    const SecondOrderNumber_ price = forward - exercise;
                    referenceTrace.prices_[i] = Tape::Access_::Value(price);
                    sum += node.weight_ * price;
                }
                return 0.5 * sum;
            },
            point, direction);
        const auto actual = plan.Evaluate(point, direction, {}, &actualTrace);
        EXPECT_DOUBLE_EQ(wide.DirectionalDerivatives()[0], expected);
        EXPECT_EQ(actual.DirectionalDerivatives(), wide.DirectionalDerivatives());
        EXPECT_TRUE(SameBits(actual.Value(), wide.Value()));
        for (size_t i = 0; i < nodes.size(); ++i)
            EXPECT_TRUE(SameBits(actualTrace.prices_[i], referenceTrace.prices_[i])) << i;
        for (int i = 0; i < direction.Cols(); ++i) {
            EXPECT_TRUE(SameBits(actual.Gradient()[i], wide.Gradient()[i])) << i;
            EXPECT_TRUE(SameBits(actual.HessianProducts()(0, i), wide.HessianProducts()(0, i))) << i;
        }
        EXPECT_EQ(actual.Execution().method_, "NativeBasketScalarReplay");
        EXPECT_EQ(actual.Execution().callbackEvaluations_, 2U);
        EXPECT_EQ(actual.Execution().recordings_, 2U);
        EXPECT_EQ(actual.Execution().reverseSweeps_, 1U);
        EXPECT_EQ(actual.Execution().safePathActivations_, 1U);
        EXPECT_STREQ(Tape::LastSecondOrderProtection(), reason);

        const auto run = [&](const SecondOrderOptions_& options = {}) { return plan.Evaluate(point, direction, options); };
        const auto cold = std::async(std::launch::async, [&] { return run().Execution().peakTapeBytes_; }).get();
        const auto hot = std::async(std::launch::async, [&] {
            const auto reject = [&](size_t cap) {
                try {
                    (void)run({{}, cap});
                    ADD_FAILURE() << "one byte under the exact cap must fail";
                } catch (const SecondOrderError_& error) {
                    EXPECT_EQ(error.Reason().Switch(), SecondOrderErrorReason_::Value_::BUDGET_EXCEEDED);
                    EXPECT_EQ(error.DirectionIndex(), 0U);
                }
            };
            reject(cold - 1);
            EXPECT_EQ(run({{}, cold}).Execution().peakTapeBytes_, cold);
            const auto exact = run().Execution().peakTapeBytes_;
            EXPECT_EQ(run({{}, exact}).Execution().peakTapeBytes_, exact);
            reject(exact - 1);
            EXPECT_EQ(run().Execution().peakTapeBytes_, cold);
            return exact;
        }).get();
        std::ostringstream observed;
        observed << std::hexfloat << actual.DirectionalDerivatives()[0];
        testing::Test::RecordProperty("directional", observed.str());
        testing::Test::RecordProperty("cold_cap", std::to_string(cold));
        testing::Test::RecordProperty("hot_cap", std::to_string(hot));
    }

    FixedConditionalBasketCall_::Nodes_ AbsorbingNodes() {
        const double logTwo = std::log(2.0);
        FixedConditionalBasketCall_::Nodes_ nodes;
        nodes.fill({0.5 + logTwo, 0x1p-55, true});
        nodes.front().weight_ = 0.25;
        nodes[46] = {0.5, 0.25 - 45.0 * 0x1p-55, true};
        nodes[47] = {0.5 - logTwo, 0.5, true};
        return nodes;
    }
} // namespace

TEST(ConditionalBasketCallSecondOrderTest, ReplaysRoundedLegProductsBeforeLinearCancellation) {
    constexpr double e = 0x1p-27;
    const Vector_<> point{3.0, 1.0, std::sqrt(-2.0 * std::log1p(-e)), 0.2, 0.0};
    Matrix_<> direction(1, 5, 0.0);
    direction(0, 0) = 0x1p47 * (1.0 + e);
    direction(0, 1) = -0x1p47;
    FixedConditionalBasketCall_::Nodes_ nodes;
    nodes.fill({0.0, 0.015625, true});
    nodes.back().weight_ = 1.0 - 47.0 * 0.015625;
    CheckReplay(nodes, point, direction, -0x1p-8, "linear basket tangent cancellation");
}

TEST(ConditionalBasketCallSecondOrderTest, RecoversReviewerQuadratureSlope) {
    const Vector_<> point{3.0, 1.0, 1.0, 0.2, 0.0};
    Matrix_<> direction(1, 5, 0.0);
    direction(0, 0) = 0x1p30;
    direction(0, 1) = -0x1p30;
    CheckReplay(AbsorbingNodes(), point, direction, 45.0 * 0x1p-26, "linear basket tangent cancellation");
}

TEST(ConditionalBasketCallSecondOrderTest, ReplaysAbsorbedQuadratureTermsBeforePublishingSlope) {
    const Vector_<> point{3.0, 1.0, 0.25, 0.25, 0.0};
    Matrix_<> direction(1, 5, 0.0);
    direction(0, 4) = 0x1p32;
    FixedConditionalBasketCall_::Nodes_ nodes;
    nodes.fill({1.0, 0x1p-55, true});
    nodes.front().weight_ = 0.5;
    nodes[46] = {-1.0, 0.25 - 45.0 * 0x1p-55, true};
    nodes[47] = {-1.0, 0.25, true};
    CheckReplay(nodes, point, direction, 45.0 * 0x1p-25, "basket quadrature tangent cancellation");
}

TEST(ConditionalBasketCallSecondOrderTest, RetainsSmallTermsThroughDiscountWithoutReplay) {
    const Vector_<> point{3.0, 1.0, 1.0, 0.2, 0.0};
    const FixedConditionalBasketCall_ plan(0.5, 0.0, AbsorbingNodes(), point);
    Matrix_<> direction(1, 5, 0.0);
    direction(0, 0) = 0x1p20;
    const auto risk = plan.Evaluate(point, direction);
    EXPECT_EQ(risk.DirectionalDerivatives()[0], 0x1p19 + 6.0 * 0x1p-33);
    EXPECT_EQ(risk.Execution().method_, "NativeBasketComposedAAD");
    EXPECT_EQ(risk.Execution().recordings_, 1U);
    EXPECT_EQ(risk.Execution().reverseSweeps_, 1U);
    EXPECT_EQ(risk.Execution().safePathActivations_, 0U);
}

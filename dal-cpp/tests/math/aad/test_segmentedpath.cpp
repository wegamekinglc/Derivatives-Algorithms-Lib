//
// Created by Codex on 2026/10/09.
//

#include <gtest/gtest.h>

#include <future>
#include <limits>
#include <string>

#include <dal/math/aad/segmentedpath.hpp>

using namespace Dal;

namespace {
    template <class K_> class TypedPath_ : public AAD::SegmentedPathKernel_ {
        const K_& Kernel() const { return static_cast<const K_&>(*this); }

    public:
        Vector_<> InitialState(const Vector_<>& parameters) const override { return Kernel().Initial(parameters); }
        Vector_<AAD::Number_> InitialState(const Vector_<AAD::Number_>& parameters) const override { return Kernel().Initial(parameters); }
        AAD::SegmentedPathTransition_<double>
        Advance(size_t step, const Vector_<>& state, const Vector_<>& parameters, const Vector_<>& drivers) const override {
            return Kernel().Step(step, state, parameters, drivers);
        }
        AAD::SegmentedPathTransition_<AAD::Number_>
        Advance(size_t step, const Vector_<AAD::Number_>& state, const Vector_<AAD::Number_>& parameters, const Vector_<>& drivers) const override {
            return Kernel().Step(step, state, parameters, drivers);
        }
        double Terminal(const Vector_<>& state, const Vector_<>& parameters) const override { return Kernel().Payoff(state, parameters); }
        AAD::Number_ Terminal(const Vector_<AAD::Number_>& state, const Vector_<AAD::Number_>& parameters) const override {
            return Kernel().Payoff(state, parameters);
        }
    };

    class LinearPath_ final : public TypedPath_<LinearPath_> {
    public:
        AAD::SegmentedPathDimensions_ Dimensions() const override { return {3, 1, 0}; }
        template <class T_> Vector_<T_> Initial(const Vector_<T_>& parameters) const { return {parameters[1]}; }

        template <class T_>
        AAD::SegmentedPathTransition_<T_> Step(size_t step, const Vector_<T_>& state, const Vector_<T_>& parameters, const Vector_<>& drivers) const {
            return {{T_(parameters[0] * state[0] + drivers[step])}, T_(0.0), {}};
        }

        template <class T_> T_ Payoff(const Vector_<T_>& state, const Vector_<T_>&) const { return T_(state[0] * state[0]); }
    };

    class CashflowPath_ final : public TypedPath_<CashflowPath_> {
    public:
        AAD::SegmentedPathDimensions_ Dimensions() const override { return {2, 2, 1}; }
        template <class T_> Vector_<T_> Initial(const Vector_<T_>& p) const { return {p[1], T_(2.0 * p[1])}; }
        template <class T_>
        AAD::SegmentedPathTransition_<T_> Step(size_t step, const Vector_<T_>& state, const Vector_<T_>& p, const Vector_<>& drivers) const {
            return {{T_(p[0] * state[0] + drivers[step]), T_(state[1] + state[0])}, T_(p[1] * state[0] + p[0] * state[1]), {step}};
        }
        template <class T_> T_ Payoff(const Vector_<T_>& state, const Vector_<T_>& p) const { return T_(state[0] * state[1] + p[0] * p[1]); }
    };

    class AliasPath_ final : public TypedPath_<AliasPath_> {
        bool large_;

    public:
        explicit AliasPath_(bool large = false) : large_(large) {}
        AAD::SegmentedPathDimensions_ Dimensions() const override { return {1, 2, 0}; }
        template <class T_> Vector_<T_> Initial(const Vector_<T_>& p) const { return {p[0], p[0]}; }
        template <class T_> AAD::SegmentedPathTransition_<T_> Step(size_t, const Vector_<T_>& state, const Vector_<T_>&, const Vector_<>&) const {
            return {{state[0], state[0]}, large_ ? T_(0.0) : state[0], {}};
        }
        template <class T_> T_ Payoff(const Vector_<T_>& state, const Vector_<T_>& p) const {
            return large_ ? T_((state[0] - state[1]) * 1e200) : T_(state[0] + state[1] + p[0]);
        }
    };

    class EmptyPath_ final : public TypedPath_<EmptyPath_> {
        size_t steps_;

    public:
        explicit EmptyPath_(size_t steps) : steps_(steps) {}
        AAD::SegmentedPathDimensions_ Dimensions() const override { return {steps_, 0, 0}; }
        template <class T_> Vector_<T_> Initial(const Vector_<T_>&) const { return {}; }
        template <class T_> AAD::SegmentedPathTransition_<T_> Step(size_t, const Vector_<T_>&, const Vector_<T_>& p, const Vector_<>&) const {
            return {{}, p.empty() ? T_(0.0) : p[0], {}};
        }
        template <class T_> T_ Payoff(const Vector_<T_>&, const Vector_<T_>& p) const { return p.empty() ? T_(0.0) : T_(p[0] * p[0]); }
    };

    class FaultPath_ final : public AAD::SegmentedPathKernel_ {
        std::string fault_;

    public:
        explicit FaultPath_(std::string fault) : fault_(std::move(fault)) {}
        AAD::SegmentedPathDimensions_ Dimensions() const override { return {2, 1, 1}; }
        Vector_<> InitialState(const Vector_<>&) const override {
            if (fault_ == "passive-initial-shape")
                return {};
            return {fault_ == "passive-initial-nonfinite" ? std::numeric_limits<double>::quiet_NaN() : 0.0};
        }
        Vector_<AAD::Number_> InitialState(const Vector_<AAD::Number_>&) const override { return {AAD::Number_(fault_ == "initial" ? 1.0 : 0.0)}; }
        AAD::SegmentedPathTransition_<double> Advance(size_t step, const Vector_<>& s, const Vector_<>&, const Vector_<>&) const override {
            AAD::SegmentedPathTransition_<double> result{{s[0] + 1.0}, 0.0, {step}};
            if (fault_ == "passive-shape")
                result.state_.clear();
            if (fault_ == "passive-trace")
                result.branchTrace_.clear();
            if (fault_ == "passive-nonfinite")
                result.contribution_ = std::numeric_limits<double>::infinity();
            return result;
        }
        AAD::SegmentedPathTransition_<AAD::Number_>
        Advance(size_t step, const Vector_<AAD::Number_>& s, const Vector_<AAD::Number_>&, const Vector_<>&) const override {
            if (fault_ == "throw")
                THROW("controlled native replay failure");
            AAD::SegmentedPathTransition_<AAD::Number_> result{{AAD::Number_(s[0] + 1.0)}, AAD::Number_(0.0), {step}};
            if (fault_ == "state")
                result.state_[0] += 1.0;
            if (fault_ == "contribution")
                result.contribution_ = 1.0;
            if (fault_ == "branch")
                result.branchTrace_[0] += 1;
            if (fault_ == "shape")
                result.state_.clear();
            if (fault_ == "trace")
                result.branchTrace_.clear();
            if (fault_ == "nonfinite")
                result.contribution_ = std::numeric_limits<double>::infinity();
            return result;
        }
        double Terminal(const Vector_<>& s, const Vector_<>&) const override {
            return fault_ == "passive-terminal" ? std::numeric_limits<double>::infinity() : s[0];
        }
        AAD::Number_ Terminal(const Vector_<AAD::Number_>& s, const Vector_<AAD::Number_>&) const override {
            return fault_ == "terminal" ? AAD::Number_(s[0] + 1.0) : s[0];
        }
    };

    class DimensionPath_ final : public TypedPath_<DimensionPath_> {
        AAD::SegmentedPathDimensions_ dimensions_;

    public:
        explicit DimensionPath_(const AAD::SegmentedPathDimensions_& dimensions) : dimensions_(dimensions) {}
        AAD::SegmentedPathDimensions_ Dimensions() const override { return dimensions_; }
        template <class T_> Vector_<T_> Initial(const Vector_<T_>&) const { THROW("kernel unexpectedly evaluated"); }
        template <class T_> AAD::SegmentedPathTransition_<T_> Step(size_t, const Vector_<T_>&, const Vector_<T_>&, const Vector_<>&) const {
            THROW("kernel unexpectedly evaluated");
        }
        template <class T_> T_ Payoff(const Vector_<T_>&, const Vector_<T_>&) const { THROW("kernel unexpectedly evaluated"); }
    };

    class ZeroStepStatePath_ final : public TypedPath_<ZeroStepStatePath_> {
    public:
        AAD::SegmentedPathDimensions_ Dimensions() const override { return {0, 1, 0}; }
        template <class T_> Vector_<T_> Initial(const Vector_<T_>& p) const { return {T_(p[0] * p[0])}; }
        template <class T_> AAD::SegmentedPathTransition_<T_> Step(size_t, const Vector_<T_>&, const Vector_<T_>&, const Vector_<>&) const {
            THROW("zero-step transition unexpectedly evaluated");
        }
        template <class T_> T_ Payoff(const Vector_<T_>& s, const Vector_<T_>& p) const { return T_(s[0] * p[0]); }
    };

    class LongPath_ final : public TypedPath_<LongPath_> {
        size_t steps_;

    public:
        explicit LongPath_(size_t steps) : steps_(steps) {}
        AAD::SegmentedPathDimensions_ Dimensions() const override { return {steps_, 1, 0}; }
        template <class T_> Vector_<T_> Initial(const Vector_<T_>& p) const { return {p[1]}; }
        template <class T_>
        AAD::SegmentedPathTransition_<T_> Step(size_t step, const Vector_<T_>& s, const Vector_<T_>& p, const Vector_<>& d) const {
            return {{T_(s[0] + p[0] * d[step])}, T_(0.0), {}};
        }
        template <class T_> T_ Payoff(const Vector_<T_>& s, const Vector_<T_>&) const { return T_(s[0] * s[0]); }
    };
} // namespace

TEST(AADSegmentedPathTest, TestIndependentThreeStepOracleAcrossSegmentLengths) {
    const LinearPath_ path;
    for (const size_t length : {1u, 2u, 3u, 8u}) {
        SCOPED_TRACE(length);
        AAD::SegmentedPathSettings_ settings;
        settings.segmentSteps_ = length;
        const auto result = AAD::ExecuteSegmentedPath(path, {2.0, 1.0}, {1.0, -1.0, 3.0}, settings);
        ASSERT_NEAR(result.Value(), 169.0, 1e-10);
        ASSERT_EQ(result.Gradient().size(), 2u);
        ASSERT_NEAR(result.Gradient()[0], 390.0, 1e-10);
        ASSERT_NEAR(result.Gradient()[1], 208.0, 1e-10);
        ASSERT_EQ(result.Execution().passiveSteps_, 3u);
        ASSERT_EQ(result.Execution().recomputedSteps_, 3u);
        ASSERT_EQ(result.Execution().segments_, 3u / length + static_cast<size_t>(3u % length != 0));
    }
}

TEST(AADSegmentedPathTest, TestMultistateCashflowsAndFreshParameterPoints) {
    const CashflowPath_ path;
    for (const size_t length : {1u, 2u, 5u}) {
        SCOPED_TRACE(length);
        AAD::SegmentedPathSettings_ settings;
        settings.segmentSteps_ = length;
        const auto first = AAD::ExecuteSegmentedPath(path, {2.0, 1.0}, {1.0, -1.0}, settings);
        ASSERT_DOUBLE_EQ(first.Value(), 46.0);
        ASSERT_DOUBLE_EQ(first.Gradient()[0], 42.0);
        ASSERT_DOUBLE_EQ(first.Gradient()[1], 68.0);
        const auto fresh = AAD::ExecuteSegmentedPath(path, {0.5, 2.0}, {1.0, -1.0}, settings);
        ASSERT_DOUBLE_EQ(fresh.Value(), 14.0);
        ASSERT_DOUBLE_EQ(fresh.Gradient()[0], 40.0);
        ASSERT_DOUBLE_EQ(fresh.Gradient()[1], 12.0);
        ASSERT_DOUBLE_EQ(first.Gradient()[0], 42.0);
        ASSERT_EQ(first.Execution().reverseSweeps_, first.Execution().segments_ + 2);
    }
}

TEST(AADSegmentedPathTest, TestAliasedOutputsAndFiniteSeedsWithoutWeightedPrimal) {
    const auto result = AAD::ExecuteSegmentedPath(AliasPath_(), {3.0}, {});
    ASSERT_DOUBLE_EQ(result.Value(), 12.0);
    ASSERT_DOUBLE_EQ(result.Gradient()[0], 4.0);
    const auto large = AAD::ExecuteSegmentedPath(AliasPath_(true), {1e200}, {});
    ASSERT_DOUBLE_EQ(large.Value(), 0.0);
    ASSERT_DOUBLE_EQ(large.Gradient()[0], 0.0);
}

TEST(AADSegmentedPathTest, TestZeroStepsStateAndParameters) {
    const auto cubic = AAD::ExecuteSegmentedPath(ZeroStepStatePath_(), {3.0}, {});
    ASSERT_DOUBLE_EQ(cubic.Value(), 27.0);
    ASSERT_DOUBLE_EQ(cubic.Gradient()[0], 27.0);
    ASSERT_EQ(cubic.Execution().segments_, 0u);
    ASSERT_EQ(cubic.Execution().reverseSweeps_, 2u);
    for (const size_t steps : {0u, 3u}) {
        SCOPED_TRACE(steps);
        const auto empty = AAD::ExecuteSegmentedPath(EmptyPath_(steps), {}, {});
        ASSERT_DOUBLE_EQ(empty.Value(), 0.0);
        ASSERT_TRUE(empty.Gradient().empty());
        ASSERT_EQ(empty.Execution().recomputedSteps_, steps);
        const auto parameter = AAD::ExecuteSegmentedPath(EmptyPath_(steps), {2.0}, {});
        ASSERT_DOUBLE_EQ(parameter.Value(), 4.0 + 2.0 * steps);
        ASSERT_DOUBLE_EQ(parameter.Gradient()[0], 4.0 + steps);
    }
}

TEST(AADSegmentedPathTest, TestPassiveValidationAndFiniteInputAdmission) {
    for (const auto* fault :
         {"passive-initial-shape", "passive-initial-nonfinite", "passive-shape", "passive-trace", "passive-nonfinite", "passive-terminal"}) {
        SCOPED_TRACE(fault);
        ASSERT_THROW((void)AAD::ExecuteSegmentedPath(FaultPath_(fault), {}, {}), Exception_);
    }
    const auto nonfinite = std::numeric_limits<double>::quiet_NaN();
    ASSERT_THROW((void)AAD::ExecuteSegmentedPath(LinearPath_(), {nonfinite, 1.0}, {1.0, -1.0, 3.0}), Exception_);
    ASSERT_THROW((void)AAD::ExecuteSegmentedPath(LinearPath_(), {2.0, 1.0}, {1.0, nonfinite, 3.0}), Exception_);
    ASSERT_DOUBLE_EQ(AAD::ExecuteSegmentedPath(LinearPath_(), {2.0, 1.0}, {1.0, -1.0, 3.0}).Value(), 169.0);
}

TEST(AADSegmentedPathTest, TestDimensionAndBudgetAdmissionBeforeKernelEvaluation) {
    AAD::SegmentedPathSettings_ settings;
    settings.segmentSteps_ = 1;
    const auto maximum = std::numeric_limits<size_t>::max();
    for (const auto dimensions :
         {AAD::SegmentedPathDimensions_{maximum, 0, 0}, AAD::SegmentedPathDimensions_{0, maximum, 0}, AAD::SegmentedPathDimensions_{2, 0, maximum}}) {
        try {
            (void)AAD::ExecuteSegmentedPath(DimensionPath_(dimensions), {}, {}, settings);
            FAIL() << "invalid dimensions were admitted";
        } catch (const Exception_& error) {
            ASSERT_NE(std::string(error.what()).find("overflow"), std::string::npos);
        }
    }
    settings.checkpointCapacityBudgetBytes_ = 1;
    try {
        (void)AAD::ExecuteSegmentedPath(DimensionPath_({2, 1, 1}), {}, {}, settings);
        FAIL() << "insufficient checkpoint budget was admitted";
    } catch (const Exception_& error) {
        ASSERT_NE(std::string(error.what()).find("checkpoint capacity budget"), std::string::npos);
    }
    settings.segmentSteps_ = 0;
    ASSERT_THROW((void)AAD::ExecuteSegmentedPath(DimensionPath_({2, 1, 1}), {}, {}, settings), Exception_);
}

TEST(AADSegmentedPathTest, TestReplayShapeErrorIdentifiesStep) {
    try {
        (void)AAD::ExecuteSegmentedPath(FaultPath_("shape"), {}, {});
        FAIL() << "invalid replay shape was accepted";
    } catch (const Exception_& error) {
        ASSERT_NE(std::string(error.what()).find("step=0"), std::string::npos);
        ASSERT_NE(std::string(error.what()).find("state shape mismatch"), std::string::npos);
    }
}

TEST(AADSegmentedPathTest, TestReplayMismatchAndFailureRecovery) {
    for (const auto* fault : {"initial", "terminal", "state", "contribution", "branch", "shape", "trace", "nonfinite", "throw"}) {
        SCOPED_TRACE(fault);
        ASSERT_THROW((void)AAD::ExecuteSegmentedPath(FaultPath_(fault), {}, {}), Exception_);
        const auto healthy = AAD::ExecuteSegmentedPath(LinearPath_(), {2.0, 1.0}, {1.0, -1.0, 3.0});
        ASSERT_DOUBLE_EQ(healthy.Gradient()[0], 390.0);
    }
}

TEST(AADSegmentedPathTest, TestExactCapacityBudgetsAndRecovery) {
    AAD::Clear(*AAD::Tape());
    const auto measured = AAD::ExecuteSegmentedPath(CashflowPath_(), {2.0, 1.0}, {1.0, -1.0});
    AAD::SegmentedPathSettings_ settings;
    settings.checkpointCapacityBudgetBytes_ = measured.Execution().checkpointBytes_;
    settings.recordingCapacityBudgetBytes_ = measured.Execution().peakTapeBytes_ + measured.Execution().cleanupReserveBytes_;
    AAD::Clear(*AAD::Tape());
    const auto admitted = AAD::ExecuteSegmentedPath(CashflowPath_(), {2.0, 1.0}, {1.0, -1.0}, settings);
    ASSERT_DOUBLE_EQ(admitted.Gradient()[1], 68.0);
    --*settings.checkpointCapacityBudgetBytes_;
    ASSERT_THROW((void)AAD::ExecuteSegmentedPath(CashflowPath_(), {2.0, 1.0}, {1.0, -1.0}, settings), Exception_);
    settings.checkpointCapacityBudgetBytes_.reset();
    --*settings.recordingCapacityBudgetBytes_;
    AAD::Clear(*AAD::Tape());
    ASSERT_THROW((void)AAD::ExecuteSegmentedPath(CashflowPath_(), {2.0, 1.0}, {1.0, -1.0}, settings), Exception_);
    const auto healthy = AAD::ExecuteSegmentedPath(CashflowPath_(), {2.0, 1.0}, {1.0, -1.0});
    ASSERT_DOUBLE_EQ(healthy.Value(), 46.0);
    AAD::Clear(*AAD::Tape());
}

TEST(AADSegmentedPathTest, TestModeRestorationAndNestedGraphPreservation) {
    const auto mode = AAD::SetNumResultsForAAD(true, 4);
    const LinearPath_ path;
    ASSERT_DOUBLE_EQ(AAD::ExecuteSegmentedPath(path, {2.0, 1.0}, {1.0, -1.0, 3.0}).Value(), 169.0);
    ASSERT_TRUE(AAD::Tape()->multi_);
    ASSERT_EQ(AAD::Tape()->numAdj_, 4u);
    ASSERT_THROW((void)AAD::ExecuteSegmentedPath(FaultPath_("branch"), {}, {}), Exception_);
    ASSERT_TRUE(AAD::Tape()->multi_);
    ASSERT_EQ(AAD::Tape()->numAdj_, 4u);
    AAD::RecordingScope_ outer;
    AAD::Number_ input;
    outer.RegisterInput(input, 3.0);
    outer.StartRecording();
    AAD::Number_ output = input * input;
    ASSERT_THROW((void)AAD::ExecuteSegmentedPath(path, {2.0, 1.0}, {1.0, -1.0, 3.0}), Exception_);
    outer.FinishRecording();
    AAD::NativeOperations_::SetSeed(output, 1.0, 2);
    outer.Reverse();
    ASSERT_DOUBLE_EQ(AAD::NativeOperations_::ReadAdjoint(input, 2), 6.0);
    outer.Close();
}

TEST(AADSegmentedPathTest, TestConcurrentRequestsOwnDetachedResults) {
    const CashflowPath_ path;
    auto first = std::async(std::launch::async, [&] { return AAD::ExecuteSegmentedPath(path, {2.0, 1.0}, {1.0, -1.0}); });
    auto second = std::async(std::launch::async, [&] { return AAD::ExecuteSegmentedPath(path, {0.5, 2.0}, {1.0, -1.0}); });
    const auto a = first.get();
    const auto b = second.get();
    ASSERT_DOUBLE_EQ(a.Gradient()[0], 42.0);
    ASSERT_DOUBLE_EQ(b.Gradient()[0], 40.0);
    AAD::Clear(*AAD::Tape());
    ASSERT_DOUBLE_EQ(a.Value(), 46.0);
    ASSERT_DOUBLE_EQ(b.Value(), 14.0);
}

TEST(AADSegmentedPathTest, TestLongPathRecomputesEveryStepWithBoundedSegmentTape) {
    const size_t steps = 16384;
    const Vector_<> drivers(steps, 1.0 / steps);
    AAD::Clear(*AAD::Tape());
    AAD::SegmentedPathSettings_ settings;
    settings.segmentSteps_ = 64;
    const auto segmented = AAD::ExecuteSegmentedPath(LongPath_(steps), {2.0, 1.0}, drivers, settings);
    ASSERT_DOUBLE_EQ(segmented.Value(), 9.0);
    ASSERT_DOUBLE_EQ(segmented.Gradient()[0], 6.0);
    ASSERT_DOUBLE_EQ(segmented.Gradient()[1], 6.0);
    ASSERT_EQ(segmented.Execution().passiveSteps_, steps);
    ASSERT_EQ(segmented.Execution().recomputedSteps_, steps);
    ASSERT_EQ(segmented.Execution().reverseSweeps_, steps / 64 + 2);
    AAD::Clear(*AAD::Tape());
    settings.segmentSteps_ = steps;
    const auto single = AAD::ExecuteSegmentedPath(LongPath_(steps), {2.0, 1.0}, drivers, settings);
    ASSERT_DOUBLE_EQ(single.Gradient()[0], segmented.Gradient()[0]);
    ASSERT_LT(segmented.Execution().peakTapeBytes_, single.Execution().peakTapeBytes_);
    ASSERT_GT(segmented.Execution().checkpointBytes_, single.Execution().checkpointBytes_);
    AAD::Clear(*AAD::Tape());
}

//
// Created by Codex on 2026/10/8.
//

#include <gtest/gtest.h>

#include <dal/math/aad/implicitroot.hpp>
#include <dal/math/aad/native.hpp>
#include <dal/math/aad/statistics.hpp>
#include <dal/math/buffercapacity.hpp>
#include <dal/platform/platform.hpp>

using namespace Dal;
using namespace Dal::AAD;

namespace {
    class CallbackBufferRootEquation_ final : public ImplicitRootEquation_ {
        Vector_<>* scratch_;
        bool reject_;

    public:
        explicit CallbackBufferRootEquation_(Vector_<>* scratch, bool reject = false) : scratch_(scratch), reject_(reject) {}
        [[nodiscard]] ImplicitRootEvaluation_ Evaluate(const Vector_<>& theta, const Vector_<>& inputs) const override {
            *scratch_ = Vector_<>(8, 3.0);
            REQUIRE(!reject_, "Controlled callback failure");
            return {Vector_<>{theta[0] - inputs[0]}, SquareMatrix_<>(1, 1.0), Matrix_<>(1, 1, -1.0)};
        }
    };
} // namespace

TEST(AADLinearSolveTest, TestRecordedImplicitRootCallbackBufferRemainsCallerOwned) {
    Clear(*Tape());
    auto mode = SetNumResultsForAAD(false, 1);
    RecordingScope_ scope;
    Number_ input;
    scope.RegisterInput(input, 2.0);
    scope.StartRecording();
    Vector_<> scratch;
    const CallbackBufferRootEquation_ equation(&scratch);
    BufferCapacityBudget_ budget(4096);
    BufferCapacityScope_ caller(&budget);
    const auto root =
        ImplicitRootWithAccuracy(&scope, equation, Vector_<>{2.0}, Vector_<Number_>{input}, ImplicitRootAccuracyPolicy_{Vector_<>{0.0}, 0.0});
    scope.FinishRecording();
    scope.Close();
    ASSERT_EQ(MeasureTape(*Tape()).reverseEventCapacityBytes_, 0);
    ASSERT_EQ(budget.CapacityBytes(), 8 * sizeof(double) + sizeof(Number_) + 2 * sizeof(double));
    scratch = Vector_<>();
    ASSERT_EQ(budget.CapacityBytes(), sizeof(Number_) + 2 * sizeof(double));
    ASSERT_DOUBLE_EQ(root.diagnostics_.residuals_[0], 0.0);
    Clear(*Tape());
}

TEST(AADLinearSolveTest, TestRecordedImplicitRootCallbackReleasesPreexistingCallerBuffer) {
    Clear(*Tape());
    auto mode = SetNumResultsForAAD(false, 1);
    RecordingScope_ scope;
    Number_ input;
    scope.RegisterInput(input, 2.0);
    scope.StartRecording();
    Vector_<> scratch;
    const CallbackBufferRootEquation_ equation(&scratch);
    BufferCapacityBudget_ budget(65536);
    BufferCapacityScope_ caller(&budget);
    scratch = Vector_<>(4096, 7.0);
    ASSERT_EQ(budget.CapacityBytes(), 4096 * sizeof(double));
    const auto root =
        ImplicitRootWithAccuracy(&scope, equation, Vector_<>{2.0}, Vector_<Number_>{input}, ImplicitRootAccuracyPolicy_{Vector_<>{0.0}, 0.0});
    scope.FinishRecording();
    scope.Close();
    ASSERT_EQ(MeasureTape(*Tape()).reverseEventCapacityBytes_, 0);
    ASSERT_EQ(budget.CapacityBytes(), 8 * sizeof(double) + sizeof(Number_) + 2 * sizeof(double));
    scratch = Vector_<>();
    ASSERT_EQ(budget.CapacityBytes(), sizeof(Number_) + 2 * sizeof(double));
    ASSERT_DOUBLE_EQ(root.diagnostics_.residuals_[0], 0.0);
    Clear(*Tape());
}

TEST(AADLinearSolveTest, TestRecordedImplicitRootThrowingCallbackPreservesCallerOwnership) {
    Clear(*Tape());
    auto mode = SetNumResultsForAAD(false, 1);
    RecordingScope_ scope;
    Number_ input;
    scope.RegisterInput(input, 2.0);
    scope.StartRecording();
    Vector_<> scratch;
    const CallbackBufferRootEquation_ equation(&scratch, true);
    BufferCapacityBudget_ budget(4096);
    BufferCapacityScope_ caller(&budget);
    const auto before = MeasureTape(*Tape());
    ASSERT_THROW(static_cast<void>(ImplicitRootWithAccuracy(&scope, equation, Vector_<>{2.0}, Vector_<Number_>{input},
                                                            ImplicitRootAccuracyPolicy_{Vector_<>{0.0}, 0.0})),
                 Exception_);
    ASSERT_EQ(MeasureTape(*Tape()).nodes_, before.nodes_);
    ASSERT_EQ(MeasureTape(*Tape()).reverseEventCapacityBytes_, 0);
    ASSERT_EQ(budget.CapacityBytes(), 8 * sizeof(double));
    ASSERT_THROW(scope.FinishRecording(), Exception_);
    scope.Close();
    scratch = Vector_<>();
    ASSERT_EQ(budget.CapacityBytes(), 0);
    Clear(*Tape());
}

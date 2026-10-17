//
// Created by Codex on 2026/10/09.
//

#include <gtest/gtest.h>

#include <future>
#include <limits>
#include <optional>
#include <utility>

#include <dal/math/aad/linearsolve.hpp>
#include <dal/math/aad/native.hpp>
#include <dal/math/aad/structuraljacobiannative.hpp>

using namespace Dal;
using namespace Dal::AAD;

namespace {
    class WidthOverride_ {
        size_t oldWidth_;

    public:
        explicit WidthOverride_(size_t width) : oldWidth_(Tape()->numAdj_) { Tape()->numAdj_ = width; }
        ~WidthOverride_() { Tape()->numAdj_ = oldWidth_; }
        WidthOverride_(const WidthOverride_&) = delete;
        WidthOverride_& operator=(const WidthOverride_&) = delete;
    };

    Vector_<Number_> RegisterInputs(RecordingScope_* recording, const Vector_<>& values) {
        Vector_<Number_> inputs(values.size());
        for (size_t column = 0; column < values.size(); ++column)
            recording->RegisterInput(inputs[column], values[column]);
        return inputs;
    }

    Matrix_<> ReferenceMatrix(const Vector_<Vector_<double>>& rows) {
        Matrix_<> result(static_cast<int>(rows.size()), rows.empty() ? 0 : static_cast<int>(rows[0].size()), 0.0);
        for (int row = 0; row < result.Rows(); ++row)
            for (int column = 0; column < result.Cols(); ++column)
                result(row, column) = rows[row][column];
        return result;
    }

    void CheckMatrix(const Matrix_<>& actual, const Matrix_<>& expected, double tolerance = 0.0) {
        ASSERT_EQ(actual.Rows(), expected.Rows());
        ASSERT_EQ(actual.Cols(), expected.Cols());
        for (int row = 0; row < actual.Rows(); ++row)
            for (int column = 0; column < actual.Cols(); ++column)
                if (tolerance == 0.0)
                    ASSERT_DOUBLE_EQ(actual(row, column), expected(row, column));
                else
                    ASSERT_NEAR(actual(row, column), expected(row, column), tolerance);
    }

    Matrix_<> DenseReference(RecordingScope_* recording, const Vector_<Number_>& inputs, const Vector_<Number_>& outputs) {
        Matrix_<> result(static_cast<int>(outputs.size()), static_cast<int>(inputs.size()), 0.0);
        for (size_t row = 0; row < outputs.size(); ++row) {
            recording->ClearAdjoints();
            auto output = outputs[row];
            NativeOperations_::SetSeed(output, 1.0);
            recording->Reverse();
            for (int column = 0; column < result.Cols(); ++column)
                result(static_cast<int>(row), column) = NativeOperations_::ReadAdjoint(inputs[column]);
        }
        return result;
    }

    Vector_<Number_> D4Outputs(const Vector_<Number_>& inputs) {
        return {2.0 * inputs[0] + 3.0 * inputs[1], 4.0 * inputs[2], 5.0 * inputs[1] + 6.0 * inputs[3]};
    }

    void PolluteSeeds(Vector_<Number_> numbers, size_t width) {
        for (auto& number : numbers)
            for (size_t lane = 0; lane < width; ++lane)
                NativeOperations_::SetSeed(number, lane % 2 == 0 ? 17.0 : -9.0, lane);
    }
} // namespace

TEST(AADStructuralJacobianTest, TestIndependentNonPrefixMatrixAcrossScalarAndVectorWidths) {
    const auto plan = PlanStructuralJacobian(4, {{0, 1}, {2}, {1, 3}}, {160});
    for (const auto [multi, width] : {std::pair{false, size_t{1}}, {true, size_t{1}}, {true, size_t{2}}, {true, size_t{4}}}) {
        const auto mode = SetNumResultsForAAD(multi, width);
        RecordingScope_ recording;
        Vector_<Number_> inputs(4);
        for (size_t column = 0; column < inputs.size(); ++column)
            recording.RegisterInput(inputs[column], static_cast<double>(column + 1));
        recording.StartRecording();
        const auto bindings = BindStructuralJacobianInputs(&recording, inputs);
        const Vector_<Number_> outputs = {2.0 * inputs[0] + 3.0 * inputs[1], 4.0 * inputs[2], 5.0 * inputs[1] + 6.0 * inputs[3]};
        recording.FinishRecording();
        const auto actual = ExecuteStructuralJacobian(&recording, bindings, plan, inputs, outputs);
        const double expected[3][4] = {{2.0, 3.0, 0.0, 0.0}, {0.0, 0.0, 4.0, 0.0}, {0.0, 5.0, 0.0, 6.0}};
        ASSERT_EQ(actual.Rows(), 3);
        ASSERT_EQ(actual.Cols(), 4);
        for (int row = 0; row < 3; ++row)
            for (int column = 0; column < 4; ++column)
                ASSERT_DOUBLE_EQ(actual(row, column), expected[row][column]);
    }
}

TEST(AADStructuralJacobianTest, TestRepeatedRequestsClearEveryLaneAndKeepGraphFixed) {
    const auto plan = PlanStructuralJacobian(4, {{0, 1}, {2}, {1, 3}});
    const auto expected = ReferenceMatrix({{2.0, 3.0, 0.0, 0.0}, {0.0, 0.0, 4.0, 0.0}, {0.0, 5.0, 0.0, 6.0}});
    for (const auto [multi, width] : {std::pair{false, size_t{1}}, {true, size_t{2}}, {true, size_t{4}}}) {
        const auto mode = SetNumResultsForAAD(multi, width);
        RecordingScope_ recording;
        const auto inputs = RegisterInputs(&recording, {1.0, 2.0, 3.0, 4.0});
        recording.StartRecording();
        const auto bindings = BindStructuralJacobianInputs(&recording, inputs);
        const auto copied = bindings;
        ASSERT_EQ(copied.Inputs(), 4);
        ASSERT_EQ(copied.Width(), width);
        ASSERT_EQ(copied.VectorAdjoints(), multi);
        const auto outputs = D4Outputs(inputs);
        recording.FinishRecording();
        const auto nodes = Tape()->nodes_.OccupiedSlots();
        for (int repeat = 0; repeat < 3; ++repeat) {
            PolluteSeeds(inputs, width);
            PolluteSeeds(outputs, width);
            const auto actual = ExecuteStructuralJacobian(&recording, copied, plan, inputs, outputs);
            ASSERT_NO_FATAL_FAILURE(CheckMatrix(actual, expected));
            ASSERT_EQ(Tape()->nodes_.OccupiedSlots(), nodes);
            ASSERT_EQ(Tape()->numAdj_, width);
            ASSERT_EQ(Tape()->multi_, multi);
            for (size_t lane = 2; lane < width; ++lane)
                for (const auto& input : inputs)
                    ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(input, lane), 0.0);
        }
    }
}

TEST(AADStructuralJacobianTest, TestZeroToNonzeroAliasesDirectIntermediateAndConstantOutputs) {
    const auto plan = PlanStructuralJacobian(6, {{0, 1}, {2}, {0, 1}, {3}, {}, {4, 5}});
    ASSERT_EQ(plan.ColorCount(), 2);
    for (const auto& values :
         Vector_<Vector_<double>>{{0.0, 0.0, 2.0, 3.0, 0.0, 0.0}, {2.0, 3.0, 4.0, 5.0, 6.0, 7.0}, {-2.0, -1.0, 0.0, -3.0, 4.0, -5.0}}) {
        for (const auto [multi, width] : {std::pair{false, size_t{1}}, {true, size_t{2}}, {true, size_t{4}}}) {
            const auto mode = SetNumResultsForAAD(multi, width);
            RecordingScope_ recording;
            const auto inputs = RegisterInputs(&recording, values);
            recording.StartRecording();
            const auto bindings = BindStructuralJacobianInputs(&recording, inputs);
            const Number_ product = inputs[0] * inputs[1];
            const Number_ intermediate = 3.0 * inputs[2] + 2.0;
            const Number_ terminal = inputs[4] * inputs[5];
            const Vector_<Number_> outputs = {product, intermediate, product, inputs[3], Number_(5.0), terminal};
            recording.FinishRecording();
            const auto expected = ReferenceMatrix({{values[1], values[0], 0.0, 0.0, 0.0, 0.0},
                                                   {0.0, 0.0, 3.0, 0.0, 0.0, 0.0},
                                                   {values[1], values[0], 0.0, 0.0, 0.0, 0.0},
                                                   {0.0, 0.0, 0.0, 1.0, 0.0, 0.0},
                                                   {0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
                                                   {0.0, 0.0, 0.0, 0.0, values[5], values[4]}});
            const auto dense = DenseReference(&recording, inputs, outputs);
            const auto actual = ExecuteStructuralJacobian(&recording, bindings, plan, inputs, outputs);
            ASSERT_NO_FATAL_FAILURE(CheckMatrix(dense, expected));
            ASSERT_NO_FATAL_FAILURE(CheckMatrix(actual, expected));
            ASSERT_EQ(plan.RowSupport(0), (Vector_<size_t>{0, 1}));
        }
    }
}

TEST(AADStructuralJacobianTest, TestRecordedSolveCompositionAndPartialColorBlock) {
    const auto plan = PlanStructuralJacobian(4, {{0, 1}, {0, 1}, {2}, {0, 1}, {3}});
    ASSERT_EQ(plan.ColorCount(), 3);
    SquareMatrix_<> matrix(2, 0.0);
    matrix(0, 0) = 2.0;
    matrix(0, 1) = matrix(1, 0) = 1.0;
    matrix(1, 1) = 3.0;
    const auto expected =
        ReferenceMatrix({{0.6, -0.2, 0.0, 0.0}, {-0.2, 0.4, 0.0, 0.0}, {0.0, 0.0, 2.0, 0.0}, {0.6, -0.2, 0.0, 0.0}, {0.0, 0.0, 0.0, 1.0}});
    for (const auto [multi, width] : {std::pair{false, size_t{1}}, {true, size_t{2}}, {true, size_t{4}}}) {
        const auto mode = SetNumResultsForAAD(multi, width);
        RecordingScope_ recording;
        const auto inputs = RegisterInputs(&recording, {4.0, 7.0, 2.0, -3.0});
        recording.StartRecording();
        const auto bindings = BindStructuralJacobianInputs(&recording, inputs);
        Matrix_<Number_> rhs(2, 1);
        rhs(0, 0) = inputs[0];
        rhs(1, 0) = inputs[1];
        const auto solved = LinearSolve(&recording, matrix, rhs);
        ASSERT_THROW(static_cast<void>(BindStructuralJacobianInputs(&recording, {solved(0, 0)})), Exception_);
        const Vector_<Number_> outputs = {solved(0, 0), solved(1, 0), 2.0 * inputs[2], solved(0, 0), inputs[3]};
        recording.FinishRecording();
        ASSERT_GT(Tape()->ReverseEventCount(), 0);
        const auto dense = DenseReference(&recording, inputs, outputs);
        PolluteSeeds(inputs, width);
        PolluteSeeds(outputs, width);
        const auto actual = ExecuteStructuralJacobian(&recording, bindings, plan, inputs, outputs);
        ASSERT_NO_FATAL_FAILURE(CheckMatrix(dense, expected, 1.0e-10));
        ASSERT_NO_FATAL_FAILURE(CheckMatrix(actual, expected, 1.0e-10));
    }
}

TEST(AADStructuralJacobianTest, TestConservativeConstantAliasesAccumulateInOneColor) {
    const auto plan = PlanStructuralJacobian(2, {{0}, {1}});
    ASSERT_EQ(plan.ColorCount(), 1);
    for (const auto [multi, width] : {std::pair{false, size_t{1}}, {true, size_t{2}}}) {
        const auto mode = SetNumResultsForAAD(multi, width);
        RecordingScope_ recording;
        const auto inputs = RegisterInputs(&recording, {2.0, 3.0});
        recording.StartRecording();
        const auto bindings = BindStructuralJacobianInputs(&recording, inputs);
        const Number_ constant(5.0);
        const Vector_<Number_> outputs = {constant, constant};
        recording.FinishRecording();
        PolluteSeeds(outputs, width);
        const auto actual = ExecuteStructuralJacobian(&recording, bindings, plan, inputs, outputs);
        ASSERT_NO_FATAL_FAILURE(CheckMatrix(actual, Matrix_<>(2, 2, 0.0)));
        ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(constant), 2.0);
        for (size_t lane = 1; lane < width; ++lane)
            ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(constant, lane), 0.0);
    }
}

TEST(AADStructuralJacobianTest, TestEmptyAxesAndEmptyRowsKeepShapeWithoutReverse) {
    for (const auto [multi, width] : {std::pair{false, size_t{1}}, {true, size_t{3}}}) {
        const auto mode = SetNumResultsForAAD(multi, width);
        {
            RecordingScope_ recording;
            const auto inputs = RegisterInputs(&recording, {2.0, 3.0});
            recording.StartRecording();
            const auto bindings = BindStructuralJacobianInputs(&recording, inputs);
            recording.FinishRecording();
            PolluteSeeds(inputs, width);
            const auto plan = PlanStructuralJacobian(2, {}, {0});
            const auto actual = ExecuteStructuralJacobian(&recording, bindings, plan, inputs, {});
            ASSERT_EQ(actual.Rows(), 0);
            ASSERT_EQ(actual.Cols(), 2);
            ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(inputs[0]), 17.0);
        }
        {
            RecordingScope_ recording;
            recording.StartRecording();
            const auto bindings = BindStructuralJacobianInputs(&recording, {});
            const Vector_<Number_> outputs = {Number_(5.0), Number_(-2.0), Number_(0.0)};
            recording.FinishRecording();
            PolluteSeeds(outputs, width);
            const auto plan = PlanStructuralJacobian(0, {{}, {}, {}}, {0});
            const auto actual = ExecuteStructuralJacobian(&recording, bindings, plan, {}, outputs);
            ASSERT_EQ(actual.Rows(), 3);
            ASSERT_EQ(actual.Cols(), 0);
            ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(outputs[0]), 17.0);
        }
        {
            RecordingScope_ recording;
            const auto inputs = RegisterInputs(&recording, {2.0, 3.0});
            recording.StartRecording();
            const auto bindings = BindStructuralJacobianInputs(&recording, inputs);
            const Vector_<Number_> outputs = {Number_(5.0), Number_(-2.0), Number_(0.0)};
            recording.FinishRecording();
            const auto plan = PlanStructuralJacobian(2, {{}, {}, {}}, {48});
            const auto actual = ExecuteStructuralJacobian(&recording, bindings, plan, inputs, outputs);
            ASSERT_NO_FATAL_FAILURE(CheckMatrix(actual, Matrix_<>(3, 2, 0.0)));
            ASSERT_EQ(plan.ColorCount(), 0);
        }
    }
}

TEST(AADStructuralJacobianTest, TestInvalidBindingRequestsLeaveRecordingUsable) {
    const auto mode = SetNumResultsForAAD(true, 2);
    RecordingScope_ recording;
    const auto inputs = RegisterInputs(&recording, {2.0, 3.0});
    ASSERT_THROW(static_cast<void>(BindStructuralJacobianInputs(nullptr, inputs)), Exception_);
    ASSERT_THROW(static_cast<void>(BindStructuralJacobianInputs(&recording, inputs)), Exception_);
    recording.StartRecording();
    ASSERT_THROW(static_cast<void>(BindStructuralJacobianInputs(&recording, {Number_()})), Exception_);
    ASSERT_THROW(static_cast<void>(BindStructuralJacobianInputs(&recording, {inputs[0], inputs[0]})), Exception_);
    const Number_ invalid(std::numeric_limits<double>::quiet_NaN());
    ASSERT_THROW(static_cast<void>(BindStructuralJacobianInputs(&recording, {invalid})), Exception_);
    const auto bindings = BindStructuralJacobianInputs(&recording, inputs);
    const Vector_<Number_> outputs = {inputs[0] * inputs[1], inputs[0] + inputs[1]};
    ASSERT_THROW(static_cast<void>(BindStructuralJacobianInputs(&recording, inputs)), Exception_);
    const auto plan = PlanStructuralJacobian(2, {{0, 1}, {0, 1}});
    PolluteSeeds(inputs, 2);
    ASSERT_THROW(static_cast<void>(ExecuteStructuralJacobian(&recording, bindings, plan, inputs, outputs)), Exception_);
    ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(inputs[0]), 17.0);
    recording.FinishRecording();
    const auto actual = ExecuteStructuralJacobian(&recording, bindings, plan, inputs, outputs);
    ASSERT_NO_FATAL_FAILURE(CheckMatrix(actual, ReferenceMatrix({{3.0, 2.0}, {1.0, 1.0}})));
}

TEST(AADStructuralJacobianTest, TestInvalidCountsSlotsAndOrderPreserveSeedsThenRecover) {
    const auto mode = SetNumResultsForAAD(true, 2);
    RecordingScope_ recording;
    const auto inputs = RegisterInputs(&recording, {1.0, 2.0, 3.0, 4.0});
    recording.StartRecording();
    const auto bindings = BindStructuralJacobianInputs(&recording, inputs);
    const auto subset = BindStructuralJacobianInputs(&recording, {inputs[0], inputs[1]});
    const auto outputs = D4Outputs(inputs);
    const Number_ unrelated(11.0);
    const Number_ nonfinite(std::numeric_limits<double>::infinity());
    recording.FinishRecording();
    const auto plan = PlanStructuralJacobian(4, {{0, 1}, {2}, {1, 3}});
    PolluteSeeds(inputs, 2);
    ASSERT_THROW(static_cast<void>(ExecuteStructuralJacobian(nullptr, bindings, plan, inputs, outputs)), Exception_);
    ASSERT_THROW(static_cast<void>(ExecuteStructuralJacobian(&recording, subset, plan, inputs, outputs)), Exception_);
    ASSERT_THROW(static_cast<void>(ExecuteStructuralJacobian(&recording, bindings, plan, {inputs[0]}, outputs)), Exception_);
    ASSERT_THROW(static_cast<void>(ExecuteStructuralJacobian(&recording, bindings, plan, inputs, {outputs[0]})), Exception_);
    const Vector_<Number_> reordered = {inputs[1], inputs[0], inputs[2], inputs[3]};
    ASSERT_THROW(static_cast<void>(ExecuteStructuralJacobian(&recording, bindings, plan, reordered, outputs)), Exception_);
    const Vector_<Number_> replaced = {unrelated, inputs[1], inputs[2], inputs[3]};
    ASSERT_THROW(static_cast<void>(ExecuteStructuralJacobian(&recording, bindings, plan, replaced, outputs)), Exception_);
    ASSERT_THROW(static_cast<void>(ExecuteStructuralJacobian(&recording, bindings, plan, inputs, {outputs[0], Number_(), outputs[2]})), Exception_);
    ASSERT_THROW(static_cast<void>(ExecuteStructuralJacobian(&recording, bindings, plan, inputs, {outputs[0], nonfinite, outputs[2]})), Exception_);
    const auto wrongPlan = PlanStructuralJacobian(3, {{0, 1}, {2}, {1, 2}});
    ASSERT_THROW(static_cast<void>(ExecuteStructuralJacobian(&recording, bindings, wrongPlan, inputs, outputs)), Exception_);
    ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(inputs[0]), 17.0);
    const auto actual = ExecuteStructuralJacobian(&recording, bindings, plan, inputs, outputs);
    ASSERT_NO_FATAL_FAILURE(CheckMatrix(actual, ReferenceMatrix({{2.0, 3.0, 0.0, 0.0}, {0.0, 0.0, 4.0, 0.0}, {0.0, 5.0, 0.0, 6.0}})));
}

TEST(AADStructuralJacobianTest, TestForeignAndClosedBindingsRejectBeforeSlotAccess) {
    const auto mode = SetNumResultsForAAD(false, 1);
    const auto plan = PlanStructuralJacobian(2, {{0, 1}});
    std::optional<NativeStructuralInputs_> previous;
    {
        RecordingScope_ recording;
        const auto inputs = RegisterInputs(&recording, {2.0, 3.0});
        recording.StartRecording();
        previous.emplace(BindStructuralJacobianInputs(&recording, inputs));
        const Vector_<Number_> outputs = {inputs[0] * inputs[1]};
        recording.FinishRecording();
        recording.Close();
        ASSERT_THROW(static_cast<void>(ExecuteStructuralJacobian(&recording, *previous, plan, inputs, outputs)), Exception_);
    }
    {
        RecordingScope_ recording;
        const auto inputs = RegisterInputs(&recording, {3.0, 4.0});
        recording.StartRecording();
        const auto current = BindStructuralJacobianInputs(&recording, inputs);
        const Vector_<Number_> outputs = {inputs[0] * inputs[1]};
        recording.FinishRecording();
        PolluteSeeds(inputs, 1);
        ASSERT_THROW(static_cast<void>(ExecuteStructuralJacobian(&recording, *previous, plan, inputs, outputs)), Exception_);
        ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(inputs[0]), 17.0);
        auto wrongThread = std::async(std::launch::async, [&] {
            try {
                static_cast<void>(ExecuteStructuralJacobian(&recording, current, plan, inputs, outputs));
                return false;
            } catch (const Exception_&) {
                return true;
            }
        });
        ASSERT_TRUE(wrongThread.get());
        const auto actual = ExecuteStructuralJacobian(&recording, current, plan, inputs, outputs);
        ASSERT_NO_FATAL_FAILURE(CheckMatrix(actual, ReferenceMatrix({{4.0, 3.0}})));
    }
}

TEST(AADStructuralJacobianTest, TestNonfiniteHarvestThenHealthyRequestClearsTheSameGraph) {
    const auto plan = PlanStructuralJacobian(1, {{0}});
    for (const auto [multi, width] : {std::pair{false, size_t{1}}, {true, size_t{2}}}) {
        const auto mode = SetNumResultsForAAD(multi, width);
        RecordingScope_ recording;
        const auto inputs = RegisterInputs(&recording, {0.0});
        recording.StartRecording();
        const auto bindings = BindStructuralJacobianInputs(&recording, inputs);
        const Number_ nonfinite = sqrt(inputs[0]);
        const Number_ healthy = 2.0 * inputs[0];
        recording.FinishRecording();
        ASSERT_THROW(static_cast<void>(ExecuteStructuralJacobian(&recording, bindings, plan, inputs, {nonfinite})), Exception_);
        const auto actual = ExecuteStructuralJacobian(&recording, bindings, plan, inputs, {healthy});
        ASSERT_NO_FATAL_FAILURE(CheckMatrix(actual, ReferenceMatrix({{2.0}})));
        for (size_t lane = 1; lane < width; ++lane)
            ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(inputs[0], lane), 0.0);
    }
}

TEST(AADStructuralJacobianTest, TestDetachedResultsAndConcurrentIndependentScopes) {
    const auto plan = PlanStructuralJacobian(4, {{0, 1}, {2}, {1, 3}});
    Vector_<std::future<Matrix_<>>> futures;
    for (int worker = 0; worker < 4; ++worker)
        futures.push_back(std::async(std::launch::async, [&, worker] {
            const auto mode = SetNumResultsForAAD(worker % 2 != 0, worker % 2 != 0 ? 4 : 1);
            RecordingScope_ recording;
            const auto inputs = RegisterInputs(&recording, {static_cast<double>(worker), 2.0, 3.0, 4.0});
            recording.StartRecording();
            const auto bindings = BindStructuralJacobianInputs(&recording, inputs);
            const auto outputs = D4Outputs(inputs);
            recording.FinishRecording();
            return ExecuteStructuralJacobian(&recording, bindings, plan, inputs, outputs);
        }));
    const auto expected = ReferenceMatrix({{2.0, 3.0, 0.0, 0.0}, {0.0, 0.0, 4.0, 0.0}, {0.0, 5.0, 0.0, 6.0}});
    Vector_<Matrix_<>> results;
    for (auto& future : futures) {
        results.push_back(future.get());
        ASSERT_NO_FATAL_FAILURE(CheckMatrix(results.back(), expected));
    }
    results[0](0, 0) = 999.0;
    ASSERT_DOUBLE_EQ(results[1](0, 0), 2.0);
}

TEST(AADStructuralJacobianTest, TestMalformedAndChangedModeRejectBeforeSlotAccess) {
    {
        const auto mode = SetNumResultsForAAD(false, 2);
        RecordingScope_ recording;
        recording.StartRecording();
        ASSERT_THROW(static_cast<void>(BindStructuralJacobianInputs(&recording, {})), Exception_);
        ASSERT_NO_THROW(recording.Close());
    }
    {
        const auto mode = SetNumResultsForAAD(true, 2);
        RecordingScope_ recording;
        const auto inputs = RegisterInputs(&recording, {3.0});
        recording.StartRecording();
        const auto bindings = BindStructuralJacobianInputs(&recording, inputs);
        const Vector_<Number_> outputs = {inputs[0] * inputs[0]};
        recording.FinishRecording();
        const auto plan = PlanStructuralJacobian(1, {{0}});
        PolluteSeeds(inputs, 2);
        {
            const WidthOverride_ changed(3);
            ASSERT_THROW(static_cast<void>(ExecuteStructuralJacobian(&recording, bindings, plan, inputs, outputs)), Exception_);
        }
        ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(inputs[0]), 17.0);
        const auto actual = ExecuteStructuralJacobian(&recording, bindings, plan, inputs, outputs);
        ASSERT_NO_FATAL_FAILURE(CheckMatrix(actual, ReferenceMatrix({{6.0}})));
    }
}

TEST(AADStructuralJacobianTest, TestReboundInputRequiresFreshBindingBeforeGraphConstruction) {
    const auto mode = SetNumResultsForAAD(false, 1);
    RecordingScope_ recording;
    auto inputs = RegisterInputs(&recording, {2.0});
    recording.StartRecording();
    const auto previous = BindStructuralJacobianInputs(&recording, inputs);
    inputs[0] = 7.0;
    const auto current = BindStructuralJacobianInputs(&recording, inputs);
    const Vector_<Number_> outputs = {inputs[0] * inputs[0]};
    recording.FinishRecording();
    const auto plan = PlanStructuralJacobian(1, {{0}});
    PolluteSeeds(inputs, 1);
    ASSERT_THROW(static_cast<void>(ExecuteStructuralJacobian(&recording, previous, plan, inputs, outputs)), Exception_);
    ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(inputs[0]), 17.0);
    const auto actual = ExecuteStructuralJacobian(&recording, current, plan, inputs, outputs);
    ASSERT_NO_FATAL_FAILURE(CheckMatrix(actual, ReferenceMatrix({{14.0}})));
}

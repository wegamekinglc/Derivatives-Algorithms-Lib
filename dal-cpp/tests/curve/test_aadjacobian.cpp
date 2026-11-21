#include <gtest/gtest.h>

#include <dal/curve/aadjacobian.hpp>
#include <dal/curve/curveparameterization.hpp>
#include <dal/curve/tapeguard.hpp>
#include <dal/math/aad/statistics.hpp>
#include <dal/math/aad/tapecapacity.hpp>

using namespace Dal;

TEST(AadJacobianTest, TestHarvestsAsymmetricRowsAndCleansLeaves) {
    auto* tape = AAD::Tape();
    AAD::Clear(*tape);
    auto independents = RegisterCurveParameters(Vector_<>{1.0, 2.0, 3.0});
    AAD::NewRecording(*tape);

    Vector_<AAD::Number_> residuals(2);
    residuals[0] = 2.0 * independents[0] + independents[2];
    residuals[1] = independents[0] * independents[1] + 3.0 * independents[2];

    const Matrix_<> first = HarvestCurveJacobian(*tape, independents, residuals);
    ASSERT_EQ(first.Rows(), 2);
    ASSERT_EQ(first.Cols(), 3);
    ASSERT_DOUBLE_EQ(first(0, 0), 2.0);
    ASSERT_DOUBLE_EQ(first(0, 1), 0.0);
    ASSERT_DOUBLE_EQ(first(0, 2), 1.0);
    ASSERT_DOUBLE_EQ(first(1, 0), 2.0);
    ASSERT_DOUBLE_EQ(first(1, 1), 1.0);
    ASSERT_DOUBLE_EQ(first(1, 2), 3.0);

    const Matrix_<> repeated = HarvestCurveJacobian(*tape, independents, residuals);
    for (int row = 0; row < first.Rows(); ++row)
        for (int column = 0; column < first.Cols(); ++column)
            ASSERT_DOUBLE_EQ(repeated(row, column), first(row, column));
    AAD::Clear(*tape);
}

TEST(AadJacobianTest, TestShortenedRowsStillCleanUnharvestedLeaves) {
    auto* tape = AAD::Tape();
    AAD::Clear(*tape);
    auto independents = RegisterCurveParameters(Vector_<>{1.0, 2.0, 3.0});
    AAD::NewRecording(*tape);

    Vector_<AAD::Number_> residuals(2);
    residuals[0] = independents[0] + independents[2];
    residuals[1] = independents[1] + 2.0 * independents[2];
    const Matrix_<> shortened = HarvestCurveJacobian(*tape, independents, residuals, Vector_<int>{1, 3});
    ASSERT_DOUBLE_EQ(shortened(0, 0), 1.0);
    ASSERT_DOUBLE_EQ(shortened(0, 1), 0.0);
    ASSERT_DOUBLE_EQ(shortened(0, 2), 0.0);
    ASSERT_DOUBLE_EQ(shortened(1, 0), 0.0);
    ASSERT_DOUBLE_EQ(shortened(1, 1), 1.0);
    ASSERT_DOUBLE_EQ(shortened(1, 2), 2.0);

    const Matrix_<> full = HarvestCurveJacobian(*tape, independents, residuals);
    ASSERT_DOUBLE_EQ(full(0, 2), 1.0);
    ASSERT_DOUBLE_EQ(full(1, 2), 2.0);
    AAD::Clear(*tape);
}

TEST(AadJacobianTest, TestVectorHarvestHandlesAliasesShortRowsAndTail) {
    auto* tape = AAD::Tape();
    AAD::Clear(*tape);
    const auto mode = AAD::SetNumResultsForAAD(true, 2);
    auto inputs = RegisterCurveParameters(Vector_<>{2., 3.});
    AAD::NewRecording(*tape);
    const AAD::Number_ product = inputs[0] * inputs[1];
    Vector_<AAD::Number_> outputs{product, inputs[1], product};
    Vector_<AAD::Number_> columns{inputs[0], inputs[1], inputs[0]};
    ASSERT_THROW(HarvestCurveJacobian(*tape, columns, outputs, {3, 1, 4}), Exception_);
    const auto shortRows = HarvestCurveJacobian(*tape, columns, outputs, {0, 2, 3});
    ASSERT_DOUBLE_EQ(shortRows(0, 0), 0.);
    ASSERT_DOUBLE_EQ(shortRows(1, 1), 1.);
    ASSERT_DOUBLE_EQ(shortRows(2, 2), 3.);
    const auto full = HarvestCurveJacobian(*tape, columns, outputs);
    for (const auto row : {0, 2}) {
        ASSERT_DOUBLE_EQ(full(row, 0), 3.);
        ASSERT_DOUBLE_EQ(full(row, 1), 2.);
        ASSERT_DOUBLE_EQ(full(row, 2), 3.);
    }
    ASSERT_DOUBLE_EQ(full(1, 0), 0.);
    ASSERT_DOUBLE_EQ(full(1, 1), 1.);
    AAD::Clear(*tape);
}

TEST(AadJacobianTest, TestVectorBudgetRejectsGrowthAndAllowsAnotherRecording) {
    auto* tape = AAD::Tape();
    AAD::Clear(*tape);
    const auto limit = AAD::MeasureTape(*tape).capacityBytes_;
    AAD::TapeCapacityBudget_ budget(limit);
    AAD::TapeCapacityScope_ capacity(&budget);
    const auto mode = AAD::SetNumResultsForAAD(true, 8);
    const auto record = [&](size_t steps) {
        TapeGuard_ guard(tape);
        auto inputs = RegisterCurveParameters({1.});
        guard.recording_.StartRecording();
        auto value = inputs.front();
        for (size_t i = 0; i < steps; ++i)
            value = value + inputs.front();
        Vector_<AAD::Number_> outputs{value};
        guard.recording_.FinishRecording();
        return HarvestCurveJacobian(*tape, inputs, outputs);
    };
    ASSERT_THROW(record(AAD::ADJ_SIZE / 8), Exception_);
    ASSERT_LE(budget.PeakCapacityBytes(), limit);
    ASSERT_DOUBLE_EQ(record(3)(0, 0), 4.);
    capacity.Close();
}

TEST(AadJacobianTest, TestTapeGuardRewindsDirtyRecordingAfterException) {
    auto* tape = AAD::Tape();
    AAD::Clear(*tape);

    const auto recordAndThrow = [&]() {
        TapeGuard_ guard(tape);
        auto independents = RegisterCurveParameters(Vector_<>{2.0, 3.0});
        AAD::NewRecording(*tape);
        Vector_<AAD::Number_> residuals(1);
        residuals[0] = independents[0] * independents[1];
        const Matrix_<> jacobian = HarvestCurveJacobian(*tape, independents, residuals);
        ASSERT_DOUBLE_EQ(jacobian(0, 0), 3.0);
        ASSERT_DOUBLE_EQ(jacobian(0, 1), 2.0);
        AAD::Adjoint(independents[0]) = 123.0;
        THROW("forced unwind after a dirty AAD recording");
    };
    ASSERT_THROW(recordAndThrow(), Exception_);

    {
        TapeGuard_ guard(tape);
        auto independents = RegisterCurveParameters(Vector_<>{5.0, 7.0});
        AAD::NewRecording(*tape);
        Vector_<AAD::Number_> residuals(1);
        residuals[0] = independents[0] + 2.0 * independents[1];
        const Matrix_<> jacobian = HarvestCurveJacobian(*tape, independents, residuals);
        ASSERT_DOUBLE_EQ(jacobian(0, 0), 1.0);
        ASSERT_DOUBLE_EQ(jacobian(0, 1), 2.0);
    }
    AAD::Clear(*tape);
}

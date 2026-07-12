//
// Created by dal-implementer on 2026/7/12.
//

#include <gtest/gtest.h>

#include <dal/curve/aadjacobian.hpp>
#include <dal/curve/curveparameterization.hpp>
#include <dal/curve/tapeguard.hpp>

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

//
// Created by Codex on 2026/10/7.
//

#include <gtest/gtest.h>

#include <cmath>
#include <dal/math/aad/linearsolve.hpp>
#include <dal/math/aad/native.hpp>
#include <dal/math/matrix/linearsolvepullback.hpp>

using namespace Dal;
using namespace Dal::AAD;

namespace {
    template <class T_> Matrix_<T_> Augment(const SquareMatrix_<T_>& matrix, const Matrix_<T_>& rhs) {
        const int size = matrix.Rows();
        Matrix_<T_> augmented(size, size + rhs.Cols());
        for (int row = 0; row < size; ++row) {
            for (int column = 0; column < size; ++column)
                augmented(row, column) = matrix(row, column);
            for (int column = 0; column < rhs.Cols(); ++column)
                augmented(row, size + column) = rhs(row, column);
        }
        return augmented;
    }

    template <class T_> void ReduceColumn(int pivot, Matrix_<T_>* augmented) {
        int best = pivot;
        for (int row = pivot + 1; row < augmented->Rows(); ++row)
            if (std::abs(static_cast<double>((*augmented)(row, pivot))) > std::abs(static_cast<double>((*augmented)(best, pivot))))
                best = row;
        for (int column = 0; column < augmented->Cols(); ++column)
            std::swap((*augmented)(pivot, column), (*augmented)(best, column));
        const T_ divisor = (*augmented)(pivot, pivot);
        for (int column = pivot; column < augmented->Cols(); ++column)
            (*augmented)(pivot, column) = (*augmented)(pivot, column) / divisor;
        for (int row = 0; row < augmented->Rows(); ++row) {
            if (row == pivot)
                continue;
            const T_ factor = (*augmented)(row, pivot);
            for (int column = pivot; column < augmented->Cols(); ++column)
                (*augmented)(row, column) = (*augmented)(row, column) - factor * (*augmented)(pivot, column);
        }
    }

    template <class T_> Matrix_<T_> GaussJordan(const SquareMatrix_<T_>& matrix, const Matrix_<T_>& rhs) {
        const int size = matrix.Rows();
        auto augmented = Augment(matrix, rhs);
        for (int pivot = 0; pivot < size; ++pivot)
            ReduceColumn(pivot, &augmented);
        Matrix_<T_> result(size, rhs.Cols());
        for (int row = 0; row < size; ++row)
            for (int column = 0; column < rhs.Cols(); ++column)
                result(row, column) = augmented(row, size + column);
        return result;
    }

    template <class T_> T_ Objective(const Matrix_<T_>& solution) {
        const double weights[3][2] = {{2.0, -1.0}, {-0.5, 0.0}, {3.0, 4.0}};
        T_ result(0.0);
        for (int row = 0; row < solution.Rows(); ++row)
            for (int column = 0; column < solution.Cols(); ++column)
                result = result + 0.5 * weights[row][column] * solution(row, column) * solution(row, column);
        return result;
    }

    LinearSolveAdjoints_ Gradient(const SquareMatrix_<>& matrix, const Matrix_<>& rhs, bool recordedSolve) {
        Clear(*Tape());
        RecordingScope_ scope;
        SquareMatrix_<Number_> activeMatrix(matrix.Rows());
        Matrix_<Number_> activeRhs(rhs.Rows(), rhs.Cols());
        for (int row = 0; row < matrix.Rows(); ++row) {
            for (int column = 0; column < matrix.Cols(); ++column)
                scope.RegisterInput(activeMatrix(row, column), matrix(row, column));
            for (int column = 0; column < rhs.Cols(); ++column)
                scope.RegisterInput(activeRhs(row, column), rhs(row, column));
        }
        scope.StartRecording();
        const auto solution = recordedSolve ? LinearSolve(&scope, activeMatrix, activeRhs) : GaussJordan(activeMatrix, activeRhs);
        Number_ objective = Objective(solution);
        scope.FinishRecording();
        scope.ClearAdjoints();
        NativeOperations_::SetSeed(objective, 1.0);
        scope.Reverse();
        LinearSolveAdjoints_ gradient{SquareMatrix_<>(matrix.Rows()), Matrix_<>(rhs.Rows(), rhs.Cols())};
        for (int row = 0; row < matrix.Rows(); ++row) {
            for (int column = 0; column < matrix.Cols(); ++column)
                gradient.matrix_(row, column) = NativeOperations_::ReadAdjoint(activeMatrix(row, column));
            for (int column = 0; column < rhs.Cols(); ++column)
                gradient.rhs_(row, column) = NativeOperations_::ReadAdjoint(activeRhs(row, column));
        }
        scope.Close();
        Clear(*Tape());
        return gradient;
    }

    void AssertCentralDifferences(const SquareMatrix_<>& matrix, const Matrix_<>& rhs, const LinearSolveAdjoints_& gradient, double step) {
        for (int row = 0; row < matrix.Rows(); ++row) {
            for (int column = 0; column < matrix.Cols(); ++column) {
                auto plus = matrix, minus = matrix;
                plus(row, column) += step;
                minus(row, column) -= step;
                const double difference = (Objective(GaussJordan(plus, rhs)) - Objective(GaussJordan(minus, rhs))) / (2.0 * step);
                ASSERT_NEAR(gradient.matrix_(row, column), difference, 2e-8 * (1.0 + std::abs(difference))) << row << "," << column;
            }
            for (int column = 0; column < rhs.Cols(); ++column) {
                auto plus = rhs, minus = rhs;
                plus(row, column) += step;
                minus(row, column) -= step;
                const double difference = (Objective(GaussJordan(matrix, plus)) - Objective(GaussJordan(matrix, minus))) / (2.0 * step);
                ASSERT_NEAR(gradient.rhs_(row, column), difference, 2e-8 * (1.0 + std::abs(difference))) << row << "," << column;
            }
        }
    }
} // namespace

TEST(AADLinearSolveTest, TestPivotedThreeByThreeMatchesScalarAadAndThreeDifferenceSteps) {
    SquareMatrix_<> matrix(3);
    Matrix_<> rhs(3, 2);
    const double matrixEntries[3][3] = {{0.0, 2.0, 1.0}, {1.0, 0.0, 3.0}, {4.0, 1.0, 0.0}};
    const double rhsEntries[3][2] = {{3.0, 4.0}, {-2.0, 7.0}, {6.0, -7.5}};
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column)
            matrix(row, column) = matrixEntries[row][column];
        for (int column = 0; column < 2; ++column)
            rhs(row, column) = rhsEntries[row][column];
    }
    const auto recorded = Gradient(matrix, rhs, true);
    const auto scalar = Gradient(matrix, rhs, false);
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column)
            ASSERT_NEAR(recorded.matrix_(row, column), scalar.matrix_(row, column), 1e-10);
        for (int column = 0; column < 2; ++column)
            ASSERT_NEAR(recorded.rhs_(row, column), scalar.rhs_(row, column), 1e-10);
    }
    for (double step : {0.5e-5, 1e-5, 2e-5}) {
        SCOPED_TRACE(step);
        AssertCentralDifferences(matrix, rhs, recorded, step);
    }
}

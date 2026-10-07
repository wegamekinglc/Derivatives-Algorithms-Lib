//
// Created by Codex on 2026/10/8.
//

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>

#include <dal/math/aad/linearsolvecoordinates.hpp>
#include <dal/math/aad/native.hpp>
#include <dal/platform/platform.hpp>

using namespace Dal;
using namespace Dal::AAD;

namespace {
    template <class T_> T_ Determinant(const SquareMatrix_<T_>& a) {
        return a(0, 0) * (a(1, 1) * a(2, 2) - a(1, 2) * a(2, 1)) - a(0, 1) * (a(1, 0) * a(2, 2) - a(1, 2) * a(2, 0)) +
               a(0, 2) * (a(1, 0) * a(2, 1) - a(1, 1) * a(2, 0));
    }

    template <class T_> SquareMatrix_<T_> ExpandReference(const LinearSolveCoordinates_& layout, const Vector_<T_>& parameters) {
        SquareMatrix_<T_> result(layout.Size(), T_(0.0));
        for (size_t index = 0; index < parameters.size(); ++index) {
            const auto [row, column] = layout.Location(index);
            result(row, column) = parameters[index];
            if (layout.IsSymmetric())
                result(column, row) = parameters[index];
        }
        return result;
    }

    template <class T_> Matrix_<T_> Cramer(const SquareMatrix_<T_>& matrix, const Matrix_<T_>& rhs) {
        const auto determinant = Determinant(matrix);
        Matrix_<T_> result(3, rhs.Cols());
        for (int column = 0; column < rhs.Cols(); ++column)
            for (int variable = 0; variable < 3; ++variable) {
                auto replaced = matrix;
                for (int row = 0; row < 3; ++row)
                    replaced(row, variable) = rhs(row, column);
                result(variable, column) = Determinant(replaced) / determinant;
            }
        return result;
    }

    template <class T_> T_ Objective(const Matrix_<T_>& solution, const Matrix_<>& weights) {
        T_ result(0.0);
        for (int row = 0; row < solution.Rows(); ++row)
            for (int column = 0; column < solution.Cols(); ++column)
                result = result + weights(row, column) * solution(row, column);
        return result;
    }

    struct Sample_ {
        LinearSolveCoordinates_ layout_;
        Vector_<> parameters_;
        Matrix_<> rhs_{3, 2}, weights_{3, 2};

        explicit Sample_(const LinearSolveCoordinates_& layout) : layout_(layout), parameters_(layout.Count()) {
            const double symmetric[3][3] = {{0.0, 2.0, 1.0}, {2.0, -1.0, 3.0}, {1.0, 3.0, 4.0}};
            const double banded[3][3] = {{4.0, 1.0, -0.5}, {2.0, 5.0, 1.0}, {0.75, -1.0, 3.0}};
            const double rhs[3][2] = {{1.0, 4.0}, {2.0, -1.0}, {3.0, 2.0}};
            const double weights[3][2] = {{0.5, -2.0}, {3.0, 0.25}, {-1.0, 2.0}};
            for (size_t index = 0; index < parameters_.size(); ++index) {
                const auto [row, column] = layout.Location(index);
                parameters_[index] = layout.IsSymmetric() ? symmetric[row][column] : banded[row][column];
            }
            for (int row = 0; row < 3; ++row)
                for (int column = 0; column < 2; ++column) {
                    rhs_(row, column) = rhs[row][column];
                    weights_(row, column) = weights[row][column];
                }
        }
    };

    struct Inputs_ {
        Vector_<Number_> parameters_;
        Matrix_<Number_> rhs_{3, 2};

        Inputs_(RecordingScope_* scope, const Sample_& sample) : parameters_(sample.parameters_.size()) {
            for (size_t index = 0; index < parameters_.size(); ++index)
                scope->RegisterInput(parameters_[index], sample.parameters_[index]);
            for (int row = 0; row < 3; ++row)
                for (int column = 0; column < 2; ++column)
                    scope->RegisterInput(rhs_(row, column), sample.rhs_(row, column));
        }
    };

    struct Gradient_ {
        Vector_<> parameters_;
        Matrix_<> rhs_{3, 2}, solution_{3, 2};
    };

    Gradient_ Gradient(const Sample_& sample, bool native) {
        Clear(*Tape());
        RecordingScope_ scope;
        const Inputs_ inputs(&scope, sample);
        scope.StartRecording();
        const auto solution = native ? LinearSolve(&scope, sample.layout_, inputs.parameters_, inputs.rhs_)
                                     : Cramer(ExpandReference(sample.layout_, inputs.parameters_), inputs.rhs_);
        Number_ objective = Objective(solution, sample.weights_);
        scope.FinishRecording();
        scope.ClearAdjoints();
        NativeOperations_::SetSeed(objective, 1.0);
        scope.Reverse();
        Gradient_ result{Vector_<>(sample.parameters_.size())};
        for (size_t index = 0; index < result.parameters_.size(); ++index)
            result.parameters_[index] = NativeOperations_::ReadAdjoint(inputs.parameters_[index]);
        for (int row = 0; row < 3; ++row)
            for (int column = 0; column < 2; ++column) {
                result.rhs_(row, column) = NativeOperations_::ReadAdjoint(inputs.rhs_(row, column));
                result.solution_(row, column) = Value(solution(row, column));
            }
        scope.Close();
        Clear(*Tape());
        return result;
    }

    double ReferenceObjective(const Sample_& sample, const Vector_<>& parameters, const Matrix_<>& rhs) {
        return Objective(Cramer(ExpandReference(sample.layout_, parameters), rhs), sample.weights_);
    }

    void CheckDifferences(const Sample_& sample, const Gradient_& gradient, double step) {
        for (size_t index = 0; index < sample.parameters_.size(); ++index) {
            auto plus = sample.parameters_, minus = sample.parameters_;
            plus[index] += step;
            minus[index] -= step;
            const double difference = (ReferenceObjective(sample, plus, sample.rhs_) - ReferenceObjective(sample, minus, sample.rhs_)) / (2.0 * step);
            ASSERT_NEAR(gradient.parameters_[index], difference, 2e-8 * (1.0 + std::abs(difference))) << index;
        }
        for (int row = 0; row < 3; ++row)
            for (int column = 0; column < 2; ++column) {
                auto plus = sample.rhs_, minus = sample.rhs_;
                plus(row, column) += step;
                minus(row, column) -= step;
                const double difference =
                    (ReferenceObjective(sample, sample.parameters_, plus) - ReferenceObjective(sample, sample.parameters_, minus)) / (2.0 * step);
                ASSERT_NEAR(gradient.rhs_(row, column), difference, 2e-8 * (1.0 + std::abs(difference))) << row << "," << column;
            }
    }

    void CheckReference(const Gradient_& native, const Gradient_& scalar) {
        ASSERT_EQ(native.parameters_.size(), scalar.parameters_.size());
        for (size_t index = 0; index < native.parameters_.size(); ++index)
            ASSERT_NEAR(native.parameters_[index], scalar.parameters_[index], 1e-8);
        for (int row = 0; row < 3; ++row)
            for (int column = 0; column < 2; ++column) {
                ASSERT_NEAR(native.rhs_(row, column), scalar.rhs_(row, column), 1e-8);
                ASSERT_NEAR(native.solution_(row, column), scalar.solution_(row, column), 1e-10);
            }
    }

    Matrix_<Number_> Solve(RecordingScope_* scope, const Sample_& sample, const Inputs_& inputs, int activity) {
        return activity == 1   ? LinearSolve(scope, sample.layout_, inputs.parameters_, sample.rhs_)
               : activity == 2 ? LinearSolve(scope, sample.layout_, sample.parameters_, inputs.rhs_)
                               : LinearSolve(scope, sample.layout_, inputs.parameters_, inputs.rhs_);
    }

    double ChannelWeight(size_t channel) { return channel % 3 == 0 ? 1.0 : channel % 3 == 1 ? -2.0 : 0.0; }

    void Seed(Matrix_<Number_>* solution, const Sample_& sample, const Gradient_& reference, size_t channels, double multiplier) {
        for (int row = 0; row < 3; ++row)
            for (int column = 0; column < 2; ++column) {
                ASSERT_NEAR(Value((*solution)(row, column)), reference.solution_(row, column), 1e-10);
                for (size_t channel = 0; channel < channels; ++channel)
                    NativeOperations_::SetSeed((*solution)(row, column), multiplier * ChannelWeight(channel) * sample.weights_(row, column), channel);
            }
    }

    void CheckChannels(const Number_& input, double expected, size_t channels, double multiplier) {
        for (size_t channel = 0; channel < channels; ++channel)
            ASSERT_NEAR(NativeOperations_::ReadAdjoint(input, channel), multiplier * ChannelWeight(channel) * expected, 1e-8);
    }

    void CheckActivity(const Inputs_& inputs, const Gradient_& reference, int activity, size_t channels, double multiplier) {
        for (size_t index = 0; index < inputs.parameters_.size(); ++index)
            ASSERT_NO_FATAL_FAILURE(
                CheckChannels(inputs.parameters_[index], activity == 2 ? 0.0 : reference.parameters_[index], channels, multiplier));
        for (int row = 0; row < 3; ++row)
            for (int column = 0; column < 2; ++column)
                ASSERT_NO_FATAL_FAILURE(
                    CheckChannels(inputs.rhs_(row, column), activity == 1 ? 0.0 : reference.rhs_(row, column), channels, multiplier));
    }

    void CheckMode(const Sample_& sample, const Gradient_& reference, size_t width, int activity) {
        Clear(*Tape());
        auto mode = SetNumResultsForAAD(width != 0, width == 0 ? 1 : width);
        const size_t channels = std::max(size_t(1), width);
        RecordingScope_ scope;
        const Inputs_ inputs(&scope, sample);
        scope.StartRecording();
        auto solution = Solve(&scope, sample, inputs, activity);
        scope.FinishRecording();
        for (double multiplier : {1.0, -0.5}) {
            scope.ClearAdjoints();
            ASSERT_NO_FATAL_FAILURE(Seed(&solution, sample, reference, channels, multiplier));
            scope.Reverse();
            ASSERT_NO_FATAL_FAILURE(CheckActivity(inputs, reference, activity, channels, multiplier));
            for (const auto& output : solution)
                for (size_t channel = 0; channel < channels; ++channel)
                    ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(output, channel), 0.0);
        }
        scope.Close();
        Clear(*Tape());
    }
} // namespace

TEST(AADLinearSolveTest, TestCoordinateIndependentScalarAadAndThreeDifferenceSteps) {
    for (const auto& layout : {LinearSolveCoordinates_::Symmetric(3), LinearSolveCoordinates_::Banded(3, 1, 2),
                               LinearSolveCoordinates_::Banded(3, 0, 0), LinearSolveCoordinates_::Banded(3, 2, 2)}) {
        SCOPED_TRACE(layout.Count());
        const Sample_ sample(layout);
        const auto native = Gradient(sample, true), scalar = Gradient(sample, false);
        ASSERT_NO_FATAL_FAILURE(CheckReference(native, scalar));
        for (double step : {2e-6, 1e-6, 5e-7})
            ASSERT_NO_FATAL_FAILURE(CheckDifferences(sample, native, step));
    }
}

TEST(AADLinearSolveTest, TestCoordinateThreeActivitiesAndScalarVectorSeedChannels) {
    for (const auto& layout : {LinearSolveCoordinates_::Symmetric(3), LinearSolveCoordinates_::Banded(3, 1, 2)}) {
        const Sample_ sample(layout);
        const auto reference = Gradient(sample, false);
        for (size_t width : {0U, 1U, 4U, 8U}) {
            SCOPED_TRACE(width);
            for (int activity : {1, 2, 3}) {
                SCOPED_TRACE(activity);
                ASSERT_NO_FATAL_FAILURE(CheckMode(sample, reference, width, activity));
            }
        }
    }
}

TEST(AADLinearSolveTest, TestCoordinateTransposeResidualAndDirectionalIdentity) {
    const Sample_ sample(LinearSolveCoordinates_::Banded(3, 1, 2));
    const auto gradient = Gradient(sample, true);
    const auto matrix = ExpandReference(sample.layout_, sample.parameters_);
    Vector_<> direction(sample.parameters_.size());
    for (size_t index = 0; index < direction.size(); ++index)
        direction[index] = 0.03 * (static_cast<int>(index % 3) - 1);
    const auto matrixDirection = ExpandReference(sample.layout_, direction);
    Matrix_<> rhsDirection(3, 2), linearizedRhs(3, 2);
    double inputPairing = 0.0;
    for (size_t index = 0; index < direction.size(); ++index)
        inputPairing += direction[index] * gradient.parameters_[index];
    for (int row = 0; row < 3; ++row)
        for (int column = 0; column < 2; ++column) {
            rhsDirection(row, column) = 0.04 * (row - column + 1);
            linearizedRhs(row, column) = rhsDirection(row, column);
            inputPairing += rhsDirection(row, column) * gradient.rhs_(row, column);
            double transposeProduct = 0.0;
            for (int entry = 0; entry < 3; ++entry) {
                linearizedRhs(row, column) -= matrixDirection(row, entry) * gradient.solution_(entry, column);
                transposeProduct += matrix(entry, row) * gradient.rhs_(entry, column);
            }
            ASSERT_NEAR(transposeProduct, sample.weights_(row, column), 1e-10);
        }
    const auto solutionDirection = Cramer(matrix, linearizedRhs);
    ASSERT_NEAR(Objective(solutionDirection, sample.weights_), inputPairing, 1e-10);
}

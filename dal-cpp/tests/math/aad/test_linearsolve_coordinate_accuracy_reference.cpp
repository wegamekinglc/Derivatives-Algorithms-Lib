//
// Created by Codex on 2026/10/8.
//

#include <gtest/gtest.h>

#include <cmath>

#include <dal/math/aad/linearsolvecoordinateaccuracy.hpp>
#include <dal/math/aad/native.hpp>
#include <dal/platform/platform.hpp>

using Dal::LinearSolveAccuracyPolicy_;
using Dal::LinearSolveCoordinates_;
using Dal::Matrix_;
using Dal::Vector_;
using Dal::AAD::Number_;

namespace {
    struct Sample_ {
        bool symmetric_;
        Vector_<> parameters_;
        Matrix_<> rhs_{2, 3};

        explicit Sample_(bool symmetric) : symmetric_(symmetric), parameters_(symmetric ? Vector_<>{3.0, 0.3, 2.0} : Vector_<>{0.0, 2.0, 3.0, -1.0}) {
            const double values[2][3] = {{1.0, 0.5, 2.0}, {2.0, -1.0, -2.0}};
            for (int row = 0; row < 2; ++row)
                for (int column = 0; column < 3; ++column)
                    rhs_(row, column) = values[row][column];
        }
        LinearSolveCoordinates_ Layout() const {
            return symmetric_ ? LinearSolveCoordinates_::Symmetric(2) : LinearSolveCoordinates_::Banded(2, 1, 1);
        }
    };

    template <class T_> Matrix_<T_> Cramer(bool symmetric, const Vector_<T_>& parameters, const Matrix_<T_>& rhs) {
        const auto& a = parameters[0];
        const auto& b = parameters[1];
        const auto& c = parameters[symmetric ? 1 : 2];
        const auto& d = parameters[symmetric ? 2 : 3];
        const T_ determinant = a * d - b * c;
        Matrix_<T_> solution(2, 3);
        for (int column = 0; column < 3; ++column) {
            solution(0, column) = (d * rhs(0, column) - b * rhs(1, column)) / determinant;
            solution(1, column) = (a * rhs(1, column) - c * rhs(0, column)) / determinant;
        }
        return solution;
    }

    template <class T_> T_ Objective(const Matrix_<T_>& solution) {
        return solution(0, 0) - 2.0 * solution(1, 0) + 0.5 * solution(0, 1) + 3.0 * solution(1, 1);
    }

    struct Active_ {
        Vector_<Number_> parameters_;
        Matrix_<Number_> rhs_{2, 3};

        Active_(Dal::AAD::RecordingScope_* scope, const Sample_& sample) : parameters_(sample.parameters_.size()) {
            for (size_t index = 0; index < parameters_.size(); ++index)
                scope->RegisterInput(parameters_[index], sample.parameters_[index]);
            for (int row = 0; row < 2; ++row)
                for (int column = 0; column < 3; ++column)
                    scope->RegisterInput(rhs_(row, column), sample.rhs_(row, column));
        }
    };

    struct Gradient_ {
        double value_;
        Vector_<> parameters_;
        Matrix_<> rhs_{2, 3};
    };

    void CheckPhysicalReport(const Dal::AAD::SolveAccuracyReports_& reports, const Dal::AAD::SolveAccuracyEvent_& event) {
        const auto& errors = reports.Report(event).transposeBackwardErrors_;
        REQUIRE(errors.Rows() == 3, "Independent report RHS extent");
        REQUIRE(errors.Cols() == 1, "Independent report channel extent");
        REQUIRE(errors(2, 0) == 0.0, "Independent zero RHS seed column must report zero");
        for (double error : errors)
            REQUIRE(std::isfinite(error) && error <= 1e-12, "Independent physical transpose limit");
    }

    Gradient_ Gradient(const Sample_& sample, bool checked) {
        using namespace Dal::AAD;
        Clear(*Tape());
        RecordingScope_ scope;
        const Active_ inputs(&scope, sample);
        scope.StartRecording();
        CheckedLinearSolveResult_ capture;
        Matrix_<Number_> solution;
        if (checked) {
            capture = LinearSolveWithAccuracy(&scope, sample.Layout(), inputs.parameters_, inputs.rhs_, LinearSolveAccuracyPolicy_{1e-12, 1e-12});
            solution = capture.solution_;
        } else
            solution = Cramer(sample.symmetric_, inputs.parameters_, inputs.rhs_);
        Number_ objective = Objective(solution);
        scope.FinishRecording();
        scope.ClearAdjoints();
        NativeOperations_::SetSeed(objective, 1.0);
        const auto reports = ReverseWithSolveAccuracy(&scope);
        if (checked)
            CheckPhysicalReport(reports, capture.event_);
        Gradient_ result{Value(objective), Vector_<>(inputs.parameters_.size())};
        for (size_t index = 0; index < result.parameters_.size(); ++index)
            result.parameters_[index] = NativeOperations_::ReadAdjoint(inputs.parameters_[index]);
        for (int row = 0; row < 2; ++row)
            for (int column = 0; column < 3; ++column)
                result.rhs_(row, column) = NativeOperations_::ReadAdjoint(inputs.rhs_(row, column));
        scope.Close();
        Clear(*Tape());
        return result;
    }

    void Compare(const Gradient_& checked, const Gradient_& reference) {
        ASSERT_NEAR(checked.value_, reference.value_, 1e-10);
        for (size_t index = 0; index < checked.parameters_.size(); ++index)
            ASSERT_NEAR(checked.parameters_[index], reference.parameters_[index], 1e-10);
        for (int row = 0; row < 2; ++row)
            for (int column = 0; column < 3; ++column)
                ASSERT_NEAR(checked.rhs_(row, column), reference.rhs_(row, column), 1e-10);
    }

    void Differences(const Sample_& sample, const Gradient_& checked, double step, double tolerance) {
        for (size_t index = 0; index < sample.parameters_.size(); ++index) {
            auto plus = sample.parameters_, minus = sample.parameters_;
            plus[index] += step;
            minus[index] -= step;
            const double difference =
                (Objective(Cramer(sample.symmetric_, plus, sample.rhs_)) - Objective(Cramer(sample.symmetric_, minus, sample.rhs_))) / (2.0 * step);
            ASSERT_NEAR(checked.parameters_[index], difference, tolerance);
        }
        for (int row = 0; row < 2; ++row)
            for (int column = 0; column < 3; ++column) {
                auto plus = sample.rhs_, minus = sample.rhs_;
                plus(row, column) += step;
                minus(row, column) -= step;
                const double difference = (Objective(Cramer(sample.symmetric_, sample.parameters_, plus)) -
                                           Objective(Cramer(sample.symmetric_, sample.parameters_, minus))) /
                                          (2.0 * step);
                ASSERT_NEAR(checked.rhs_(row, column), difference, tolerance);
            }
    }
} // namespace

TEST(AADLinearSolveTest, TestCheckedCoordinateIndependentNativeCramerAndZeroRhsSeeds) {
    for (bool symmetric : {false, true}) {
        const Sample_ sample(symmetric);
        ASSERT_NO_FATAL_FAILURE(Compare(Gradient(sample, true), Gradient(sample, false)));
    }
}

TEST(AADLinearSolveTest, TestCheckedCoordinateIndependentMultipleStepDifferences) {
    for (bool symmetric : {false, true}) {
        const Sample_ sample(symmetric);
        const auto checked = Gradient(sample, true);
        const double steps[] = {1e-3, 1e-4, 1e-5}, tolerances[] = {3e-6, 3e-8, 1e-9};
        for (int index = 0; index < 3; ++index)
            ASSERT_NO_FATAL_FAILURE(Differences(sample, checked, steps[index], tolerances[index]));
    }
}

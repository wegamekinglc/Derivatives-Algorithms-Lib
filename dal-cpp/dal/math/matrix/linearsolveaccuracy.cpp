//
// Created by Codex on 2026/10/8.
//

#include <dal/platform/platform.hpp>

#include <cmath>
#include <utility>

#include <dal/math/matrix/linearsolveaccuracy.hpp>
#include <dal/utilities/exceptions.hpp>

namespace Dal {
    namespace {
        void ValidateLimit(double limit, const char* message) { REQUIRE(std::isfinite(limit) && limit >= 0.0 && limit <= 1.0, message); }

        LinearSolveAccuracyPolicy_ ValidatePolicy(const LinearSolveAccuracyPolicy_& policy) {
            ValidateLimit(policy.forwardBackwardErrorLimit_, "CheckedLinearSolve: forward backward-error limit must be finite in [0,1]");
            ValidateLimit(policy.transposeBackwardErrorLimit_, "CheckedLinearSolve: transpose backward-error limit must be finite in [0,1]");
            return policy;
        }

        SquareMatrix_<> Transpose(const SquareMatrix_<>& matrix) {
            SquareMatrix_<> result(matrix.Rows());
            for (int row = 0; row < matrix.Rows(); ++row)
                for (int column = 0; column < matrix.Rows(); ++column)
                    result(row, column) = matrix(column, row);
            return result;
        }

        void ValidateErrors(const Vector_<>& errors, double limit, const char* message) {
            for (double error : errors)
                REQUIRE(std::isfinite(error) && error >= 0.0 && error <= limit, message);
        }
    } // namespace

    CheckedLinearSolve_::CheckedLinearSolve_(const SquareMatrix_<>& matrix,
                                             const Matrix_<>& rhs,
                                             const LinearSolveAccuracyPolicy_& policy,
                                             double relativePivotTolerance)
        : policy_(ValidatePolicy(policy)), transpose_(Transpose(matrix)), solve_(matrix, rhs, relativePivotTolerance) {
        ValidateErrors(Diagnostics().componentwiseBackwardErrors_, policy_.forwardBackwardErrorLimit_,
                       "CheckedLinearSolve: forward backward error exceeds the declared accuracy limit");
    }

    Vector_<> CheckedLinearSolve_::TransposeErrors(const Matrix_<>& seeds, const Matrix_<>& adjoints) const {
        auto errors = LinearSolveBackwardErrors(transpose_, seeds, adjoints);
        ValidateErrors(errors, policy_.transposeBackwardErrorLimit_,
                       "CheckedLinearSolve.Reverse: transpose backward error exceeds the declared accuracy limit");
        return errors;
    }

    CheckedLinearSolveAdjoints_ CheckedLinearSolve_::Reverse(const Matrix_<>& solutionAdjoints) const {
        auto adjoints = solve_.Solve().Reverse(solutionAdjoints);
        auto errors = TransposeErrors(solutionAdjoints, adjoints.rhs_);
        return {std::move(adjoints), std::move(errors)};
    }

    CheckedLinearSolveAdjoints_ CheckedLinearSolve_::ReverseRhs(const Matrix_<>& solutionAdjoints) const {
        auto rhs = solve_.Solve().ReverseRhs(solutionAdjoints);
        auto errors = TransposeErrors(solutionAdjoints, rhs);
        return {{SquareMatrix_<>(), std::move(rhs)}, std::move(errors)};
    }
} // namespace Dal

#include <dal/platform/platform.hpp>

#include <algorithm>
#include <cmath>
#include <optional>
#include <utility>

#include <dal/math/matrix/linearsolveaccuracy.hpp>
#include <dal/math/matrix/linearsolveadjointsinternal.hpp>
#include <dal/math/matrix/physicalrowerrorinternal.hpp>
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
        constexpr double REFINEMENT_TARGET = 64. * std::numeric_limits<double>::epsilon();
        bool NeedsRefinement(const Vector_<>& errors) {
            return std::any_of(errors.begin(), errors.end(), [](double error) { return error > REFINEMENT_TARGET; });
        }
        Vector_<int> HomogeneousUnknowns(const SquareMatrix_<>& matrix, const Matrix_<>& seeds, int column) {
            Vector_<int> result(matrix.Rows(), -1);
            for (int row = 0; row < matrix.Rows(); ++row)
                if (seeds(row, column) == 0.) {
                    result[row] = 0;
                    for (int entry = 0; entry < matrix.Rows(); ++entry)
                        result[row] += matrix(row, entry) != 0.;
                }
            return result;
        }
        int SingleUnknown(const SquareMatrix_<>& matrix, int row, const Vector_<int>& zeros) {
            for (int entry = 0; entry < matrix.Rows(); ++entry)
                if (matrix(row, entry) != 0. && !zeros[entry])
                    return entry;
            return -1;
        }
        void PropagateZero(const SquareMatrix_<>& matrix, int entry, Vector_<int>* unknowns, Vector_<int>* pending) {
            for (int row = 0; row < matrix.Rows(); ++row)
                if ((*unknowns)[row] > 0 && matrix(row, entry) != 0.) {
                    --(*unknowns)[row];
                    if ((*unknowns)[row] == 1)
                        pending->push_back(row);
                }
        }
        Vector_<int> StructuralZeros(const SquareMatrix_<>& matrix, const Matrix_<>& seeds, int column) {
            auto unknowns = HomogeneousUnknowns(matrix, seeds, column);
            Vector_<int> zeros(matrix.Rows(), 0), pending;
            for (int row = 0; row < matrix.Rows(); ++row)
                if (unknowns[row] == 1)
                    pending.push_back(row);
            while (!pending.empty()) {
                const int row = pending.back();
                pending.pop_back();
                if (unknowns[row] != 1)
                    continue;
                const int entry = SingleUnknown(matrix, row, zeros);
                if (entry < 0)
                    continue;
                zeros[entry] = 1;
                PropagateZero(matrix, entry, &unknowns, &pending);
            }
            return zeros;
        }
        Matrix_<> StructuralCandidate(const SquareMatrix_<>& matrix, const Matrix_<>& seeds, const Matrix_<>& solution, const Vector_<>& errors) {
            Matrix_<> result(solution);
            for (int column = 0; column < seeds.Cols(); ++column) {
                if (errors[column] <= REFINEMENT_TARGET)
                    continue;
                const auto zeros = StructuralZeros(matrix, seeds, column);
                for (int row = 0; row < matrix.Rows(); ++row)
                    if (zeros[row])
                        result(row, column) = 0.;
            }
            return result;
        }
        std::optional<Matrix_<>> Residual(const SquareMatrix_<>& matrix, const Matrix_<>& seeds, const Matrix_<>& solution, const Vector_<>& errors) {
            Matrix_<> result(seeds.Rows(), seeds.Cols(), 0.);
            for (int column = 0; column < seeds.Cols(); ++column) {
                if (errors[column] <= REFINEMENT_TARGET)
                    continue;
                for (int row = 0; row < seeds.Rows(); ++row) {
                    const double error = LinearSolveDetail::PhysicalRowBackwardError(seeds(row, column), seeds.Rows(), [&](int entry) {
                        return LinearSolveDetail::ScaledProduct_(matrix(row, entry), solution(entry, column));
                    });
                    if (error <= REFINEMENT_TARGET)
                        continue;
                    double value = seeds(row, column);
                    for (int entry = 0; entry < seeds.Rows(); ++entry)
                        value = std::fma(-matrix(row, entry), solution(entry, column), value);
                    if (!std::isfinite(value))
                        return std::nullopt;
                    result(row, column) = value;
                }
            }
            return result;
        }
        std::optional<Matrix_<>> Corrected(const LinearSolvePullback_& solve, const Matrix_<>& solution, const std::optional<Matrix_<>>& residual) {
            if (!residual)
                return std::nullopt;
            const auto correction = solve.ReverseRhs(*residual);
            Matrix_<> result(solution);
            for (int row = 0; row < result.Rows(); ++row)
                for (int column = 0; column < result.Cols(); ++column) {
                    result(row, column) += correction(row, column);
                    if (!std::isfinite(result(row, column)))
                        return std::nullopt;
                }
            return result;
        }
        Vector_<> AcceptCorrection(
            const SquareMatrix_<>& matrix, const Matrix_<>& seeds, const Matrix_<>& candidate, LinearSolveAdjoints_* adjoints, Vector_<> errors) {
            const auto next = LinearSolveBackwardErrors(matrix, seeds, candidate);
            for (int column = 0; column < seeds.Cols(); ++column)
                if (errors[column] > REFINEMENT_TARGET && next[column] <= errors[column]) {
                    for (int row = 0; row < adjoints->rhs_.Rows(); ++row)
                        adjoints->rhs_(row, column) = candidate(row, column);
                    errors[column] = next[column];
                }
            return errors;
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

    Vector_<> CheckedLinearSolve_::TransposeErrors(const Matrix_<>& seeds, LinearSolveAdjoints_* adjoints) const {
        auto errors = LinearSolveBackwardErrors(transpose_, seeds, adjoints->rhs_);
        // Homogeneous equations with one unproved variable force exact zeros; no magnitude threshold is used.
        if (NeedsRefinement(errors)) {
            const auto candidate = StructuralCandidate(transpose_, seeds, adjoints->rhs_, errors);
            errors = AcceptCorrection(transpose_, seeds, candidate, adjoints, std::move(errors));
        }
        constexpr int MAX_REFINEMENTS =
            (std::numeric_limits<double>::max_exponent - std::numeric_limits<double>::min_exponent) / std::numeric_limits<double>::digits + 2;
        for (int pass = 0; pass < MAX_REFINEMENTS && NeedsRefinement(errors); ++pass) {
            const auto candidate = Corrected(solve_.Solve(), adjoints->rhs_, Residual(transpose_, seeds, adjoints->rhs_, errors));
            if (!candidate || std::equal(candidate->begin(), candidate->end(), adjoints->rhs_.begin()))
                break;
            errors = AcceptCorrection(transpose_, seeds, *candidate, adjoints, std::move(errors));
        }
        ValidateErrors(errors, policy_.transposeBackwardErrorLimit_,
                       "CheckedLinearSolve.Reverse: transpose backward error exceeds the declared accuracy limit");
        return errors;
    }

    CheckedLinearSolveAdjoints_ CheckedLinearSolve_::Reverse(const Matrix_<>& solutionAdjoints) const {
        auto result = ReverseRhs(solutionAdjoints);
        result.adjoints_.matrix_ = LinearSolveDetail::MatrixAdjoints(Solution(), result.adjoints_.rhs_);
        return result;
    }

    CheckedLinearSolveAdjoints_ CheckedLinearSolve_::ReverseRhs(const Matrix_<>& solutionAdjoints) const {
        LinearSolveAdjoints_ adjoints{SquareMatrix_<>(), solve_.Solve().ReverseRhs(solutionAdjoints)};
        auto errors = TransposeErrors(solutionAdjoints, &adjoints);
        return {std::move(adjoints), std::move(errors)};
    }
} // namespace Dal

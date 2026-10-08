//
// Created by Codex on 2026/10/8.
//

#include <dal/platform/platform.hpp>

#include <utility>

#include <dal/math/matrix/linearsolvecoordinateaccuracy.hpp>
#include <dal/math/matrix/linearsolvecoordinatesinternal.hpp>

namespace Dal {
    CheckedCoordinateLinearSolve_::CheckedCoordinateLinearSolve_(const LinearSolveCoordinates_& coordinates,
                                                                 const Vector_<>& parameters,
                                                                 const Matrix_<>& rhs,
                                                                 const LinearSolveAccuracyPolicy_& policy,
                                                                 double relativePivotTolerance)
        : coordinates_(coordinates), solve_(coordinates.Expand(parameters), rhs, policy, relativePivotTolerance) {}

    CheckedCoordinateLinearSolveAdjoints_ CheckedCoordinateLinearSolve_::Reverse(const Matrix_<>& solutionAdjoints) const {
        auto risk = solve_.ReverseRhs(solutionAdjoints);
        auto coordinates = Detail::LinearSolveCoordinateAdjoints(coordinates_, Solution(), risk.adjoints_.rhs_);
        return {{std::move(coordinates), std::move(risk.adjoints_.rhs_)}, std::move(risk.transposeBackwardErrors_)};
    }

    CheckedCoordinateLinearSolveAdjoints_ CheckedCoordinateLinearSolve_::ReverseRhs(const Matrix_<>& solutionAdjoints) const {
        auto risk = solve_.ReverseRhs(solutionAdjoints);
        return {{Vector_<>(), std::move(risk.adjoints_.rhs_)}, std::move(risk.transposeBackwardErrors_)};
    }
} // namespace Dal

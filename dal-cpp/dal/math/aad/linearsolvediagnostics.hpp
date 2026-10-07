//
// Created by Codex on 2026/10/8.
//

#pragma once

#include <limits>

#include <dal/math/aad/recording.hpp>
#include <dal/math/matrix/linearsolvediagnostics.hpp>

namespace Dal::AAD {
    struct DiagnosedLinearSolveResult_ {
        Matrix_<Number_> solution_;
        LinearSolveDiagnostics_ diagnostics_;
    };

    [[nodiscard]] DiagnosedLinearSolveResult_ LinearSolveWithDiagnostics(RecordingScope_* recording,
                                                                         const SquareMatrix_<Number_>& matrix,
                                                                         const Matrix_<Number_>& rhs,
                                                                         double relativePivotTolerance = 64.0 *
                                                                                                         std::numeric_limits<double>::epsilon());
    [[nodiscard]] DiagnosedLinearSolveResult_ LinearSolveWithDiagnostics(RecordingScope_* recording,
                                                                         const SquareMatrix_<>& matrix,
                                                                         const Matrix_<Number_>& rhs,
                                                                         double relativePivotTolerance = 64.0 *
                                                                                                         std::numeric_limits<double>::epsilon());
    [[nodiscard]] DiagnosedLinearSolveResult_ LinearSolveWithDiagnostics(RecordingScope_* recording,
                                                                         const SquareMatrix_<Number_>& matrix,
                                                                         const Matrix_<>& rhs,
                                                                         double relativePivotTolerance = 64.0 *
                                                                                                         std::numeric_limits<double>::epsilon());
} // namespace Dal::AAD

//
// Created by Codex on 2026/10/7.
//

#include <dal/platform/platform.hpp>

#include <algorithm>
#include <cmath>
#include <optional>
#include <type_traits>

#include <dal/math/aad/linearsolve.hpp>
#include <dal/math/aad/linearsolvecoordinates.hpp>
#include <dal/math/aad/linearsolvediagnostics.hpp>
#include <dal/math/aad/linearsolveinternal.hpp>
#include <dal/math/aad/native.hpp>
#include <dal/math/aad/reverseevent.hpp>
#include <dal/math/buffercapacity.hpp>
#include <dal/math/matrix/linearsolvepullback.hpp>

namespace Dal::AAD {
    namespace {
        const SquareMatrix_<>& ParameterAdjoints(const LinearSolveAdjoints_& contributions) { return contributions.matrix_; }

        const Vector_<>& ParameterAdjoints(const CoordinateLinearSolveAdjoints_& contributions) { return contributions.coordinates_; }

        template <class C_, class P_>
        void ReverseChannels(
            const C_& solve, P_* parameters, bool parametersActive, Matrix_<Number_>* rhs, Matrix_<Number_>* outputs, bool multi, size_t width) {
            const auto channels = multi ? width : 1;
            for (size_t channel = 0; channel < channels; ++channel) {
                const auto seeds = CollectSeeds(*outputs, channel);
                if (!std::all_of(seeds.begin(), seeds.end(), [](double value) { return value == 0.0; })) {
                    if (parametersActive) {
                        const auto contributions = solve.Reverse(seeds);
                        Scatter(parameters, ParameterAdjoints(contributions), channel);
                        Scatter(rhs, contributions.rhs_, channel);
                    } else {
                        const auto contribution = solve.ReverseRhs(seeds);
                        Scatter(rhs, contribution, channel);
                    }
                }
                ClearOutputs(outputs, channel);
            }
        }

        template <class C_> class LinearSolvePayload_ {
            C_ solve_;
            SquareMatrix_<Number_> matrix_;
            Matrix_<Number_> rhs_;
            Matrix_<Number_> outputs_;

            [[nodiscard]] const LinearSolvePullback_& NumericSolve() const {
                if constexpr (std::is_same_v<C_, LinearSolvePullback_>)
                    return solve_;
                else
                    return solve_.Solve();
            }

        public:
            template <class M_, class R_>
            LinearSolvePayload_(const NativeInputSlots_& slots, const SquareMatrix_<M_>& matrix, const Matrix_<R_>& rhs, double tolerance)
                : solve_(Snapshot(slots, matrix), Snapshot(slots, rhs), tolerance), outputs_(rhs.Rows(), rhs.Cols()) {
                if constexpr (std::is_same_v<M_, Number_>)
                    matrix_ = matrix;
                if constexpr (std::is_same_v<R_, Number_>)
                    rhs_ = rhs;
            }

            [[nodiscard]] Matrix_<Number_> MakeOutputs() { return AAD::MakeOutputs(NumericSolve().Solution(), &outputs_); }

            [[nodiscard]] const LinearSolveDiagnostics_& Diagnostics() const { return solve_.Diagnostics(); }

            void Reverse(bool multi, size_t width) { ReverseChannels(NumericSolve(), &matrix_, matrix_.Rows() != 0, &rhs_, &outputs_, multi, width); }
        };

        class CoordinateLinearSolvePayload_ {
            CoordinateLinearSolvePullback_ solve_;
            Vector_<Number_> parameters_;
            Matrix_<Number_> rhs_;
            Matrix_<Number_> outputs_;

        public:
            template <class P_, class R_>
            CoordinateLinearSolvePayload_(const NativeInputSlots_& slots,
                                          const LinearSolveCoordinates_& coordinates,
                                          const Vector_<P_>& parameters,
                                          const Matrix_<R_>& rhs,
                                          double tolerance)
                : solve_(coordinates, Snapshot(slots, parameters), Snapshot(slots, rhs), tolerance), outputs_(rhs.Rows(), rhs.Cols()) {
                if constexpr (std::is_same_v<P_, Number_>)
                    parameters_ = parameters;
                if constexpr (std::is_same_v<R_, Number_>)
                    rhs_ = rhs;
            }

            [[nodiscard]] Matrix_<Number_> MakeOutputs() { return AAD::MakeOutputs(solve_.Solution(), &outputs_); }

            void Reverse(bool multi, size_t width) { ReverseChannels(solve_, &parameters_, !parameters_.empty(), &rhs_, &outputs_, multi, width); }
        };

        template <class C_, class M_, class R_>
        auto RecordSolve(RecordingScope_* recording, const SquareMatrix_<M_>& matrix, const Matrix_<R_>& rhs, double tolerance) {
            return WithRecordingFailure(recording, [&](Tape_* tape) {
                REQUIRE(matrix.Rows() > 0 && rhs.Rows() == matrix.Rows() && rhs.Cols() > 0, "LinearSolve: requires a nonempty square system and RHS");
                auto event = NativeRecordedOperation_::MakeEvent<LinearSolveEvent_<LinearSolvePayload_<C_>>>(tape, tape, matrix, rhs, tolerance);
                auto diagnostics = [&] {
                    if constexpr (std::is_same_v<C_, DiagnosedLinearSolve_>)
                        return event->Diagnostics();
                    else
                        return std::nullopt;
                }();
                auto result = PublishSolve(recording, tape, std::move(event));
                if constexpr (std::is_same_v<C_, DiagnosedLinearSolve_>)
                    return DiagnosedLinearSolveResult_{std::move(result), std::move(diagnostics)};
                else
                    return result;
            });
        }

        template <class P_, class R_>
        Matrix_<Number_> RecordCoordinateSolve(RecordingScope_* recording,
                                               const LinearSolveCoordinates_& coordinates,
                                               const Vector_<P_>& parameters,
                                               const Matrix_<R_>& rhs,
                                               double tolerance) {
            return WithRecordingFailure(recording, [&](Tape_* tape) {
                REQUIRE(parameters.size() == coordinates.Count(), "LinearSolve: coordinate parameter count does not match the layout");
                REQUIRE(rhs.Rows() == coordinates.Size() && rhs.Cols() > 0, "LinearSolve: coordinate RHS requires compatible nonempty dimensions");
                auto event = NativeRecordedOperation_::MakeEvent<LinearSolveEvent_<CoordinateLinearSolvePayload_>>(tape, tape, coordinates,
                                                                                                                   parameters, rhs, tolerance);
                return PublishSolve(recording, tape, std::move(event));
            });
        }
    } // namespace

    Matrix_<Number_>
    LinearSolve(RecordingScope_* recording, const SquareMatrix_<Number_>& matrix, const Matrix_<Number_>& rhs, double relativePivotTolerance) {
        return RecordSolve<LinearSolvePullback_>(recording, matrix, rhs, relativePivotTolerance);
    }

    Matrix_<Number_>
    LinearSolve(RecordingScope_* recording, const SquareMatrix_<>& matrix, const Matrix_<Number_>& rhs, double relativePivotTolerance) {
        return RecordSolve<LinearSolvePullback_>(recording, matrix, rhs, relativePivotTolerance);
    }

    Matrix_<Number_>
    LinearSolve(RecordingScope_* recording, const SquareMatrix_<Number_>& matrix, const Matrix_<>& rhs, double relativePivotTolerance) {
        return RecordSolve<LinearSolvePullback_>(recording, matrix, rhs, relativePivotTolerance);
    }

    Matrix_<Number_> LinearSolve(RecordingScope_* recording,
                                 const LinearSolveCoordinates_& coordinates,
                                 const Vector_<Number_>& parameters,
                                 const Matrix_<Number_>& rhs,
                                 double relativePivotTolerance) {
        return RecordCoordinateSolve(recording, coordinates, parameters, rhs, relativePivotTolerance);
    }

    Matrix_<Number_> LinearSolve(RecordingScope_* recording,
                                 const LinearSolveCoordinates_& coordinates,
                                 const Vector_<>& parameters,
                                 const Matrix_<Number_>& rhs,
                                 double relativePivotTolerance) {
        return RecordCoordinateSolve(recording, coordinates, parameters, rhs, relativePivotTolerance);
    }

    Matrix_<Number_> LinearSolve(RecordingScope_* recording,
                                 const LinearSolveCoordinates_& coordinates,
                                 const Vector_<Number_>& parameters,
                                 const Matrix_<>& rhs,
                                 double relativePivotTolerance) {
        return RecordCoordinateSolve(recording, coordinates, parameters, rhs, relativePivotTolerance);
    }

    DiagnosedLinearSolveResult_ LinearSolveWithDiagnostics(RecordingScope_* recording,
                                                           const SquareMatrix_<Number_>& matrix,
                                                           const Matrix_<Number_>& rhs,
                                                           double relativePivotTolerance) {
        return RecordSolve<DiagnosedLinearSolve_>(recording, matrix, rhs, relativePivotTolerance);
    }

    DiagnosedLinearSolveResult_ LinearSolveWithDiagnostics(RecordingScope_* recording,
                                                           const SquareMatrix_<>& matrix,
                                                           const Matrix_<Number_>& rhs,
                                                           double relativePivotTolerance) {
        return RecordSolve<DiagnosedLinearSolve_>(recording, matrix, rhs, relativePivotTolerance);
    }

    DiagnosedLinearSolveResult_ LinearSolveWithDiagnostics(RecordingScope_* recording,
                                                           const SquareMatrix_<Number_>& matrix,
                                                           const Matrix_<>& rhs,
                                                           double relativePivotTolerance) {
        return RecordSolve<DiagnosedLinearSolve_>(recording, matrix, rhs, relativePivotTolerance);
    }
} // namespace Dal::AAD

//
// Created by Codex on 2026/10/7.
//

#include <dal/platform/platform.hpp>

#include <algorithm>
#include <cmath>
#include <type_traits>

#include <dal/math/aad/linearsolve.hpp>
#include <dal/math/aad/native.hpp>
#include <dal/math/aad/reverseevent.hpp>
#include <dal/math/matrix/linearsolvepullback.hpp>

namespace Dal::AAD {
    namespace {
        template <class T_, class E_> void CopyValues(Tape_* tape, const Matrix_<E_>& source, T_* destination) {
            for (int row = 0; row < source.Rows(); ++row)
                for (int column = 0; column < source.Cols(); ++column) {
                    if constexpr (std::is_same_v<E_, Number_>) {
                        NativeRecordedOperation_::ValidateInput(tape, source(row, column));
                        (*destination)(row, column) = Value(source(row, column));
                    } else
                        (*destination)(row, column) = source(row, column);
                }
        }

        void AddContribution(Number_* input, double contribution, size_t channel) {
            const double total = NativeOperations_::ReadAdjoint(*input, channel) + contribution;
            REQUIRE(std::isfinite(total), "LinearSolve.Reverse: input adjoint accumulation overflow");
            NativeOperations_::SetSeed(*input, total, channel);
        }

        template <class T_> void Scatter(T_* inputs, const Matrix_<>& contributions, size_t channel) {
            for (int row = 0; row < inputs->Rows(); ++row)
                for (int column = 0; column < inputs->Cols(); ++column)
                    AddContribution(&(*inputs)(row, column), contributions(row, column), channel);
        }

        class LinearSolveEvent_ final : public ReverseEvent_ {
            LinearSolvePullback_ solve_;
            SquareMatrix_<Number_> matrix_;
            Matrix_<Number_> rhs_;
            Matrix_<Number_> outputs_;

            Matrix_<> CollectSeeds(size_t channel) const {
                Matrix_<> seeds(outputs_.Rows(), outputs_.Cols());
                for (int row = 0; row < seeds.Rows(); ++row)
                    for (int column = 0; column < seeds.Cols(); ++column)
                        seeds(row, column) = NativeOperations_::ReadAdjoint(outputs_(row, column), channel);
                return seeds;
            }

            void ClearOutputs(size_t channel) {
                for (int row = 0; row < outputs_.Rows(); ++row)
                    for (int column = 0; column < outputs_.Cols(); ++column)
                        NativeOperations_::SetSeed(outputs_(row, column), 0.0, channel);
            }

            void ReverseChannel(size_t channel) {
                const auto seeds = CollectSeeds(channel);
                if (!std::all_of(seeds.begin(), seeds.end(), [](double value) { return value == 0.0; })) {
                    if (matrix_.Rows() == 0) {
                        const auto contribution = solve_.ReverseRhs(seeds);
                        Scatter(&rhs_, contribution, channel);
                    } else {
                        const auto contributions = solve_.Reverse(seeds);
                        Scatter(&matrix_, contributions.matrix_, channel);
                        Scatter(&rhs_, contributions.rhs_, channel);
                    }
                }
                ClearOutputs(channel);
            }

        public:
            template <class M_, class R_>
            LinearSolveEvent_(const SquareMatrix_<M_>& matrix, const Matrix_<R_>& rhs, LinearSolvePullback_ solve) : solve_(std::move(solve)) {
                if constexpr (std::is_same_v<M_, Number_>)
                    matrix_ = matrix;
                if constexpr (std::is_same_v<R_, Number_>)
                    rhs_ = rhs;
            }

            [[nodiscard]] Matrix_<Number_> MakeOutputs() {
                const auto& values = solve_.Solution();
                Matrix_<Number_> result(values.Rows(), values.Cols());
                for (int row = 0; row < values.Rows(); ++row)
                    for (int column = 0; column < values.Cols(); ++column)
                        result(row, column) = values(row, column);
                outputs_ = result;
                return result;
            }

            void Reverse(bool multi, size_t width) override {
                const auto channels = multi ? width : 1;
                for (size_t channel = 0; channel < channels; ++channel)
                    ReverseChannel(channel);
            }
        };
        template <class M_, class R_>
        Matrix_<Number_> RecordSolve(RecordingScope_* recording, const SquareMatrix_<M_>& matrix, const Matrix_<R_>& rhs, double tolerance) {
            auto* tape = NativeRecordedOperation_::Begin(recording);
            try {
                REQUIRE(!TapeCapacityActive(), "LinearSolve: event cache capacity admission is not yet available");
                REQUIRE(matrix.Rows() > 0 && rhs.Rows() == matrix.Rows() && rhs.Cols() > 0, "LinearSolve: requires a nonempty square system and RHS");
                SquareMatrix_<> matrixValues(matrix.Rows());
                Matrix_<> rhsValues(rhs.Rows(), rhs.Cols());
                CopyValues(tape, static_cast<const Matrix_<M_>&>(matrix), &matrixValues);
                CopyValues(tape, rhs, &rhsValues);
                auto event = std::make_unique<LinearSolveEvent_>(matrix, rhs, LinearSolvePullback_(matrixValues, rhsValues, tolerance));
                auto result = event->MakeOutputs();
                NativeRecordedOperation_::Commit(recording, std::move(event));
                return result;
            } catch (...) {
                NativeRecordedOperation_::Fail(recording);
                throw;
            }
        }
    } // namespace

    Matrix_<Number_>
    LinearSolve(RecordingScope_* recording, const SquareMatrix_<Number_>& matrix, const Matrix_<Number_>& rhs, double relativePivotTolerance) {
        return RecordSolve(recording, matrix, rhs, relativePivotTolerance);
    }

    Matrix_<Number_>
    LinearSolve(RecordingScope_* recording, const SquareMatrix_<>& matrix, const Matrix_<Number_>& rhs, double relativePivotTolerance) {
        return RecordSolve(recording, matrix, rhs, relativePivotTolerance);
    }

    Matrix_<Number_>
    LinearSolve(RecordingScope_* recording, const SquareMatrix_<Number_>& matrix, const Matrix_<>& rhs, double relativePivotTolerance) {
        return RecordSolve(recording, matrix, rhs, relativePivotTolerance);
    }
} // namespace Dal::AAD

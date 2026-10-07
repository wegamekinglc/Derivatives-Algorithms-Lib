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
#include <dal/math/aad/native.hpp>
#include <dal/math/aad/reverseevent.hpp>
#include <dal/math/buffercapacity.hpp>
#include <dal/math/matrix/linearsolvepullback.hpp>

namespace Dal::AAD {
    namespace {
        template <class T_, class E_> void CopyValues(const NativeInputSlots_& slots, const Matrix_<E_>& source, T_* destination) {
            for (int row = 0; row < source.Rows(); ++row)
                for (int column = 0; column < source.Cols(); ++column) {
                    if constexpr (std::is_same_v<E_, Number_>) {
                        NativeRecordedOperation_::ValidateInput(slots, source(row, column));
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

        void Scatter(Vector_<Number_>* inputs, const Vector_<>& contributions, size_t channel) {
            for (size_t index = 0; index < inputs->size(); ++index)
                AddContribution(&(*inputs)[index], contributions[index], channel);
        }

        template <class E_> SquareMatrix_<> Snapshot(const NativeInputSlots_& slots, const SquareMatrix_<E_>& source) {
            SquareMatrix_<> result(source.Rows());
            CopyValues(slots, static_cast<const Matrix_<E_>&>(source), &result);
            return result;
        }

        template <class E_> Matrix_<> Snapshot(const NativeInputSlots_& slots, const Matrix_<E_>& source) {
            Matrix_<> result(source.Rows(), source.Cols());
            CopyValues(slots, source, &result);
            return result;
        }

        template <class E_> Vector_<> Snapshot(const NativeInputSlots_& slots, const Vector_<E_>& source) {
            Vector_<> result(source.size());
            for (size_t index = 0; index < source.size(); ++index) {
                if constexpr (std::is_same_v<E_, Number_>) {
                    NativeRecordedOperation_::ValidateInput(slots, source[index]);
                    result[index] = Value(source[index]);
                } else
                    result[index] = source[index];
            }
            return result;
        }

        Matrix_<> CollectSeeds(const Matrix_<Number_>& outputs, size_t channel) {
            Matrix_<> seeds(outputs.Rows(), outputs.Cols());
            for (int row = 0; row < seeds.Rows(); ++row)
                for (int column = 0; column < seeds.Cols(); ++column)
                    seeds(row, column) = NativeOperations_::ReadAdjoint(outputs(row, column), channel);
            return seeds;
        }

        void ClearOutputs(Matrix_<Number_>* outputs, size_t channel) {
            for (int row = 0; row < outputs->Rows(); ++row)
                for (int column = 0; column < outputs->Cols(); ++column)
                    NativeOperations_::SetSeed((*outputs)(row, column), 0.0, channel);
        }

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

        Matrix_<Number_> MakeOutputs(const Matrix_<>& values, Matrix_<Number_>* bindings) {
            Matrix_<Number_> result(values.Rows(), values.Cols());
            for (int row = 0; row < values.Rows(); ++row)
                for (int column = 0; column < values.Cols(); ++column) {
                    result(row, column) = values(row, column);
                    (*bindings)(row, column) = result(row, column);
                }
            return result;
        }

        class EventBufferAccount_ final : public Detail::OwnedBufferAccount_ {
            Tape_* tape_;
            size_t current_ = 0;
            size_t scratchBase_ = 0;
            bool scratching_ = false;

        public:
            explicit EventBufferAccount_(Tape_* tape) : tape_(tape) {}
            void Reserve(size_t bytes) override {
                NativeRecordedOperation_::ReserveStorage(tape_, bytes);
                current_ += bytes;
                if (scratching_)
                    NativeRecordedOperation_::ObserveScratch(tape_, current_ - scratchBase_);
            }
            void Release(size_t bytes) noexcept override {
                if (bytes > current_)
                    std::terminate();
                NativeRecordedOperation_::ReleaseStorage(tape_, bytes);
                current_ -= bytes;
            }
            void StartScratch() noexcept {
                scratchBase_ = current_;
                scratching_ = true;
            }
            void FinishScratch() noexcept { scratching_ = false; }
        };

        class ScratchMeasurement_ {
            EventBufferAccount_* account_;

        public:
            explicit ScratchMeasurement_(EventBufferAccount_* account) : account_(account) { account_->StartScratch(); }
            ~ScratchMeasurement_() noexcept { account_->FinishScratch(); }
            ScratchMeasurement_(const ScratchMeasurement_&) = delete;
            ScratchMeasurement_& operator=(const ScratchMeasurement_&) = delete;
        };

        template <class F_> void WithOwnedPayload(EventBufferAccount_* account, const F_& operation) {
            Detail::BufferCapacitySuspension_ suspended;
            Detail::OwnedBufferScope_ owned(account);
            operation();
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

        template <class P_> class LinearSolveEvent_ final : public ReverseEvent_ {
            EventBufferAccount_ account_;
            std::optional<P_> payload_;

        public:
            template <class... A_> LinearSolveEvent_(Tape_* tape, const A_&... arguments) : account_(tape) {
                WithOwnedPayload(&account_, [&] {
                    const NativeInputSlots_ slots(tape);
                    payload_.emplace(slots, arguments...);
                });
            }
            ~LinearSolveEvent_() noexcept override {
                WithOwnedPayload(&account_, [this] { payload_.reset(); });
            }
            [[nodiscard]] Matrix_<Number_> MakeOutputs() { return payload_->MakeOutputs(); }
            [[nodiscard]] const LinearSolveDiagnostics_& Diagnostics() const { return payload_->Diagnostics(); }
            void Reverse(bool multi, size_t width) override {
                ScratchMeasurement_ measurement(&account_);
                Detail::OwnedBufferScope_ scratch(&account_);
                payload_->Reverse(multi, width);
            }
        };

        template <class F_> auto WithRecordingFailure(RecordingScope_* recording, const F_& operation) {
            auto* tape = NativeRecordedOperation_::Begin(recording);
            try {
                return operation(tape);
            } catch (...) {
                NativeRecordedOperation_::Fail(recording);
                throw;
            }
        }

        template <class E_> Matrix_<Number_> PublishSolve(RecordingScope_* recording, Tape_* tape, std::unique_ptr<E_, ReverseEventDeleter_> event) {
            NativeRecordedOperation_::Prepare(tape);
            auto result = event->MakeOutputs();
            NativeRecordedOperation_::Commit(recording, std::move(event));
            return result;
        }

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

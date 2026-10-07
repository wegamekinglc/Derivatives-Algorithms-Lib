//
// Created by Codex on 2026/10/7.
//

#include <dal/platform/platform.hpp>

#include <algorithm>
#include <cmath>
#include <optional>
#include <type_traits>

#include <dal/math/aad/linearsolve.hpp>
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
                        const auto contribution = NumericSolve().ReverseRhs(seeds);
                        Scatter(&rhs_, contribution, channel);
                    } else {
                        const auto contributions = NumericSolve().Reverse(seeds);
                        Scatter(&matrix_, contributions.matrix_, channel);
                        Scatter(&rhs_, contributions.rhs_, channel);
                    }
                }
                ClearOutputs(channel);
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

            [[nodiscard]] Matrix_<Number_> MakeOutputs() {
                const auto& values = NumericSolve().Solution();
                Matrix_<Number_> result(values.Rows(), values.Cols());
                for (int row = 0; row < values.Rows(); ++row)
                    for (int column = 0; column < values.Cols(); ++column) {
                        result(row, column) = values(row, column);
                        outputs_(row, column) = result(row, column);
                    }
                return result;
            }

            [[nodiscard]] const LinearSolveDiagnostics_& Diagnostics() const { return solve_.Diagnostics(); }

            void Reverse(bool multi, size_t width) {
                const auto channels = multi ? width : 1;
                for (size_t channel = 0; channel < channels; ++channel)
                    ReverseChannel(channel);
            }
        };

        template <class C_> class LinearSolveEvent_ final : public ReverseEvent_ {
            EventBufferAccount_ account_;
            std::optional<LinearSolvePayload_<C_>> payload_;

        public:
            template <class M_, class R_>
            LinearSolveEvent_(Tape_* tape, const SquareMatrix_<M_>& matrix, const Matrix_<R_>& rhs, double tolerance) : account_(tape) {
                WithOwnedPayload(&account_, [&] {
                    const NativeInputSlots_ slots(tape);
                    payload_.emplace(slots, matrix, rhs, tolerance);
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

        template <class C_, class M_, class R_>
        auto RecordSolve(RecordingScope_* recording, const SquareMatrix_<M_>& matrix, const Matrix_<R_>& rhs, double tolerance) {
            auto* tape = NativeRecordedOperation_::Begin(recording);
            try {
                REQUIRE(matrix.Rows() > 0 && rhs.Rows() == matrix.Rows() && rhs.Cols() > 0, "LinearSolve: requires a nonempty square system and RHS");
                auto event = NativeRecordedOperation_::MakeEvent<LinearSolveEvent_<C_>>(tape, tape, matrix, rhs, tolerance);
                auto diagnostics = [&] {
                    if constexpr (std::is_same_v<C_, DiagnosedLinearSolve_>)
                        return event->Diagnostics();
                    else
                        return std::nullopt;
                }();
                NativeRecordedOperation_::Prepare(tape);
                auto result = event->MakeOutputs();
                NativeRecordedOperation_::Commit(recording, std::move(event));
                if constexpr (std::is_same_v<C_, DiagnosedLinearSolve_>)
                    return DiagnosedLinearSolveResult_{std::move(result), std::move(diagnostics)};
                else
                    return result;
            } catch (...) {
                NativeRecordedOperation_::Fail(recording);
                throw;
            }
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

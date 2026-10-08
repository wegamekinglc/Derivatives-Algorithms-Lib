//
// Created by Codex on 2026/10/8.
//

#pragma once

#include <cmath>
#include <optional>
#include <type_traits>

#include <dal/math/aad/native.hpp>
#include <dal/math/aad/recording.hpp>
#include <dal/math/aad/reverseevent.hpp>
#include <dal/math/buffercapacity.hpp>
#include <dal/math/matrix/squarematrix.hpp>

namespace Dal {
    struct LinearSolveDiagnostics_;
} // namespace Dal

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

        FORCE_INLINE void AddContribution(Number_* input, double contribution, size_t channel) {
            const double total = NativeOperations_::ReadAdjoint(*input, channel) + contribution;
            REQUIRE(std::isfinite(total), "LinearSolve.Reverse: input adjoint accumulation overflow");
            NativeOperations_::SetSeed(*input, total, channel);
        }

        template <class T_> void Scatter(T_* inputs, const Matrix_<>& contributions, size_t channel) {
            for (int row = 0; row < inputs->Rows(); ++row)
                for (int column = 0; column < inputs->Cols(); ++column)
                    AddContribution(&(*inputs)(row, column), contributions(row, column), channel);
        }

        [[maybe_unused]] void Scatter(Vector_<Number_>* inputs, const Vector_<>& contributions, size_t channel) {
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

        template <class P_, bool Checked_ = false> class LinearSolveEvent_ final : public ReverseEvent_ {
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
                if constexpr (Checked_)
                    payload_->ReverseWithScratch(multi, width, &account_);
                else {
                    ScratchMeasurement_ measurement(&account_);
                    Detail::OwnedBufferScope_ scratch(&account_);
                    payload_->Reverse(multi, width);
                }
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

    } // namespace
} // namespace Dal::AAD

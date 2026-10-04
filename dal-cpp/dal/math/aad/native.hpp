//
// Created by Codex on 2026/10/04.
//

#pragma once

#include <cstddef>
#include <dal/math/aad/aad.hpp>
#include <dal/utilities/exceptions.hpp>

namespace Dal::AAD {
    struct NativeCapabilities_ {
        bool scalarReverse_ = true;
        bool repeatedFixedGraphReverse_ = true;
        bool intervalReverse_ = true;
        bool prefixAccumulation_ = true;
        bool vectorAdjoints_ = true;
        size_t maxAdjointWidth_ = ADJ_SIZE;
        bool scopedLifecycleValidation_ = true;
        bool numberLifetimeDiagnosticsAvailable_ = true;
        bool numberLifetimeDiagnosticsEnabled_ = false;
        bool independentNesting_ = false;
        bool reverseEvents_ = false;
        bool higherOrder_ = false;
    };

    struct NativeOperations_ {
        [[nodiscard]] static constexpr NativeCapabilities_ Capabilities() {
            NativeCapabilities_ result;
#if defined(DAL_ENABLE_AAD_LIFETIME_DIAGNOSTICS)
            result.numberLifetimeDiagnosticsEnabled_ = true;
#endif
            return result;
        }

        [[nodiscard]] static FORCE_INLINE Number_ ActiveRoot(const Number_& payoff, const Number_& activeZero) {
            return PayoffRoot(payoff, activeZero);
        }

        static void ValidateAdjointMode(bool multi, size_t width) { RequireMode(multi, width, "NativeAAD.ValidateAdjointMode"); }
        static FORCE_INLINE void SetSeed(Number_& number, double seed, size_t channel = 0) { Channel(number, channel, "NativeAAD.SetSeed") = seed; }
        static FORCE_INLINE void AddSeed(Number_& number, double seed, size_t channel = 0) { Channel(number, channel, "NativeAAD.AddSeed") += seed; }
        [[nodiscard]] static FORCE_INLINE double ReadAdjoint(const Number_& number, size_t channel = 0) {
            return Channel(number, channel, "NativeAAD.ReadAdjoint");
        }

    private:
        [[noreturn]] static void Reject(const char* operation, const char* constraint) { THROW(String_(operation) + " [Native]: " + constraint); }

        static FORCE_INLINE void RequireMode(bool multi, size_t width, const char* operation) {
            if (width == 0 || width > ADJ_SIZE)
                Reject(operation, "adjoint width must be positive and at most ADJ_SIZE");
            if (!multi && width != 1)
                Reject(operation, "scalar adjoint width must be one");
        }

        static FORCE_INLINE double& Channel(const Number_& number, size_t channel, const char* operation) {
            auto* tape = Tape();
#if defined(DAL_ENABLE_AAD_LIFETIME_DIAGNOSTICS)
            number.ValidateOperands(tape, operation);
#endif
            if (number.node_ == nullptr)
                Reject(operation, "number has no tape node");
            RequireMode(tape->multi_, tape->numAdj_, operation);
            if (channel >= (tape->multi_ ? tape->numAdj_ : 1))
                Reject(operation, "channel must be below the current adjoint width");
            if (tape->multi_ && number.node_->pAdjoints_ == nullptr)
                Reject(operation, "number has no vector adjoint storage");
            return tape->multi_ ? number.node_->Adjoint(channel) : number.node_->Adjoint();
        }
    };
} // namespace Dal::AAD

//
// Created by Codex on 2026/10/04.
//

#pragma once

#include <cstddef>
#include <dal/math/aad/aad.hpp>
#include <dal/utilities/exceptions.hpp>

namespace Dal::AAD {
    struct BackendCapabilities_ {
        const char* backendName_;
        bool scalarReverse_ = true;
        bool repeatedFixedGraphReverse_ = true;
        bool intervalReverse_ = true;
        bool prefixAccumulation_ = true;
        bool vectorAdjoints_ = false;
        size_t maxAdjointWidth_ = 1;
        bool scopedLifecycleValidation_ = true;
        bool numberLifetimeDiagnosticsAvailable_ = false;
        bool numberLifetimeDiagnosticsEnabled_ = false;
        bool independentNesting_ = false;
        bool reverseEvents_ = false;
        bool higherOrder_ = false;
        bool prefixReverseDiscardsSuffix_ = false;
    };

    struct BackendOperations_ {
        static constexpr auto ACTIVATE = &AAD::Activate;
        static constexpr auto DEACTIVATE = &AAD::Deactivate;
        static constexpr auto REGISTER_INPUT = &AAD::RegisterIndependent;
        static constexpr auto START_RECORDING = &AAD::NewRecording;
        static constexpr auto CLEAR_GRAPH = static_cast<void (*)(Tape_&)>(&AAD::Clear);
        static constexpr auto RESET = &AAD::Rewind;
        static constexpr auto MAKE_MARK = &AAD::Mark;
        static constexpr auto RESTORE_SUFFIX = &AAD::RewindToMark;
        static constexpr auto CLEAR_ADJOINTS = &AAD::ZeroAdjoints;
        static constexpr auto REVERSE = &AAD::PropagateToStart;
        static constexpr auto REVERSE_SUFFIX = &AAD::PropagateToMark;
        static constexpr auto REVERSE_PREFIX = &AAD::PropagateMarkToStart;

        [[nodiscard]] static FORCE_INLINE Number_ ActiveRoot(const Number_& payoff, const Number_& activeZero) {
            return PayoffRoot(payoff, activeZero);
        }

    protected:
        [[noreturn]] static void Reject(const char* operation, const char* backend, const char* constraint) {
            THROW(String_(operation) + " [" + backend + "]: " + constraint);
        }
    };

#if !defined(DAL_USE_XAD_AAD) && !defined(DAL_USE_CODIPACK_AAD) && !defined(DAL_USE_ADEPT_AAD)
    struct NativeBackendAdapter_ : BackendOperations_ {
        [[nodiscard]] static constexpr BackendCapabilities_ Capabilities() {
            BackendCapabilities_ result{"Native"};
            result.vectorAdjoints_ = true;
            result.maxAdjointWidth_ = ADJ_SIZE;
            result.numberLifetimeDiagnosticsAvailable_ = true;
#if defined(DAL_ENABLE_AAD_LIFETIME_DIAGNOSTICS)
            result.numberLifetimeDiagnosticsEnabled_ = true;
#endif
            return result;
        }

        static void ValidateAdjointMode(bool multi, size_t width) { RequireMode(multi, width, "Backend.ValidateAdjointMode"); }

        static FORCE_INLINE void SetSeed(Number_& number, double seed, size_t channel = 0) { Channel(number, channel, "NativeBackend.SetSeed") = seed; }
        static FORCE_INLINE void AddSeed(Number_& number, double seed, size_t channel = 0) { Channel(number, channel, "NativeBackend.AddSeed") += seed; }
        [[nodiscard]] static FORCE_INLINE double ReadAdjoint(const Number_& number, size_t channel = 0) {
            return Channel(number, channel, "NativeBackend.ReadAdjoint");
        }

    private:
        static FORCE_INLINE void RequireMode(bool multi, size_t width, const char* operation) {
            if (width == 0 || width > ADJ_SIZE)
                Reject(operation, "Native", "adjoint width must be positive and at most ADJ_SIZE");
            if (!multi && width != 1)
                Reject(operation, "Native", "scalar adjoint width must be one");
        }

        static FORCE_INLINE double& Channel(const Number_& number, size_t channel, const char* operation) {
            auto* tape = Tape();
#if defined(DAL_ENABLE_AAD_LIFETIME_DIAGNOSTICS)
            number.ValidateOperands(tape, operation);
#endif
            if (number.node_ == nullptr)
                Reject(operation, "Native", "number has no tape node");
            RequireMode(tape->multi_, tape->numAdj_, operation);
            if (channel >= (tape->multi_ ? tape->numAdj_ : 1))
                Reject(operation, "Native", "channel must be below the current adjoint width");
            if (tape->multi_ && number.node_->pAdjoints_ == nullptr)
                Reject(operation, "Native", "number has no vector adjoint storage");
            return tape->multi_ ? number.node_->Adjoint(channel) : number.node_->Adjoint();
        }
    };

    using BackendAdapter_ = NativeBackendAdapter_;
#else
    struct ScalarBackendAdapter_ : BackendOperations_ {
        [[nodiscard]] static constexpr BackendCapabilities_ Capabilities() {
#if defined(DAL_USE_XAD_AAD)
            BackendCapabilities_ result{"XAD"};
            result.prefixReverseDiscardsSuffix_ = true;
            return result;
#elif defined(DAL_USE_CODIPACK_AAD)
            return {"CoDiPack"};
#else
            return {"Adept"};
#endif
        }

        static void ValidateAdjointMode(bool multi, size_t width) {
            if (multi || width != 1)
                Reject("Backend.ValidateAdjointMode", Capabilities().backendName_, "compiled DAL adapter supports scalar mode with width one");
        }

        static FORCE_INLINE void SetSeed(Number_& number, double seed, size_t channel = 0) {
            RequireChannel(channel, "Backend.SetSeed");
            Adjoint(number) = seed;
        }
        static FORCE_INLINE void AddSeed(Number_& number, double seed, size_t channel = 0) {
            RequireChannel(channel, "Backend.AddSeed");
            Adjoint(number) = AdjointValue(number) + seed;
        }
        [[nodiscard]] static FORCE_INLINE double ReadAdjoint(const Number_& number, size_t channel = 0) {
            RequireChannel(channel, "Backend.ReadAdjoint");
            return AdjointValue(number);
        }

    private:
        static FORCE_INLINE void RequireChannel(size_t channel, const char* operation) {
            if (channel != 0)
                Reject(operation, Capabilities().backendName_, "compiled DAL adapter supports only channel zero");
        }
    };

    using BackendAdapter_ = ScalarBackendAdapter_;
#endif
} // namespace Dal::AAD

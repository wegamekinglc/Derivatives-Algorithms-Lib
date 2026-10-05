//
// Created by Codex on 2026/10/04.
//

#include <array>
#include <iostream>
#include <optional>

#include <dal/math/aad/native.hpp>
#include <dal/math/aad/profiling.hpp>
#include <dal/math/aad/recording.hpp>

#if EXPECT_DIAGNOSTICS && !defined(DAL_ENABLE_AAD_LIFETIME_DIAGNOSTICS)
#error Installed diagnostic definition was not propagated
#elif !EXPECT_DIAGNOSTICS && defined(DAL_ENABLE_AAD_LIFETIME_DIAGNOSTICS)
#error Installed default ABI incorrectly enables diagnostics
#endif

#if EXPECT_PROFILING && !defined(DAL_ENABLE_AAD_PROFILING)
#error Installed profiling definition was not propagated
#elif !EXPECT_PROFILING && defined(DAL_ENABLE_AAD_PROFILING)
#error Installed default ABI incorrectly enables profiling
#endif

using namespace Dal::AAD;

namespace {
    void StartProfile(std::optional<ProfilingScope_>* scope, ProfilingData_* data) {
        if constexpr (ProfilingAvailable())
            scope->emplace(data);
    }

    void SampleProfile(std::optional<ProfilingScope_>* scope) {
        if constexpr (ProfilingAvailable())
            scope->value().CaptureTape(*Tape());
    }

    bool ProfileComplete(const ProfilingData_& data) {
        if constexpr (!ProfilingAvailable())
            return true;
        return data.complete_ && data.tapeSamples_ == 1 && data.highWater_.nodes_ == 5;
    }

    bool RejectsClosedInput(const Number_& value) {
        if constexpr (NativeOperations_::Capabilities().numberLifetimeDiagnosticsEnabled_) {
            try {
                static_cast<void>(NativeOperations_::ReadAdjoint(value));
            } catch (const Dal::Exception_&) {
                return true;
            }
            return false;
        }
        return true;
    }

    int ScalarGradients(RecordingScope_* scope, const Number_& x, const Number_& y, Number_* u, Number_* v) {
        scope->ClearAdjoints();
        NativeOperations_::SetSeed(*u, 2.0);
        NativeOperations_::SetSeed(*v, -1.0);
        scope->Reverse();
        const std::array<double, 4> observed{Value(*u), Value(*v), NativeOperations_::ReadAdjoint(x), NativeOperations_::ReadAdjoint(y)};
        if (observed != std::array<double, 4>{6.0, 7.0, 2.0, 3.0})
            return 1;
        scope->ClearAdjoints();
        NativeOperations_::SetSeed(*v, 3.0);
        scope->Reverse();
        const std::array<double, 2> repeated{NativeOperations_::ReadAdjoint(x), NativeOperations_::ReadAdjoint(y)};
        return repeated == std::array<double, 2>{12.0, 3.0} ? 0 : 2;
    }

    int CheckScalar() {
        ProfilingData_ profiling;
        {
            std::optional<ProfilingScope_> profile;
            StartProfile(&profile, &profiling);
            RecordingScope_ scope;
            Number_ x, y;
            scope.RegisterInput(x, 2.0);
            scope.RegisterInput(y, 3.0);
            scope.StartRecording();
            Number_ square = x * x;
            Number_ u = x * y;
            Number_ v = square + y;
            scope.FinishRecording();
            SampleProfile(&profile);
            const int result = ScalarGradients(&scope, x, y, &u, &v);
            if (result != 0)
                return result;
            scope.Close();
            if (!RejectsClosedInput(u))
                return 3;
        }
        return ProfileComplete(profiling) ? 0 : 5;
    }

    int CheckVector() {
        Clear(*Tape());
        auto mode = SetNumResultsForAAD(true, 3);
        RecordingScope_ scope;
        Number_ x, y;
        scope.RegisterInput(x, 2.0);
        scope.RegisterInput(y, 3.0);
        scope.StartRecording();
        Number_ u = x * y;
        Number_ v = x * x + y;
        scope.FinishRecording();
        scope.ClearAdjoints();
        NativeOperations_::SetSeed(u, 2.0, 0);
        NativeOperations_::SetSeed(v, -1.0, 0);
        NativeOperations_::SetSeed(v, 3.0, 1);
        scope.Reverse();
        const std::array<double, 6> observed{NativeOperations_::ReadAdjoint(x, 0), NativeOperations_::ReadAdjoint(y, 0),
                                             NativeOperations_::ReadAdjoint(x, 1), NativeOperations_::ReadAdjoint(y, 1),
                                             NativeOperations_::ReadAdjoint(x, 2), NativeOperations_::ReadAdjoint(y, 2)};
        if (observed != std::array<double, 6>{2.0, 3.0, 12.0, 3.0, 0.0, 0.0})
            return 4;
        scope.Close();
        Clear(*Tape());
        return 0;
    }
} // namespace

int main() {
    constexpr auto capabilities = NativeOperations_::Capabilities();
    static_assert(capabilities.scalarReverse_);
    static_assert(capabilities.vectorAdjoints_);
    static_assert(capabilities.maxAdjointWidth_ >= 3);
    static_assert(capabilities.numberLifetimeDiagnosticsEnabled_ == static_cast<bool>(EXPECT_DIAGNOSTICS));
    static_assert(ProfilingAvailable() == static_cast<bool>(EXPECT_PROFILING));
    const int scalar = CheckScalar();
    if (scalar != 0)
        return scalar;
    const int vector = CheckVector();
    if (vector != 0)
        return vector;
    std::cout << "Installed native scalar/vector gradients and diagnostic/profiling ABI: PASS\n";
}

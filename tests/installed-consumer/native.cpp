//
// Created by Codex on 2026/10/04.
//

#include <array>
#include <iostream>
#include <dal/math/aad/native.hpp>
#include <dal/math/aad/recording.hpp>

#if EXPECT_DIAGNOSTICS && !defined(DAL_ENABLE_AAD_LIFETIME_DIAGNOSTICS)
#error Installed diagnostic definition was not propagated
#elif !EXPECT_DIAGNOSTICS && defined(DAL_ENABLE_AAD_LIFETIME_DIAGNOSTICS)
#error Installed default ABI incorrectly enables diagnostics
#endif

int main() {
    using namespace Dal::AAD;
    constexpr auto capabilities = NativeOperations_::Capabilities();
    static_assert(capabilities.scalarReverse_);
    static_assert(capabilities.vectorAdjoints_);
    static_assert(capabilities.maxAdjointWidth_ >= 3);
    static_assert(capabilities.numberLifetimeDiagnosticsEnabled_ == static_cast<bool>(EXPECT_DIAGNOSTICS));
    {
        RecordingScope_ scope;
        Number_ x, y;
        scope.RegisterInput(x, 2.0);
        scope.RegisterInput(y, 3.0);
        scope.StartRecording();
        Number_ square = x * x;
        Number_ u = x * y;
        Number_ v = square + y;
        scope.FinishRecording();
        scope.ClearAdjoints();
        NativeOperations_::SetSeed(u, 2.0);
        NativeOperations_::SetSeed(v, -1.0);
        scope.Reverse();
        const std::array<double, 4> observed{Value(u), Value(v), NativeOperations_::ReadAdjoint(x), NativeOperations_::ReadAdjoint(y)};
        if (observed != std::array<double, 4>{6.0, 7.0, 2.0, 3.0})
            return 1;
        scope.ClearAdjoints();
        NativeOperations_::SetSeed(v, 3.0);
        scope.Reverse();
        const std::array<double, 2> repeated{NativeOperations_::ReadAdjoint(x), NativeOperations_::ReadAdjoint(y)};
        if (repeated != std::array<double, 2>{12.0, 3.0})
            return 2;
        scope.Close();
#if EXPECT_DIAGNOSTICS
        bool rejected = false;
        try {
            static_cast<void>(NativeOperations_::ReadAdjoint(u));
        } catch (const Dal::Exception_&) {
            rejected = true;
        }
        if (!rejected)
            return 3;
#endif
    }
    Clear(*Tape());
    {
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
    }
    std::cout << "Installed native scalar/vector gradients and diagnostic ABI: PASS\n";
}

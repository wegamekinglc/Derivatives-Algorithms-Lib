//
// Created by Codex on 2026/10/04.
//

#include <gtest/gtest.h>
#include <future>
#include <string>
#include <dal/platform/platform.hpp>
#include <dal/math/aad/backend.hpp>
#include <dal/math/aad/recording.hpp>

using Dal::Exception_;
using Dal::AAD::Adjoint;
using Dal::AAD::BackendAdapter_;
using Dal::AAD::Clear;
using Dal::AAD::Number_;
using Dal::AAD::RecordingScope_;
using Dal::AAD::Tape;
using Dal::AAD::Value;

TEST(AADBackendTest, TestCapabilitiesDescribeCompiledFirstOrderIntegration) {
    constexpr auto capabilities = BackendAdapter_::Capabilities();
    ASSERT_NE(capabilities.backendName_, nullptr);
    ASSERT_TRUE(capabilities.scalarReverse_);
    ASSERT_TRUE(capabilities.repeatedFixedGraphReverse_);
    ASSERT_TRUE(capabilities.intervalReverse_);
    ASSERT_TRUE(capabilities.prefixAccumulation_);
    ASSERT_TRUE(capabilities.scopedLifecycleValidation_);
    ASSERT_FALSE(capabilities.independentNesting_);
    ASSERT_FALSE(capabilities.reverseEvents_);
    ASSERT_FALSE(capabilities.higherOrder_);
#if defined(DAL_USE_XAD_AAD)
    ASSERT_STREQ(capabilities.backendName_, "XAD");
    ASSERT_TRUE(capabilities.prefixReverseDiscardsSuffix_);
#elif defined(DAL_USE_CODIPACK_AAD)
    ASSERT_STREQ(capabilities.backendName_, "CoDiPack");
    ASSERT_FALSE(capabilities.prefixReverseDiscardsSuffix_);
#elif defined(DAL_USE_ADEPT_AAD)
    ASSERT_STREQ(capabilities.backendName_, "Adept");
    ASSERT_FALSE(capabilities.prefixReverseDiscardsSuffix_);
#else
    ASSERT_STREQ(capabilities.backendName_, "Native");
    ASSERT_TRUE(capabilities.vectorAdjoints_);
    ASSERT_EQ(capabilities.maxAdjointWidth_, Dal::AAD::ADJ_SIZE);
    ASSERT_TRUE(capabilities.numberLifetimeDiagnosticsAvailable_);
    ASSERT_FALSE(capabilities.prefixReverseDiscardsSuffix_);
#endif
#if defined(DAL_ENABLE_AAD_LIFETIME_DIAGNOSTICS)
    ASSERT_TRUE(capabilities.numberLifetimeDiagnosticsEnabled_);
#else
    ASSERT_FALSE(capabilities.numberLifetimeDiagnosticsEnabled_);
#endif
#if defined(DAL_USE_XAD_AAD) || defined(DAL_USE_CODIPACK_AAD) || defined(DAL_USE_ADEPT_AAD)
    ASSERT_FALSE(capabilities.vectorAdjoints_);
    ASSERT_EQ(capabilities.maxAdjointWidth_, 1);
    ASSERT_FALSE(capabilities.numberLifetimeDiagnosticsAvailable_);
#endif
}

TEST(AADBackendTest, TestWeightedAndRepeatedSweepsWithMaterializedIntermediate) {
    Clear(*Tape());
    RecordingScope_ scope;
    Number_ x, y;
    scope.RegisterInput(x, 2.0);
    scope.RegisterInput(y, 3.0);
    scope.StartRecording();
    Number_ square = x * x;
    Number_ u = x * y;
    Number_ v = square + y;
    scope.FinishRecording();
    ASSERT_DOUBLE_EQ(Value(u), 6.0);
    ASSERT_DOUBLE_EQ(Value(v), 7.0);
    scope.ClearAdjoints();
    BackendAdapter_::SetSeed(u, 2.0);
    BackendAdapter_::SetSeed(v, -1.0);
    scope.Reverse();
    ASSERT_DOUBLE_EQ(BackendAdapter_::ReadAdjoint(x), 2.0);
    ASSERT_DOUBLE_EQ(BackendAdapter_::ReadAdjoint(y), 3.0);
    scope.ClearAdjoints();
    BackendAdapter_::SetSeed(u, 0.0);
    BackendAdapter_::SetSeed(v, 3.0);
    scope.Reverse();
    ASSERT_DOUBLE_EQ(BackendAdapter_::ReadAdjoint(x), 12.0);
    ASSERT_DOUBLE_EQ(BackendAdapter_::ReadAdjoint(y), 3.0);
    scope.Close();
    Clear(*Tape());
}

TEST(AADBackendTest, TestAliasWeightsAccumulateAndSetReplacesSeed) {
    Clear(*Tape());
    RecordingScope_ scope;
    Number_ x, y;
    scope.RegisterInput(x, 2.0);
    scope.RegisterInput(y, 3.0);
    scope.StartRecording();
    Number_ u = x * y;
    Number_& alias = u;
    scope.FinishRecording();
    scope.ClearAdjoints();
    BackendAdapter_::SetSeed(u, 9.0);
    BackendAdapter_::SetSeed(u, 2.0);
    BackendAdapter_::AddSeed(alias, -1.0);
    ASSERT_DOUBLE_EQ(BackendAdapter_::ReadAdjoint(u), 1.0);
    scope.Reverse();
    ASSERT_DOUBLE_EQ(BackendAdapter_::ReadAdjoint(x), 3.0);
    ASSERT_DOUBLE_EQ(BackendAdapter_::ReadAdjoint(y), 2.0);
    scope.Close();
    Clear(*Tape());
}

TEST(AADBackendTest, TestConstantAndIdentityOutputsUseActiveRoots) {
    Clear(*Tape());
    RecordingScope_ scope;
    Number_ x;
    scope.RegisterInput(x, 2.0);
    scope.StartRecording();
    Number_ activeZero = x * 0.0;
    Number_ constant = BackendAdapter_::ActiveRoot(Number_(5.0), activeZero);
    Number_ identity = BackendAdapter_::ActiveRoot(x, activeZero);
    scope.FinishRecording();
    scope.ClearAdjoints();
    BackendAdapter_::SetSeed(constant, 7.0);
    BackendAdapter_::SetSeed(identity, 3.0);
    scope.Reverse();
    ASSERT_DOUBLE_EQ(Value(constant), 5.0);
    ASSERT_DOUBLE_EQ(Value(identity), 2.0);
    ASSERT_DOUBLE_EQ(BackendAdapter_::ReadAdjoint(x), 3.0);
    scope.Close();
    Clear(*Tape());
}

TEST(AADBackendTest, TestInvalidModeAndChannelPreserveScalarGraph) {
    Clear(*Tape());
    RecordingScope_ scope;
    Number_ x;
    scope.RegisterInput(x, 2.0);
    scope.StartRecording();
    Number_ output = x * x;
    scope.FinishRecording();
    scope.ClearAdjoints();
    BackendAdapter_::SetSeed(output, 2.0);
    ASSERT_THROW(BackendAdapter_::SetSeed(output, 9.0, 1), Exception_);
    ASSERT_THROW(BackendAdapter_::AddSeed(output, 9.0, 1), Exception_);
    ASSERT_THROW(static_cast<void>(BackendAdapter_::ReadAdjoint(output, 1)), Exception_);
    try {
        BackendAdapter_::SetSeed(output, 9.0, 1);
        FAIL() << "invalid channel was accepted";
    } catch (const Exception_& error) {
        const std::string message = error.what();
        ASSERT_NE(message.find("SetSeed"), std::string::npos);
        ASSERT_NE(message.find(BackendAdapter_::Capabilities().backendName_), std::string::npos);
    }
    ASSERT_THROW(BackendAdapter_::ValidateAdjointMode(false, 0), Exception_);
    ASSERT_THROW(BackendAdapter_::ValidateAdjointMode(false, 2), Exception_);
    ASSERT_THROW(BackendAdapter_::ValidateAdjointMode(true, 0), Exception_);
#if defined(DAL_USE_XAD_AAD) || defined(DAL_USE_CODIPACK_AAD) || defined(DAL_USE_ADEPT_AAD)
    ASSERT_THROW(BackendAdapter_::ValidateAdjointMode(true, 1), Exception_);
#else
    ASSERT_THROW(BackendAdapter_::ValidateAdjointMode(true, Dal::AAD::ADJ_SIZE + 1), Exception_);
#endif
    BackendAdapter_::ValidateAdjointMode(false, 1);
    ASSERT_DOUBLE_EQ(BackendAdapter_::ReadAdjoint(output), 2.0);
    scope.Reverse();
    ASSERT_DOUBLE_EQ(BackendAdapter_::ReadAdjoint(x), 8.0);
    scope.Close();
    Clear(*Tape());
}

TEST(AADBackendTest, TestPrefixAccumulatesThreeRestoredSuffixes) {
    Clear(*Tape());
    RecordingScope_ scope;
    Number_ x;
    scope.RegisterInput(x, 2.0);
    scope.StartRecording();
    Number_ prefix = x * x;
    const auto checkpoint = scope.MakeCheckpoint();
    scope.ClearAdjoints();
    double sum = 0.0;
    for (double weight : {1.0, 2.0, 3.0}) {
        scope.Restore(checkpoint);
        Number_ output = prefix * weight;
        sum += Value(output);
        scope.FinishRecording();
        BackendAdapter_::SetSeed(output, 1.0);
        scope.ReverseSuffix(checkpoint);
    }
    ASSERT_DOUBLE_EQ(sum, 24.0);
    ASSERT_DOUBLE_EQ(BackendAdapter_::ReadAdjoint(prefix), 6.0);
    scope.ReversePrefix(checkpoint);
    ASSERT_DOUBLE_EQ(BackendAdapter_::ReadAdjoint(x), 24.0);
    scope.Close();
    Clear(*Tape());
}

#if !defined(DAL_USE_XAD_AAD) && !defined(DAL_USE_CODIPACK_AAD) && !defined(DAL_USE_ADEPT_AAD)
TEST(AADBackendTest, TestNativeChannelsUseVectorStorageAcrossWidths) {
    for (size_t width : {1U, 3U, 5U, 7U, 10U, 16U}) {
        SCOPED_TRACE(width);
        Clear(*Tape());
        BackendAdapter_::ValidateAdjointMode(true, width);
        auto mode = Dal::AAD::SetNumResultsForAAD(true, width);
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
        Adjoint(u) = 99.0;
        BackendAdapter_::SetSeed(u, 2.0, 0);
        BackendAdapter_::AddSeed(u, 1.0, 0);
        BackendAdapter_::AddSeed(u, -1.0, 0);
        BackendAdapter_::SetSeed(v, -1.0, 0);
        ASSERT_DOUBLE_EQ(Adjoint(u), 99.0);
        ASSERT_DOUBLE_EQ(BackendAdapter_::ReadAdjoint(u, 0), 2.0);
        if (width > 1)
            BackendAdapter_::SetSeed(v, 3.0, 1);
        ASSERT_THROW(BackendAdapter_::SetSeed(u, 9.0, width), Exception_);
        ASSERT_THROW(BackendAdapter_::AddSeed(u, 9.0, width), Exception_);
        ASSERT_THROW(static_cast<void>(BackendAdapter_::ReadAdjoint(u, width)), Exception_);
        scope.Reverse();
        ASSERT_DOUBLE_EQ(BackendAdapter_::ReadAdjoint(x, 0), 2.0);
        ASSERT_DOUBLE_EQ(BackendAdapter_::ReadAdjoint(y, 0), 3.0);
        if (width > 1) {
            ASSERT_DOUBLE_EQ(BackendAdapter_::ReadAdjoint(x, 1), 12.0);
            ASSERT_DOUBLE_EQ(BackendAdapter_::ReadAdjoint(y, 1), 3.0);
        }
        for (size_t channel = 2; channel < width; ++channel) {
            ASSERT_DOUBLE_EQ(BackendAdapter_::ReadAdjoint(x, channel), 0.0);
            ASSERT_DOUBLE_EQ(BackendAdapter_::ReadAdjoint(y, channel), 0.0);
        }
        scope.Close();
        Clear(*Tape());
    }
}

TEST(AADBackendTest, TestNativeUnboundNumberRejectsChannelAccess) {
    Clear(*Tape());
    Number_ unbound;
    ASSERT_THROW(BackendAdapter_::SetSeed(unbound, 1.0), Exception_);
    ASSERT_THROW(BackendAdapter_::AddSeed(unbound, 1.0), Exception_);
    ASSERT_THROW(static_cast<void>(BackendAdapter_::ReadAdjoint(unbound)), Exception_);
    ASSERT_DOUBLE_EQ(Value(unbound), 0.0);
    Clear(*Tape());
}

TEST(AADBackendTest, TestNativeScalarNodeRejectsMissingVectorStorageAndGraphRecovers) {
    Clear(*Tape());
    Number_ x(3.0);
    Number_ output = x * x;
    {
        auto mode = Dal::AAD::SetNumResultsForAAD(true, 3);
        ASSERT_THROW(BackendAdapter_::SetSeed(output, 1.0), Exception_);
        ASSERT_THROW(BackendAdapter_::AddSeed(output, 1.0), Exception_);
        ASSERT_THROW(static_cast<void>(BackendAdapter_::ReadAdjoint(output)), Exception_);
    }
    ASSERT_DOUBLE_EQ(Value(output), 9.0);
    ASSERT_DOUBLE_EQ(BackendAdapter_::ReadAdjoint(output), 0.0);
    BackendAdapter_::SetSeed(output, 1.0);
    BackendAdapter_::REVERSE(*Tape());
    ASSERT_DOUBLE_EQ(BackendAdapter_::ReadAdjoint(x), 6.0);
    Clear(*Tape());
}
#endif

#if defined(DAL_ENABLE_AAD_LIFETIME_DIAGNOSTICS)
TEST(AADBackendTest, TestDiagnosticChannelAccessRejectsDiscardedAndForeignNumbers) {
    Clear(*Tape());
    RecordingScope_ scope;
    Number_ x;
    scope.RegisterInput(x, 2.0);
    scope.StartRecording();
    Number_ prefix = x * x;
    const auto checkpoint = scope.MakeCheckpoint();
    Number_ discarded = prefix * 3.0;
    scope.Restore(checkpoint);
    ASSERT_THROW(BackendAdapter_::SetSeed(discarded, 1.0), Exception_);
    ASSERT_THROW(BackendAdapter_::AddSeed(discarded, 1.0), Exception_);
    ASSERT_THROW(static_cast<void>(BackendAdapter_::ReadAdjoint(discarded)), Exception_);
    auto foreign = std::async(std::launch::async, [&] {
        ASSERT_THROW(BackendAdapter_::SetSeed(prefix, 1.0), Exception_);
        ASSERT_THROW(BackendAdapter_::AddSeed(prefix, 1.0), Exception_);
        ASSERT_THROW(static_cast<void>(BackendAdapter_::ReadAdjoint(prefix)), Exception_);
    });
    foreign.get();
    scope.Close();
    Clear(*Tape());
    ASSERT_THROW(BackendAdapter_::SetSeed(prefix, 1.0), Exception_);
    ASSERT_THROW(BackendAdapter_::AddSeed(prefix, 1.0), Exception_);
    ASSERT_THROW(static_cast<void>(BackendAdapter_::ReadAdjoint(prefix)), Exception_);
}
#endif

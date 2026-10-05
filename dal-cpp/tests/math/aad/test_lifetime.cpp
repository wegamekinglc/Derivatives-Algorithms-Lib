//
// Created by Codex on 2026/10/04.
//

#include <gtest/gtest.h>

#include <future>
#include <limits>
#include <string>
#include <vector>

#include <dal/math/aad/aad.hpp>
#include <dal/platform/platform.hpp>

#if defined(DAL_ENABLE_AAD_LIFETIME_DIAGNOSTICS)

#include <dal/math/aad/recording.hpp>
#include <dal/model/surface/lvmodel.hpp>

using namespace Dal::AAD;

namespace Dal::AAD {
    struct NativeLifetimeTestAccess_ {
        static TapNode_* Node(const Number_& number) { return number.node_; }
        static std::uint64_t Epoch(const Tape_& tape) { return tape.lifetimeEpoch_; }
        static void SetEpoch(Tape_* tape, std::uint64_t value) { tape->lifetimeEpoch_ = value; }
        static std::uint64_t Generation(const Tape_& tape) { return tape.lifetimeGeneration_; }
        static void SetGeneration(Tape_* tape, std::uint64_t value) { tape->lifetimeGeneration_ = value; }
        static std::uint64_t LiveNodes(const Tape_& tape) { return tape.liveNodes_; }
        static void SetLiveNodes(Tape_* tape, std::uint64_t value) { tape->liveNodes_ = value; }
        static std::uint64_t ClaimIdentity(std::atomic<std::uint64_t>* next) { return Tape_::ClaimLifetimeIdentity(next); }
    };
} // namespace Dal::AAD

namespace {
    template <class F_> std::string LifetimeError(const F_& operation) {
        try {
            operation();
        } catch (const Dal::Exception_& error) {
            return error.what();
        }
        return {};
    }
} // namespace

TEST(AADLifetimeTest, TestSmoothScalarGraphRetainsAnalyticDerivative) {
    Clear(*Tape());
    Number_ x(2.0);
    PutOnTape(x);
    NewRecording(*Tape());
    Number_ y = x * x + 3.0 * x;
    Adjoint(y) = 1.0;
    PropagateToStart(*Tape());
    ASSERT_DOUBLE_EQ(Value(y), 10.0);
    ASSERT_DOUBLE_EQ(Adjoint(x), 7.0);
    Clear(*Tape());
}

TEST(AADLifetimeTest, TestFullRewindRejectsOldAdjointBeforeSlotReuse) {
    Clear(*Tape());
    Number_ old(3.0);
    Rewind(*Tape());
    ASSERT_DOUBLE_EQ(Value(old), 3.0);
    ASSERT_THROW(Adjoint(old), Dal::Exception_);
    Clear(*Tape());
}

TEST(AADLifetimeTest, TestSuffixRestoreRejectsOldAdjointBeforeSlotReuse) {
    Clear(*Tape());
    Number_ prefix(2.0);
    Mark(*Tape());
    Number_ suffix = prefix * 3.0;
    RewindToMark(*Tape());
    ASSERT_THROW(Adjoint(suffix), Dal::Exception_);
    Number_ fresh = prefix * 5.0;
    Adjoint(fresh) = 1.0;
    PropagateToMark(*Tape());
    ASSERT_DOUBLE_EQ(Adjoint(prefix), 5.0);
    Clear(*Tape());
}

TEST(AADLifetimeTest, TestStaleExpressionAssignmentPreservesDestinationAndGraph) {
    Clear(*Tape());
    Number_ prefix(2.0);
    Mark(*Tape());
    Number_ suffix = prefix * 3.0;
    RewindToMark(*Tape());
    Number_ destination = prefix * 4.0;
    const auto occupied = Tape()->nodes_.OccupiedSlots();
    ASSERT_THROW(destination = suffix * 2.0, Dal::Exception_);
    ASSERT_DOUBLE_EQ(Value(destination), 8.0);
    ASSERT_EQ(Tape()->nodes_.OccupiedSlots(), occupied);
    Adjoint(destination) = 1.0;
    PropagateToMark(*Tape());
    ASSERT_DOUBLE_EQ(Adjoint(prefix), 4.0);
    Clear(*Tape());
}

TEST(AADLifetimeTest, TestClearRejectsFreedHandlesAfterFreshAllocations) {
    Clear(*Tape());
    Number_ old(3.0);
    const Number_ copied = old;
    Clear(*Tape());
    Number_ fresh(4.0);
    ASSERT_THROW(Adjoint(old), Dal::Exception_);
    ASSERT_THROW(static_cast<void>(Number_(copied + fresh)), Dal::Exception_);
    ASSERT_DOUBLE_EQ(Value(copied), 3.0);
    Number_ output = fresh * fresh;
    Adjoint(output) = 1.0;
    PropagateToStart(*Tape());
    ASSERT_DOUBLE_EQ(Adjoint(fresh), 8.0);
    Clear(*Tape());
}

TEST(AADLifetimeTest, TestSameAddressReuseRejectsCopiedOldGeneration) {
    Clear(*Tape());
    Number_ prefix(2.0);
    Mark(*Tape());
    Number_ old = prefix * 3.0;
    const Number_ copied = old;
    auto* address = NativeLifetimeTestAccess_::Node(old);
    RewindToMark(*Tape());
    Number_ fresh = prefix * 5.0;
    ASSERT_EQ(NativeLifetimeTestAccess_::Node(fresh), address);
    const auto message = LifetimeError([&] { Adjoint(copied) = 1.0; });
    ASSERT_NE(message.find("Number.Adjoint"), std::string::npos);
    ASSERT_NE(message.find("generation"), std::string::npos);
    ASSERT_NE(message.find("slot="), std::string::npos);
    Adjoint(fresh) = 1.0;
    PropagateToMark(*Tape());
    ASSERT_DOUBLE_EQ(Adjoint(prefix), 5.0);
    Clear(*Tape());
}

TEST(AADLifetimeTest, TestPrefixAccumulatesThreeIndependentSuffixes) {
    Clear(*Tape());
    Number_ input(2.0);
    Number_ prefix = input * input;
    Mark(*Tape());
    const double factors[] = {3.0, 5.0, -1.0};
    double accumulated = 0.0;
    for (const double factor : factors) {
        RewindToMark(*Tape());
        Number_ result = prefix * factor;
        Adjoint(result) = 1.0;
        PropagateToMark(*Tape());
        accumulated += factor;
        ASSERT_DOUBLE_EQ(Adjoint(prefix), accumulated);
    }
    PropagateMarkToStart(*Tape());
    ASSERT_DOUBLE_EQ(Adjoint(input), 28.0);
    Clear(*Tape());
}

TEST(AADLifetimeTest, TestSavedNestedExpressionRejectsRestoredSuffix) {
    Clear(*Tape());
    Number_ prefix(2.0);
    Mark(*Tape());
    Number_ suffix = prefix * 3.0;
    const auto saved = exp(prefix + suffix) + prefix;
    RewindToMark(*Tape());
    const auto occupied = Tape()->nodes_.OccupiedSlots();
    ASSERT_THROW(static_cast<void>(Number_(saved)), Dal::Exception_);
    ASSERT_EQ(Tape()->nodes_.OccupiedSlots(), occupied);
    ASSERT_DOUBLE_EQ(Adjoint(prefix), 0.0);
    Clear(*Tape());
}

TEST(AADLifetimeTest, TestCompoundAssignmentRejectsStaleContainerOperand) {
    Clear(*Tape());
    Number_ prefix(2.0);
    Mark(*Tape());
    std::vector<Number_> old{prefix * 3.0};
    RewindToMark(*Tape());
    Number_ destination = prefix * 4.0;
    ASSERT_THROW(destination += old.front(), Dal::Exception_);
    ASSERT_DOUBLE_EQ(Value(destination), 8.0);
    Adjoint(destination) = 1.0;
    PropagateToMark(*Tape());
    ASSERT_DOUBLE_EQ(Adjoint(prefix), 4.0);
    Clear(*Tape());
}

TEST(AADLifetimeTest, TestCopiedModelSurfaceRejectsDiscardedParameterBindings) {
    Clear(*Tape());
    Dal::LocalVolSurfaceData_ data("lifetime-surface", {90.0, 110.0}, {0.5, 1.0}, Dal::Matrix_<>(2, 2, 0.2));
    LocalVolSurface_<Number_> original(data);
    LocalVolSurface_<Number_> copied(original);
    Rewind(*Tape());
    Number_ spot(100.0);
    ASSERT_THROW(static_cast<void>(copied.Vol(0.75, spot)), Dal::Exception_);
    ASSERT_DOUBLE_EQ(Adjoint(spot), 0.0);
    Clear(*Tape());
}

TEST(AADLifetimeTest, TestFreshRegistrationStartsIndependentChainAfterReset) {
    Clear(*Tape());
    Number_ input(3.0);
    Number_ discarded = input * input;
    Rewind(*Tape());
    RegisterIndependent(discarded, Value(discarded));
    NewRecording(*Tape());
    Number_ result = discarded * 5.0;
    Adjoint(result) = 1.0;
    PropagateToStart(*Tape());
    ASSERT_DOUBLE_EQ(Value(result), 45.0);
    ASSERT_DOUBLE_EQ(Adjoint(discarded), 5.0);
    ASSERT_THROW(Adjoint(input), Dal::Exception_);
    Clear(*Tape());
}

TEST(AADLifetimeTest, TestPutOnTapeRebindsPassiveValueAfterClear) {
    Clear(*Tape());
    Number_ input(3.0);
    Number_ discarded = input * input;
    Clear(*Tape());
    PutOnTape(discarded);
    Number_ result = discarded * 7.0;
    Adjoint(result) = 1.0;
    PropagateToStart(*Tape());
    ASSERT_DOUBLE_EQ(Value(result), 63.0);
    ASSERT_DOUBLE_EQ(Adjoint(discarded), 7.0);
    Clear(*Tape());
}

TEST(AADLifetimeTest, TestMissingBindingRejectsActiveUseAndKeepsPassiveValue) {
    Clear(*Tape());
    Number_ empty;
    ASSERT_DOUBLE_EQ(Value(empty), 0.0);
    ASSERT_THROW(Adjoint(empty), Dal::Exception_);
    ASSERT_THROW(static_cast<void>(Number_(empty + 1.0)), Dal::Exception_);
    empty = 2.0;
    Number_ result = empty * 3.0;
    Adjoint(result) = 1.0;
    PropagateToStart(*Tape());
    ASSERT_DOUBLE_EQ(Adjoint(empty), 3.0);
    Clear(*Tape());
}

TEST(AADLifetimeTest, TestForeignLiveThreadRejectsBeforeNodeAccess) {
    Clear(*Tape());
    std::promise<Number_> published;
    std::promise<void> release;
    auto done = release.get_future();
    auto worker = std::async(std::launch::async, [&] {
        Clear(*Tape());
        Number_ foreign(4.0);
        published.set_value(foreign);
        done.wait();
        Clear(*Tape());
    });
    Number_ foreign = published.get_future().get();
    const auto adjointError = LifetimeError([&] { Adjoint(foreign) = 1.0; });
    const auto expressionError = LifetimeError([&] { Number_ result = foreign * 2.0; });
    release.set_value();
    worker.get();
    ASSERT_NE(adjointError.find("another tape lifetime"), std::string::npos);
    ASSERT_NE(expressionError.find("another tape lifetime"), std::string::npos);
    ASSERT_DOUBLE_EQ(Value(foreign), 4.0);
    Clear(*Tape());
}

TEST(AADLifetimeTest, TestExitedThreadRejectsWithoutDereferencingFreedTape) {
    Clear(*Tape());
    auto worker = std::async(std::launch::async, [] {
        Clear(*Tape());
        return Number_(4.0);
    });
    Number_ foreign = worker.get();
    ASSERT_THROW(Adjoint(foreign), Dal::Exception_);
    ASSERT_THROW(static_cast<void>(Number_(foreign * 2.0)), Dal::Exception_);
    PutOnTape(foreign);
    Number_ result = foreign * 5.0;
    Adjoint(result) = 1.0;
    PropagateToStart(*Tape());
    ASSERT_DOUBLE_EQ(Adjoint(foreign), 5.0);
    Clear(*Tape());
}

TEST(AADLifetimeTest, TestRawModeChangeRejectsBeforeScalarVectorStorageAccess) {
    Clear(*Tape());
    Number_ input(2.0);
    Tape()->multi_ = true;
    const auto adjointError = LifetimeError([&] { Adjoint(input) = 1.0; });
    const auto zeroError = LifetimeError([&] { ZeroAdjoints(*Tape()); });
    const auto reverseError = LifetimeError([&] { PropagateToStart(*Tape()); });
    Tape()->multi_ = false;
    ASSERT_NE(adjointError.find("mode"), std::string::npos);
    ASSERT_NE(zeroError.find("mode"), std::string::npos);
    ASSERT_NE(reverseError.find("mode"), std::string::npos);
    ASSERT_DOUBLE_EQ(Adjoint(input), 0.0);
    Clear(*Tape());
}

TEST(AADLifetimeTest, TestRawWidthChangeRejectsBeforeVectorStorageAccess) {
    Clear(*Tape());
    auto mode = SetNumResultsForAAD(true, 2);
    Number_ input(2.0);
    Tape()->numAdj_ = 3;
    const auto message = LifetimeError([&] { Number_ result = input * 3.0; });
    Tape()->numAdj_ = 2;
    ASSERT_NE(message.find("width"), std::string::npos);
    ASSERT_EQ(Tape()->nodes_.OccupiedSlots(), 1);
    Clear(*Tape());
}

TEST(AADLifetimeTest, TestVectorGraphRetainsAnalyticChannelsAndFreshRegistration) {
    Clear(*Tape());
    auto mode = SetNumResultsForAAD(true, 3);
    Number_ input(2.0);
    PutOnTape(input);
    NewRecording(*Tape());
    Number_ output = input * input;
    NativeLifetimeTestAccess_::Node(output)->Adjoint(0) = 1.0;
    NativeLifetimeTestAccess_::Node(output)->Adjoint(1) = -2.0;
    PropagateToStart(*Tape());
    ASSERT_DOUBLE_EQ(NativeLifetimeTestAccess_::Node(input)->Adjoint(0), 4.0);
    ASSERT_DOUBLE_EQ(NativeLifetimeTestAccess_::Node(input)->Adjoint(1), -8.0);
    ASSERT_DOUBLE_EQ(NativeLifetimeTestAccess_::Node(input)->Adjoint(2), 0.0);
    Clear(*Tape());
}

TEST(AADLifetimeTest, TestFullFinalBlockPrefixAndRestoredBoundaryAreValid) {
    Clear(*Tape());
    Number_ prefix(2.0);
    for (size_t i = 1; i != BLOCK_SIZE; ++i)
        Tape()->RecordNode<0>();
    Mark(*Tape());
    Number_ suffix = prefix * 3.0;
    RewindToMark(*Tape());
    ASSERT_EQ(Tape()->nodes_.Size(), static_cast<int>(BLOCK_SIZE));
    ASSERT_THROW(Adjoint(suffix), Dal::Exception_);
    Number_ fresh = prefix * 5.0;
    Adjoint(fresh) = 1.0;
    PropagateToMark(*Tape());
    ASSERT_DOUBLE_EQ(Adjoint(prefix), 5.0);
    Clear(*Tape());
}

TEST(AADLifetimeTest, TestGenerationExhaustionRejectsBeforeNodeAllocation) {
    Clear(*Tape());
    Number_ input(2.0);
    const auto previous = NativeLifetimeTestAccess_::Generation(*Tape());
    NativeLifetimeTestAccess_::SetGeneration(Tape(), std::numeric_limits<std::uint64_t>::max());
    const auto message = LifetimeError([] { Number_ extra(3.0); });
    NativeLifetimeTestAccess_::SetGeneration(Tape(), previous);
    ASSERT_NE(message.find("generation exhausted"), std::string::npos);
    ASSERT_EQ(Tape()->nodes_.OccupiedSlots(), 1);
    ASSERT_DOUBLE_EQ(Adjoint(input), 0.0);
    Clear(*Tape());
}

TEST(AADLifetimeTest, TestNodeCountExhaustionRejectsBeforeNodeAllocation) {
    Clear(*Tape());
    Number_ input(2.0);
    NativeLifetimeTestAccess_::SetLiveNodes(Tape(), std::numeric_limits<std::uint64_t>::max());
    const auto message = LifetimeError([] { Number_ extra(3.0); });
    NativeLifetimeTestAccess_::SetLiveNodes(Tape(), 1);
    ASSERT_NE(message.find("node count exhausted"), std::string::npos);
    ASSERT_EQ(Tape()->nodes_.OccupiedSlots(), 1);
    ASSERT_DOUBLE_EQ(Adjoint(input), 0.0);
    Clear(*Tape());
}

TEST(AADLifetimeTest, TestGenerationExhaustionRebindPreservesExistingValueAndGraph) {
    Clear(*Tape());
    Number_ input(2.0);
    Number_ output = input * 3.0;
    const auto previous = NativeLifetimeTestAccess_::Generation(*Tape());
    NativeLifetimeTestAccess_::SetGeneration(Tape(), std::numeric_limits<std::uint64_t>::max());
    const auto message = LifetimeError([&] { output = 99.0; });
    NativeLifetimeTestAccess_::SetGeneration(Tape(), previous);
    ASSERT_NE(message.find("generation exhausted"), std::string::npos);
    ASSERT_DOUBLE_EQ(Value(output), 6.0);
    ASSERT_EQ(Tape()->nodes_.OccupiedSlots(), 2);
    Adjoint(output) = 1.0;
    PropagateToStart(*Tape());
    ASSERT_DOUBLE_EQ(Adjoint(input), 3.0);
    Clear(*Tape());
}

TEST(AADLifetimeTest, TestNodeCountExhaustionRegistrationPreservesExistingValueAndGraph) {
    Clear(*Tape());
    Number_ input(2.0);
    Number_ output = input * 3.0;
    NativeLifetimeTestAccess_::SetLiveNodes(Tape(), std::numeric_limits<std::uint64_t>::max());
    const auto message = LifetimeError([&] { RegisterIndependent(output, 99.0); });
    NativeLifetimeTestAccess_::SetLiveNodes(Tape(), 2);
    ASSERT_NE(message.find("node count exhausted"), std::string::npos);
    ASSERT_DOUBLE_EQ(Value(output), 6.0);
    ASSERT_EQ(Tape()->nodes_.OccupiedSlots(), 2);
    Adjoint(output) = 1.0;
    PropagateToStart(*Tape());
    ASSERT_DOUBLE_EQ(Adjoint(input), 3.0);
    Clear(*Tape());
}

TEST(AADLifetimeTest, TestGenerationExhaustionExpressionAssignmentPreservesExistingGraph) {
    Clear(*Tape());
    Number_ input(2.0);
    Number_ output = input * 3.0;
    const auto previous = NativeLifetimeTestAccess_::Generation(*Tape());
    NativeLifetimeTestAccess_::SetGeneration(Tape(), std::numeric_limits<std::uint64_t>::max());
    const auto message = LifetimeError([&] { output = input * 5.0; });
    NativeLifetimeTestAccess_::SetGeneration(Tape(), previous);
    ASSERT_NE(message.find("generation exhausted"), std::string::npos);
    ASSERT_DOUBLE_EQ(Value(output), 6.0);
    ASSERT_EQ(Tape()->nodes_.OccupiedSlots(), 2);
    Adjoint(output) = 1.0;
    PropagateToStart(*Tape());
    ASSERT_DOUBLE_EQ(Adjoint(input), 3.0);
    Clear(*Tape());
}

TEST(AADLifetimeTest, TestFailedScopedRegistrationRejectsWorkAndNextRecordingRecovers) {
    Clear(*Tape());
    {
        RecordingScope_ recording;
        Number_ input;
        recording.RegisterInput(input, 2.0);
        const auto previous = NativeLifetimeTestAccess_::Generation(*Tape());
        NativeLifetimeTestAccess_::SetGeneration(Tape(), std::numeric_limits<std::uint64_t>::max());
        const auto message = LifetimeError([&] { recording.RegisterInput(input, 99.0); });
        NativeLifetimeTestAccess_::SetGeneration(Tape(), previous);
        ASSERT_NE(message.find("generation exhausted"), std::string::npos);
        ASSERT_DOUBLE_EQ(Value(input), 2.0);
        ASSERT_TRUE(LastRecordingCleanupFailure());
        ASSERT_THROW(recording.StartRecording(), Dal::Exception_);
        recording.Close();
    }
    RecordingScope_ recovered;
    ASSERT_FALSE(LastRecordingCleanupFailure());
    Number_ input;
    recovered.RegisterInput(input, 3.0);
    recovered.StartRecording();
    Number_ output = input * input;
    recovered.FinishRecording();
    Adjoint(output) = 1.0;
    recovered.Reverse();
    ASSERT_DOUBLE_EQ(Adjoint(input), 6.0);
    recovered.Close();
}

TEST(AADLifetimeTest, TestEpochExhaustionRejectsBeforeStorageRelease) {
    Clear(*Tape());
    Number_ input(2.0);
    const auto previous = NativeLifetimeTestAccess_::Epoch(*Tape());
    NativeLifetimeTestAccess_::SetEpoch(Tape(), std::numeric_limits<std::uint64_t>::max());
    const auto clearError = LifetimeError([] { Clear(*Tape()); });
    const auto rewindError = LifetimeError([] { Rewind(*Tape()); });
    NativeLifetimeTestAccess_::SetEpoch(Tape(), previous);
    ASSERT_NE(clearError.find("epoch exhausted"), std::string::npos);
    ASSERT_NE(rewindError.find("epoch exhausted"), std::string::npos);
    ASSERT_EQ(Tape()->nodes_.OccupiedSlots(), 1);
    ASSERT_DOUBLE_EQ(Adjoint(input), 0.0);
    Clear(*Tape());
}

TEST(AADLifetimeTest, TestIdentityExhaustionNeverWrapsOrReusesZero) {
    std::atomic<std::uint64_t> next{std::numeric_limits<std::uint64_t>::max()};
    ASSERT_EQ(NativeLifetimeTestAccess_::ClaimIdentity(&next), std::numeric_limits<std::uint64_t>::max());
    ASSERT_EQ(next.load(), 0);
    ASSERT_THROW(NativeLifetimeTestAccess_::ClaimIdentity(&next), Dal::Exception_);
    ASSERT_EQ(next.load(), 0);
}

TEST(AADLifetimeTest, TestPartialAllocationFailureRequiresResetBeforeNewBinding) {
    Clear(*Tape());
    Tape()->multi_ = true;
    Tape()->numAdj_ = ADJ_SIZE + 1;
    const auto failed = LifetimeError([] { Number_ invalid(2.0); });
    Tape()->multi_ = false;
    Tape()->numAdj_ = 1;
    ASSERT_FALSE(failed.empty());
    ASSERT_EQ(Tape()->nodes_.OccupiedSlots(), 1);
    ASSERT_THROW(static_cast<void>(Number_(3.0)), Dal::Exception_);
    Clear(*Tape());
    Number_ fresh(3.0);
    Number_ result = fresh * 2.0;
    Adjoint(result) = 1.0;
    PropagateToStart(*Tape());
    ASSERT_DOUBLE_EQ(Adjoint(fresh), 2.0);
    Clear(*Tape());
}

#endif

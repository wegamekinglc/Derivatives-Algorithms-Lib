//
// Created on 2026/10/04.
//

#include <dal/math/aad/recording.hpp>
#include <dal/platform/platform.hpp>
#include <dal/utilities/exceptions.hpp>
#include <future>
#include <gtest/gtest.h>

using namespace Dal;
using namespace Dal::AAD;

namespace Dal::AAD {
    struct RecordingStateTestAccess_ {
        static void FailReverse(RecordingScope_& recording) {
            recording.ReverseUsing([](Tape_& tape) {
                PropagateToStart(tape);
                THROW("controlled backend reverse failure");
            });
        }
    };
} // namespace Dal::AAD

TEST(AADRecordingStateTest, TestScopedScalarGraphSupportsFreshWeightedSweeps) {
    RecordingScope_ recording;
    Number_ input;
    recording.RegisterInput(input, 2.0);
    recording.StartRecording();
    Number_ output = input * input + 3.0 * input;
    recording.FinishRecording();
    ASSERT_DOUBLE_EQ(Value(output), 10.0);
    recording.ClearAdjoints();
    Adjoint(output) = 1.0;
    recording.Reverse();
    ASSERT_DOUBLE_EQ(AdjointValue(input), 7.0);
    recording.ClearAdjoints();
    Adjoint(output) = 2.0;
    recording.Reverse();
    ASSERT_DOUBLE_EQ(AdjointValue(input), 14.0);
    recording.Close();
}

TEST(AADRecordingStateTest, TestPhaseErrorsLeaveTheValidGraphUsable) {
    RecordingScope_ recording;
    ASSERT_THROW(recording.Reverse(), Exception_);
    ASSERT_THROW(recording.FinishRecording(), Exception_);
    ASSERT_THROW(recording.ClearAdjoints(), Exception_);
    Number_ input;
    recording.RegisterInput(input, 3.0);
    recording.StartRecording();
    Number_ output = input * input;
    Number_ lateInput;
    ASSERT_THROW(recording.RegisterInput(lateInput, 7.0), Exception_);
    ASSERT_THROW(recording.StartRecording(), Exception_);
    ASSERT_THROW(recording.Reverse(), Exception_);
    recording.FinishRecording();
    ASSERT_THROW(recording.FinishRecording(), Exception_);
    recording.ClearAdjoints();
    Adjoint(output) = 1.0;
    recording.Reverse();
    ASSERT_DOUBLE_EQ(AdjointValue(input), 6.0);
    recording.Close();
    ASSERT_THROW(recording.StartRecording(), Exception_);
    ASSERT_THROW(recording.FinishRecording(), Exception_);
    ASSERT_THROW(recording.ClearAdjoints(), Exception_);
    ASSERT_THROW(recording.Reverse(), Exception_);
    ASSERT_THROW(recording.RegisterInput(lateInput, 7.0), Exception_);
}

TEST(AADRecordingStateTest, TestCheckpointSuffixesAccumulateBeforeOnePrefixSweep) {
    RecordingScope_ recording;
    Number_ input;
    recording.RegisterInput(input, 2.0);
    recording.StartRecording();
    Number_ prefix = input * input;
    const auto checkpoint = recording.MakeCheckpoint();
    double sum = 0.0;
    for (int weight = 1; weight <= 3; ++weight) {
        recording.Restore(checkpoint);
        Number_ output = prefix * static_cast<double>(weight);
        recording.FinishRecording();
        sum += Value(output);
        Adjoint(output) = 1.0;
        recording.ReverseSuffix(checkpoint);
    }
    ASSERT_DOUBLE_EQ(sum, 24.0);
    ASSERT_DOUBLE_EQ(AdjointValue(prefix), 6.0);
    recording.ReversePrefix(checkpoint);
    ASSERT_DOUBLE_EQ(AdjointValue(input), 24.0);
    recording.Close();
}

TEST(AADRecordingStateTest, TestInvalidCheckpointHandlesDoNotDiscardTheCurrentPrefix) {
    Checkpoint_ previous;
    {
        RecordingScope_ old;
        old.StartRecording();
        previous = old.MakeCheckpoint();
        old.Close();
    }
    auto task = std::async(std::launch::async, [] {
        RecordingScope_ other;
        other.StartRecording();
        auto checkpoint = other.MakeCheckpoint();
        other.Close();
        return checkpoint;
    });
    const auto foreign = task.get();
    RecordingScope_ recording;
    Number_ input;
    recording.RegisterInput(input, 2.0);
    recording.StartRecording();
    Number_ prefix = input * input;
    const auto replaced = recording.MakeCheckpoint();
    const auto valid = recording.MakeCheckpoint();
    ASSERT_THROW(recording.Restore(Checkpoint_{}), Exception_);
    ASSERT_THROW(recording.Restore(previous), Exception_);
    ASSERT_THROW(recording.Restore(foreign), Exception_);
    ASSERT_THROW(recording.Restore(replaced), Exception_);
    recording.Restore(valid);
    Number_ output = 3.0 * prefix;
    recording.FinishRecording();
    ASSERT_THROW(recording.ReverseSuffix(replaced), Exception_);
    ASSERT_THROW(recording.ReversePrefix(previous), Exception_);
    Adjoint(output) = 1.0;
    recording.ReverseSuffix(valid);
    recording.ReversePrefix(valid);
    ASSERT_DOUBLE_EQ(AdjointValue(input), 12.0);
    recording.Close();
    ASSERT_THROW(recording.Restore(valid), Exception_);
    ASSERT_THROW(recording.ReverseSuffix(valid), Exception_);
    ASSERT_THROW(recording.ReversePrefix(valid), Exception_);
}

TEST(AADRecordingStateTest, TestForeignThreadMethodsRejectBeforeAccessingTheOwnerGraph) {
    RecordingScope_ recording;
    Number_ input;
    recording.RegisterInput(input, 3.0);
    recording.StartRecording();
    const auto checkpoint = recording.MakeCheckpoint();
    Number_ output = input * input;
    recording.FinishRecording();
    auto task = std::async(std::launch::async, [&] {
        RecordingScope_ other;
        Number_ foreignInput;
        ASSERT_THROW(recording.RegisterInput(foreignInput, 2.0), Exception_);
        ASSERT_THROW(recording.StartRecording(), Exception_);
        ASSERT_THROW(recording.FinishRecording(), Exception_);
        ASSERT_THROW(recording.MakeCheckpoint(), Exception_);
        ASSERT_THROW(recording.Restore(checkpoint), Exception_);
        ASSERT_THROW(recording.ClearAdjoints(), Exception_);
        ASSERT_THROW(recording.Reverse(), Exception_);
        ASSERT_THROW(recording.ReverseSuffix(checkpoint), Exception_);
        ASSERT_THROW(recording.ReversePrefix(checkpoint), Exception_);
    });
    task.get();
    recording.ClearAdjoints();
    Adjoint(output) = 1.0;
    recording.ReverseSuffix(checkpoint);
    recording.ReversePrefix(checkpoint);
    ASSERT_DOUBLE_EQ(AdjointValue(input), 6.0);
    recording.Close();
}

TEST(AADRecordingStateTest, TestFailedReverseRejectsFurtherWorkAndRequiresRecovery) {
    {
        RecordingScope_ recording;
        Number_ input;
        recording.RegisterInput(input, 2.0);
        recording.StartRecording();
        const auto checkpoint = recording.MakeCheckpoint();
        Number_ output = input * input;
        recording.FinishRecording();
        recording.ClearAdjoints();
        Adjoint(output) = 1.0;
        ASSERT_THROW(RecordingStateTestAccess_::FailReverse(recording), Exception_);
        ASSERT_TRUE(LastRecordingCleanupFailure());
        ASSERT_THROW(recording.Reverse(), Exception_);
        ASSERT_THROW(recording.Restore(checkpoint), Exception_);
        ASSERT_THROW(recording.ClearAdjoints(), Exception_);
        ASSERT_THROW(RecordingScope_ nested, Exception_);
        recording.Close();
        ASSERT_TRUE(LastRecordingCleanupFailure());
    }
    RecordingScope_ next;
    ASSERT_FALSE(LastRecordingCleanupFailure());
    Number_ input;
    next.RegisterInput(input, 3.0);
    next.StartRecording();
    Number_ output = input * input;
    next.FinishRecording();
    next.ClearAdjoints();
    Adjoint(output) = 1.0;
    next.Reverse();
    ASSERT_DOUBLE_EQ(AdjointValue(input), 6.0);
    next.Close();
}

TEST(AADRecordingStateTest, TestPrefixPayoffAliasesGetIndependentSuffixRoots) {
    RecordingScope_ recording;
    Number_ input, zero;
    recording.RegisterInput(input, 2.0);
    recording.RegisterInput(zero, 0.0);
    recording.StartRecording();
    Number_ prefix = input * input;
    const auto checkpoint = recording.MakeCheckpoint();
    for (int path = 0; path < 3; ++path) {
        recording.Restore(checkpoint);
        Number_ output = PayoffRoot(prefix, zero);
        recording.FinishRecording();
        ASSERT_DOUBLE_EQ(Value(output), 4.0);
        Adjoint(output) = 1.0;
        recording.ReverseSuffix(checkpoint);
    }
    recording.ReversePrefix(checkpoint);
    ASSERT_DOUBLE_EQ(AdjointValue(input), 12.0);
    recording.Close();
}

#if !defined(DAL_USE_XAD_AAD) && !defined(DAL_USE_CODIPACK_AAD) && !defined(DAL_USE_ADEPT_AAD)
TEST(AADRecordingStateTest, TestRawModeMutationRejectsBeforeUsingCheckpointPositionsAndRecovers) {
    {
        RecordingScope_ recording;
        Number_ input;
        recording.RegisterInput(input, 2.0);
        recording.StartRecording();
        Number_ prefix = input * input;
        const auto checkpoint = recording.MakeCheckpoint();
        Tape()->numAdj_ = 2;
        ASSERT_THROW(recording.Restore(checkpoint), Exception_);
        ASSERT_THROW(recording.FinishRecording(), Exception_);
        Tape()->numAdj_ = 1;
        recording.Restore(checkpoint);
        Number_ output = prefix * 3.0;
        recording.FinishRecording();
        recording.ClearAdjoints();
        Adjoint(output) = 1.0;
        recording.ReverseSuffix(checkpoint);
        recording.ReversePrefix(checkpoint);
        ASSERT_DOUBLE_EQ(AdjointValue(input), 12.0);
        Tape()->multi_ = true;
        ASSERT_THROW(recording.Close(), Exception_);
        ASSERT_TRUE(LastRecordingCleanupFailure());
        Tape()->multi_ = false;
    }
    RecordingScope_ next;
    ASSERT_FALSE(LastRecordingCleanupFailure());
    Number_ input;
    next.RegisterInput(input, 4.0);
    next.StartRecording();
    Number_ output = input * input;
    next.FinishRecording();
    Adjoint(output) = 1.0;
    next.Reverse();
    ASSERT_DOUBLE_EQ(AdjointValue(input), 8.0);
    next.Close();
}

TEST(AADRecordingStateTest, TestClearingAFullFinalBlockRetainsCapacityAndSupportsNewSeeds) {
    for (const bool multi : {false, true}) {
        Clear(*Tape());
        auto mode = SetNumResultsForAAD(multi, 3);
        RecordingScope_ recording;
        Number_ input;
        recording.RegisterInput(input, 1.0);
        recording.StartRecording();
        Vector_<TapNode_*> nodes;
        nodes.reserve(BLOCK_SIZE);
        nodes.push_back(&*Tape()->nodes_.Begin());
        Number_ state = input;
        for (size_t i = 1; i < BLOCK_SIZE; ++i) {
            state = state * 1.00001;
            nodes.push_back(&*std::prev(Tape()->nodes_.End()));
        }
        recording.FinishRecording();
        ASSERT_EQ(Tape()->nodes_.OccupiedSlots(), BLOCK_SIZE);
        const auto blocks = Tape()->nodes_.AllocatedBlocks();
        for (auto* node : nodes) {
            node->Adjoint() = 42.0;
            if (multi)
                for (size_t channel = 0; channel < 3; ++channel)
                    node->Adjoint(channel) = 42.0;
        }
        recording.ClearAdjoints();
        ASSERT_EQ(Tape()->nodes_.AllocatedBlocks(), blocks);
        for (auto* node : nodes) {
            ASSERT_DOUBLE_EQ(node->Adjoint(), 0.0);
            if (multi)
                for (size_t channel = 0; channel < 3; ++channel)
                    ASSERT_DOUBLE_EQ(node->Adjoint(channel), 0.0);
        }
        const double expected = std::pow(1.00001, BLOCK_SIZE - 1);
        if (multi) {
            nodes.back()->Adjoint(0) = 2.0;
            recording.Reverse();
            ASSERT_NEAR(nodes.front()->Adjoint(0), 2.0 * expected, 1.0e-10);
            ASSERT_DOUBLE_EQ(nodes.front()->Adjoint(1), 0.0);
            ASSERT_DOUBLE_EQ(nodes.front()->Adjoint(2), 0.0);
        } else {
            Adjoint(state) = 2.0;
            recording.Reverse();
            ASSERT_NEAR(AdjointValue(input), 2.0 * expected, 1.0e-10);
        }
        recording.Close();
    }
    Clear(*Tape());
}

TEST(AADRecordingStateTest, TestModeSelectionRejectsBeforeChangingAScopedGraph) {
    RecordingScope_ recording;
    Number_ input;
    recording.RegisterInput(input, 3.0);
    recording.StartRecording();
    const auto checkpoint = recording.MakeCheckpoint();
    Number_ output = input * input;
    recording.FinishRecording();
    ASSERT_THROW(SetNumResultsForAAD(true, 2), Exception_);
    ASSERT_FALSE(Tape()->multi_);
    ASSERT_EQ(Tape()->numAdj_, 1);
    recording.ClearAdjoints();
    Adjoint(output) = 1.0;
    recording.ReverseSuffix(checkpoint);
    recording.ReversePrefix(checkpoint);
    ASSERT_DOUBLE_EQ(AdjointValue(input), 6.0);
    recording.Close();
}

TEST(AADRecordingStateTest, TestFullClearingCoversVectorLeavesAndEveryChannel) {
    for (const size_t width : {1, 3, 10, 17, 33}) {
        Clear(*Tape());
        auto mode = SetNumResultsForAAD(true, width);
        RecordingScope_ recording;
        Number_ input;
        recording.RegisterInput(input, 3.0);
        recording.StartRecording();
        Number_ output = input * input;
        recording.FinishRecording();
        for (auto& node : Tape()->nodes_) {
            node.Adjoint() = 42.0;
            for (size_t channel = 0; channel < width; ++channel)
                node.Adjoint(channel) = channel + 1.0;
        }
        recording.ClearAdjoints();
        for (auto& node : Tape()->nodes_) {
            ASSERT_DOUBLE_EQ(node.Adjoint(), 0.0);
            for (size_t channel = 0; channel < width; ++channel)
                ASSERT_DOUBLE_EQ(node.Adjoint(channel), 0.0);
        }
        auto* root = &*std::prev(Tape()->nodes_.End());
        for (size_t channel = 0; channel < width; ++channel)
            root->Adjoint(channel) = channel + 1.0;
        recording.Reverse();
        auto* leaf = &*Tape()->nodes_.Begin();
        for (size_t channel = 0; channel < width; ++channel)
            ASSERT_DOUBLE_EQ(leaf->Adjoint(channel), 6.0 * (channel + 1.0));
        recording.Close();
    }
    Clear(*Tape());
}
#endif

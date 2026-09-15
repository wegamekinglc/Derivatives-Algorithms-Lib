//
// Created by wegam on 2026/5/2.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/math/aad/aad.hpp>

using Dal::AAD::Number_;
using Dal::AAD::Tape_;
using Dal::AAD::Adjoint;
using Dal::AAD::PutOnTape;

TEST(AADTapeTest, TestPropagateToMark) {
    auto* tape = Dal::AAD::Tape();
    Clear(*tape);

    Number_ x0 = 1.0;
    Number_ x1 = 2.0;
    PutOnTape(x0);
    PutOnTape(x1);
    Mark(*tape);

    Number_ y = x0 * x1;
    Adjoint(y) = 1.0;
    PropagateToMark(*tape);

    ASSERT_NEAR(Adjoint(x0), 2.0, 1e-10);
    ASSERT_NEAR(Adjoint(x1), 1.0, 1e-10);

    Clear(*tape);
}

TEST(AADTapeTest, TestMultipleMarkCycles) {
    auto* tape = Dal::AAD::Tape();
    Clear(*tape);

    Number_ x0 = 1.0;
    PutOnTape(x0);
    Mark(*tape);

    {
        Number_ y = x0 * 3.0;
        Adjoint(y) = 1.0;
        PropagateToMark(*tape);
        ASSERT_NEAR(Adjoint(x0), 3.0, 1e-10);
    }

    RewindToMark(*tape);

    {
        Number_ y = x0 * 5.0;
        Adjoint(y) = 1.0;
        PropagateToMark(*tape);
        ASSERT_NEAR(Adjoint(x0), 8.0, 1e-10);
    }

    Clear(*tape);
}

TEST(AADTapeTest, TestRepeatedClearAndRecord) {
    auto* tape = Dal::AAD::Tape();

    for (int i = 0; i < 3; ++i) {
        Clear(*tape);
        Number_ x = 1.0;
        PutOnTape(x);
        Mark(*tape);
        Number_ y = x * 2.0;
        Adjoint(y) = 1.0;
        PropagateToMark(*tape);
        ASSERT_NEAR(Adjoint(x), 2.0, 1e-10);
    }
}

TEST(AADTapeTest, TestGradientCapacityGrowsAfterSeeding) {
    auto* tape = Dal::AAD::Tape();
    Clear(*tape);

    Number_ x0 = 2.0;
    PutOnTape(x0);
    Mark(*tape);

    // The first sweep seeds with few live variables, sizing backend gradient storage.
    Number_ y0 = x0 * 3.0;
    Adjoint(y0) = 1.0;
    PropagateToMark(*tape);
    ASSERT_NEAR(Adjoint(x0), 3.0, 1e-10);

    RewindToMark(*tape);

    // A later window with many more simultaneously live variables must still
    // accumulate every adjoint instead of overrunning the frozen storage.
    const size_t n = 64;
    Number_ vars[n];
    for (size_t j = 0; j < n; ++j)
        vars[j] = x0 * static_cast<double>(j + 1);
    Number_ y1 = vars[0];
    for (size_t j = 1; j < n; ++j)
        y1 = y1 + vars[j];
    Adjoint(y1) = 1.0;
    PropagateToMark(*tape);

    ASSERT_NEAR(Adjoint(x0), 3.0 + 2080.0, 1e-10);

    Clear(*tape);
}

#if !defined(DAL_USE_XAD_AAD) && !defined(DAL_USE_CODIPACK_AAD) && !defined(DAL_USE_ADEPT_AAD)
namespace {
    void ExhaustAllocationStream(Tape_* tape, int stream) {
        if (stream == 0) {
            for (size_t i = 0; i < Dal::AAD::BLOCK_SIZE; ++i)
                tape->nodes_.EmplaceBack(0);
        } else if (stream == 1) {
            tape->ders_.EmplaceBackMulti<Dal::AAD::DATA_SIZE - 2>();
        } else if (stream == 2) {
            tape->argPtrs_.EmplaceBackMulti<Dal::AAD::DATA_SIZE - 2>();
        } else {
            tape->adjointsMulti_.EmplaceBackMulti<Dal::AAD::ADJ_SIZE - 2>();
        }
    }

    void CheckThreeInputAllocation(Tape_* tape) {
        auto* node = tape->RecordNode<3>();
        double gradients[3][3] = {};
        auto derivatives = std::prev(tape->ders_.End(), 3);
        auto pointers = std::prev(tape->argPtrs_.End(), 3);
        for (int input = 0; input != 3; ++input) {
            *derivatives = input + 2.0;
            *pointers = gradients[input];
            ++derivatives;
            ++pointers;
            ASSERT_DOUBLE_EQ(node->Adjoint(input), 0.0);
            node->Adjoint(input) = input + 1.0;
        }
        node->PropagateAll(3);
        for (int input = 0; input != 3; ++input) {
            ASSERT_DOUBLE_EQ(node->Adjoint(input), 0.0);
            for (int result = 0; result != 3; ++result)
                ASSERT_DOUBLE_EQ(gradients[input][result], (input + 2.0) * (result + 1.0));
        }
        node->Adjoint(2) = 99.0;
    }
} // namespace

TEST(AADTapeTest, TestThreeInputMultipleResultsAndAliasedOperands) {
    auto* tape = Dal::AAD::Tape();
    Clear(*tape);
    {
        auto resetter = Dal::AAD::SetNumResultsForAAD(true, 2);
        Number_ x = 2.0;
        Number_ y = 3.0;
        Number_ z = 5.0;
        auto* xNode = &*tape->nodes_.Begin();
        auto* yNode = &*std::next(tape->nodes_.Begin());
        auto* zNode = &*std::prev(tape->nodes_.End());
        Mark(*tape);
        for (int cycle = 1; cycle <= 3; ++cycle) {
            RewindToMark(*tape);
            Number_ first = x + y * z;
            auto* firstNode = &*std::prev(tape->nodes_.End());
            Number_ second = x * x + x;
            auto* secondNode = &*std::prev(tape->nodes_.End());
            ASSERT_DOUBLE_EQ(Value(first), 17.0);
            ASSERT_DOUBLE_EQ(Value(second), 6.0);
            ASSERT_EQ(tape->nodes_.Size(), 5);
            firstNode->Adjoint(0) = 1.0;
            secondNode->Adjoint(1) = 1.0;
            PropagateToMark(*tape);
            ASSERT_DOUBLE_EQ(xNode->Adjoint(0), cycle);
            ASSERT_DOUBLE_EQ(yNode->Adjoint(0), 5.0 * cycle);
            ASSERT_DOUBLE_EQ(zNode->Adjoint(0), 3.0 * cycle);
            ASSERT_DOUBLE_EQ(xNode->Adjoint(1), 5.0 * cycle);
            ASSERT_DOUBLE_EQ(yNode->Adjoint(1), 0.0);
            ASSERT_DOUBLE_EQ(zNode->Adjoint(1), 0.0);
        }
        RewindToMark(*tape);
        x += x + y;
        ASSERT_DOUBLE_EQ(Value(x), 7.0);
        ASSERT_EQ(tape->nodes_.Size(), 4);
        auto* result = &*std::prev(tape->nodes_.End());
        result->Adjoint(0) = 2.0;
        result->Adjoint(1) = 3.0;
        PropagateToMark(*tape);
        ASSERT_DOUBLE_EQ(xNode->Adjoint(0), 7.0);
        ASSERT_DOUBLE_EQ(yNode->Adjoint(0), 17.0);
        ASSERT_DOUBLE_EQ(xNode->Adjoint(1), 21.0);
        ASSERT_DOUBLE_EQ(yNode->Adjoint(1), 3.0);
    }
    Clear(*tape);
}

TEST(AADTapeTest, TestThreeInputIndependentAllocationRolloverAndReuse) {
    for (int stream = 0; stream != 4; ++stream) {
        SCOPED_TRACE(stream);
        Tape_ tape;
        tape.multi_ = true;
        tape.numAdj_ = 3;
        Mark(tape);
        for (int cycle = 0; cycle != 3; ++cycle) {
            RewindToMark(tape);
            // Exhaust one storage stream at a time without coupling the other cursors.
            ExhaustAllocationStream(&tape, stream);
            ASSERT_NO_FATAL_FAILURE(CheckThreeInputAllocation(&tape));
        }
        Rewind(tape);
        NewRecording(tape);
        auto* fresh = tape.RecordNode<3>();
        ASSERT_EQ(tape.nodes_.Size(), 1);
        for (size_t result = 0; result != 3; ++result)
            ASSERT_DOUBLE_EQ(fresh->Adjoint(result), 0.0);
    }
}

TEST(AADTapeTest, TestCallerOwnedTapeRetainsGenericArities) {
    Tape_ tape;
    tape.RecordNode<0>();
    ASSERT_EQ(tape.ders_.Size(), 0);
    ASSERT_EQ(tape.argPtrs_.Size(), 0);
    ASSERT_EQ(tape.adjointsMulti_.Size(), 0);
    tape.RecordNode<1>();
    tape.RecordNode<3>();
    auto* node = tape.RecordNode<5>();
    ASSERT_EQ(tape.nodes_.Size(), 4);
    ASSERT_EQ(tape.ders_.Size(), 9);
    ASSERT_EQ(tape.argPtrs_.Size(), 9);
    double gradients[5] = {};
    auto derivatives = std::prev(tape.ders_.End(), 5);
    auto pointers = std::prev(tape.argPtrs_.End(), 5);
    for (int input = 0; input != 5; ++input) {
        *derivatives = input + 1.0;
        *pointers = &gradients[input];
        ++derivatives;
        ++pointers;
    }
    node->Adjoint() = 2.0;
    node->PropagateOne();
    for (int input = 0; input != 5; ++input)
        ASSERT_DOUBLE_EQ(gradients[input], 2.0 * (input + 1.0));
}

TEST(AADTapeTest, TestMultiModePropagateToStartFillsAllResultSlots) {
    auto* tape = Dal::AAD::Tape();
    Clear(*tape);

    {
        auto resetter = Dal::AAD::SetNumResultsForAAD(true, 2);
        Number_ x0 = 1.0;
        Number_ x1 = 2.0;
        PutOnTape(x0);
        PutOnTape(x1);

        Number_ y0 = x0 * x1;
        Number_ y1 = x0 + x1;

        auto y1Node = std::prev(tape->nodes_.End());
        auto y0Node = std::prev(y1Node);
        auto x1Node = std::prev(y0Node);
        auto x0Node = std::prev(x1Node);

        y0Node->Adjoint(0) = 1.0;
        y1Node->Adjoint(1) = 1.0;
        PropagateToStart(*tape);

        ASSERT_NEAR(x0Node->Adjoint(0), 2.0, 1e-10);
        ASSERT_NEAR(x1Node->Adjoint(0), 1.0, 1e-10);
        ASSERT_NEAR(x0Node->Adjoint(1), 1.0, 1e-10);
        ASSERT_NEAR(x1Node->Adjoint(1), 1.0, 1e-10);
    }

    Clear(*tape);
}

TEST(AADTapeTest, TestMultiModeRepeatedSweepDoesNotAccumulateStaleAdjoints) {
    auto* tape = Dal::AAD::Tape();
    Clear(*tape);

    {
        auto resetter = Dal::AAD::SetNumResultsForAAD(true, 2);
        Number_ x0 = 1.0;
        Number_ x1 = 2.0;
        PutOnTape(x0);
        PutOnTape(x1);

        Number_ y = x0 * x1;

        auto yNode = std::prev(tape->nodes_.End());
        auto x1Node = std::prev(yNode);
        auto x0Node = std::prev(x1Node);

        yNode->Adjoint(0) = 1.0;
        yNode->Adjoint(1) = 1.0;
        PropagateToStart(*tape);

        ASSERT_NEAR(x0Node->Adjoint(0), 2.0, 1e-10);
        ASSERT_NEAR(x0Node->Adjoint(1), 2.0, 1e-10);
        ASSERT_NEAR(x1Node->Adjoint(0), 1.0, 1e-10);
        ASSERT_NEAR(x1Node->Adjoint(1), 1.0, 1e-10);

        PropagateToStart(*tape);

        ASSERT_NEAR(x0Node->Adjoint(0), 2.0, 1e-10);
        ASSERT_NEAR(x0Node->Adjoint(1), 2.0, 1e-10);
        ASSERT_NEAR(x1Node->Adjoint(0), 1.0, 1e-10);
        ASSERT_NEAR(x1Node->Adjoint(1), 1.0, 1e-10);
    }

    Clear(*tape);
}

TEST(AADTapeTest, TestNestedSetNumResultsForAADRestoresOuterScope) {
    auto* tape = Dal::AAD::Tape();
    Clear(*tape);

    ASSERT_FALSE(tape->multi_);
    ASSERT_EQ(tape->numAdj_, size_t(1));
    {
        auto outer = Dal::AAD::SetNumResultsForAAD(true, 3);
        ASSERT_TRUE(tape->multi_);
        ASSERT_EQ(tape->numAdj_, size_t(3));
        {
            auto inner = Dal::AAD::SetNumResultsForAAD(true, 5);
            ASSERT_TRUE(tape->multi_);
            ASSERT_EQ(tape->numAdj_, size_t(5));
        }
        ASSERT_TRUE(tape->multi_);
        ASSERT_EQ(tape->numAdj_, size_t(3));
    }
    ASSERT_FALSE(tape->multi_);
    ASSERT_EQ(tape->numAdj_, size_t(1));

    Clear(*tape);
}

TEST(AADTapeTest, TestClearEmptiesAdjointsMultiAfterMultiToNonMultiToggle) {
    auto* tape = Dal::AAD::Tape();
    Clear(*tape);

    {
        auto resetter = Dal::AAD::SetNumResultsForAAD(true, 2);
        Number_ x0 = 1.0;
        Number_ x1 = 2.0;
        PutOnTape(x0);
        PutOnTape(x1);
        ASSERT_GT(tape->adjointsMulti_.Size(), 0);
    }

    ASSERT_FALSE(tape->multi_);
    Clear(*tape);
    ASSERT_EQ(tape->adjointsMulti_.Size(), 0);

    Clear(*tape);
}

TEST(AADTapeTest, TestSetNumResultsForAADRejectsOutOfRange) {
    ASSERT_THROW(Dal::AAD::SetNumResultsForAAD(true, 0), Dal::Exception_);
    ASSERT_THROW(Dal::AAD::SetNumResultsForAAD(true, Dal::AAD::ADJ_SIZE + 1), Dal::Exception_);
}
#endif

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

#if !defined(DAL_USE_XAD_AAD) && !defined(DAL_USE_CODIPACK_AAD) && !defined(DAL_USE_ADEPT_AAD)
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
#endif

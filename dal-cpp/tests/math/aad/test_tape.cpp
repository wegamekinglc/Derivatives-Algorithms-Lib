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

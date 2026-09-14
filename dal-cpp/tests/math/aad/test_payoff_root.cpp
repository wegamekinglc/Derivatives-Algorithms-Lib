//
// Created by Codex on 2026/9/14.
//

#include <gtest/gtest.h>

#include <dal/curve/tapeguard.hpp>
#include <dal/math/aad/aad.hpp>

using namespace Dal;
using namespace Dal::AAD;

#if !defined(DAL_USE_XAD_AAD) && !defined(DAL_USE_CODIPACK_AAD) && !defined(DAL_USE_ADEPT_AAD)
TEST(AADTest, TestPayoffRootReusesTerminalPathNode) {
    Activate(*Tape());
    TapeGuard_ guard(Tape());
    Number_ scale = 2.0;
    Number_ zero = 0.0;
    PutOnTape(scale);
    PutOnTape(zero);
    NewRecording(*Tape());
    const Number_ seed = scale * 80.0;
    Mark(*Tape());
    for (size_t path = 0; path < 257; ++path) {
        RewindToMark(*Tape());
        const Number_ payoff = seed * 3.0;
        const auto end = Tape()->nodes_.End();
        Number_ root = PayoffRoot(payoff, zero);
        ASSERT_TRUE(Tape()->nodes_.End() == end);
        ASSERT_DOUBLE_EQ(Value(root), 480.0);
        Adjoint(root) = 1.0;
        PropagateToMark(*Tape());
    }
    PropagateMarkToStart(*Tape());
    ASSERT_NEAR(AdjointValue(scale) / 257, 240.0, 1.0e-10);
}
#endif

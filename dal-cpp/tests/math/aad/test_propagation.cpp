//
// Created by Codex on 2026/10/04.
//

#include <array>
#include <cmath>
#include <dal/math/aad/aad.hpp>
#include <dal/platform/platform.hpp>
#include <gtest/gtest.h>
#include <limits>
#include <utility>

using Dal::AAD::Number_;

namespace {
    std::pair<double, double> ScaledIdentity(double scale, double seed, bool materialize) {
        using namespace Dal::AAD;
        Clear(*Tape());
        Number_ x = 3.0;
        PutOnTape(x);
        NewRecording(*Tape());
        Number_ y;
        if (materialize) {
            Number_ intermediate = x * scale;
            y = intermediate / scale;
        } else {
            y = (x * scale) / scale;
        }
        Adjoint(y) = seed;
        PropagateToStart(*Tape());
        const auto result = std::make_pair(Value(y), Dal::AAD::AdjointValue(x));
        Clear(*Tape());
        return result;
    }
} // namespace

TEST(AADPropagationTest, TestScaledIdentityWithMaterializedAndFusedExpressions) {
    for (double scale : {1.0, -1.0, 1e10, -1e10, 1e16, -1e16, 1e-16, -1e-16}) {
        for (double seed : {1.0, -1.0, 1e-16, -1e-16, 0.0}) {
            for (bool materialize : {true, false}) {
                SCOPED_TRACE(::testing::Message() << "scale=" << scale << " seed=" << seed << " materialize=" << materialize);
                const auto result = ScaledIdentity(scale, seed, materialize);
                ASSERT_NEAR(result.first, 3.0, 1e-10);
                if (seed == 0.0)
                    ASSERT_DOUBLE_EQ(result.second, 0.0);
                else
                    ASSERT_NEAR(result.second / seed, 1.0, 1e-10);
            }
        }
    }
}

TEST(AADPropagationTest, TestSmallCheckpointAdjointsAccumulateBeforePrefixPropagation) {
    using namespace Dal::AAD;
    Clear(*Tape());
    Number_ x = 3.0;
    PutOnTape(x);
    NewRecording(*Tape());
    Number_ prefix = x * 1e16;
    Mark(*Tape());
    for (double seed : {1.0, -0.5, 2.0}) {
        RewindToMark(*Tape());
        Number_ y = prefix / 1e16;
        Adjoint(y) = seed;
        PropagateToMark(*Tape());
    }
    PropagateMarkToStart(*Tape());
    ASSERT_NEAR(AdjointValue(x), 2.5, 1e-10);
    Clear(*Tape());
}

TEST(AADPropagationTest, TestNonFiniteSeedsRemainObservable) {
    using namespace Dal::AAD;
    for (double seed :
         {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()}) {
        Clear(*Tape());
        Number_ x = 3.0;
        PutOnTape(x);
        NewRecording(*Tape());
        Number_ y = x * 2.0;
        Adjoint(y) = seed;
        PropagateToStart(*Tape());
        if (std::isnan(seed))
            ASSERT_TRUE(std::isnan(AdjointValue(x)));
        else
            ASSERT_EQ(AdjointValue(x), seed);
        Clear(*Tape());
    }
}

#if !defined(DAL_USE_XAD_AAD) && !defined(DAL_USE_CODIPACK_AAD) && !defined(DAL_USE_ADEPT_AAD)
TEST(AADPropagationTest, TestVectorAdjointsDoNotDependOnOtherRequestedOutputs) {
    using namespace Dal::AAD;
    for (double secondSeed : {0.0, 1.0, 1e16}) {
        Clear(*Tape());
        {
            auto mode = SetNumResultsForAAD(true, 2);
            Number_ x = 3.0;
            auto* inputNode = &*std::prev(Tape()->nodes_.End());
            Number_ intermediate = x * 1e16;
            Number_ y = intermediate / 1e16;
            auto* outputNode = &*std::prev(Tape()->nodes_.End());
            outputNode->Adjoint(0) = 1.0;
            outputNode->Adjoint(1) = secondSeed;
            PropagateToStart(*Tape());
            ASSERT_NEAR(inputNode->Adjoint(0), 1.0, 1e-10);
            if (secondSeed == 0.0)
                ASSERT_DOUBLE_EQ(inputNode->Adjoint(1), 0.0);
            else
                ASSERT_NEAR(inputNode->Adjoint(1) / secondSeed, 1.0, 1e-10);
            ASSERT_DOUBLE_EQ(outputNode->Adjoint(0), 0.0);
            ASSERT_DOUBLE_EQ(outputNode->Adjoint(1), 0.0);
        }
        Clear(*Tape());
    }
}

TEST(AADPropagationTest, TestZeroVectorSeedDoesNotMultiplyInfiniteDerivative) {
    using namespace Dal::AAD;
    Clear(*Tape());
    {
        auto mode = SetNumResultsForAAD(true, 2);
        Number_ x = 0.0;
        auto* inputNode = &*std::prev(Tape()->nodes_.End());
        Number_ y = sqrt(x);
        auto* outputNode = &*std::prev(Tape()->nodes_.End());
        ASSERT_DOUBLE_EQ(Value(y), 0.0);
        outputNode->Adjoint(0) = 1.0;
        outputNode->Adjoint(1) = 0.0;
        PropagateToStart(*Tape());
        ASSERT_EQ(inputNode->Adjoint(0), std::numeric_limits<double>::infinity());
        ASSERT_DOUBLE_EQ(inputNode->Adjoint(1), 0.0);
    }
    Clear(*Tape());
}

TEST(AADPropagationTest, TestVectorNaNSeedIsNotDiscarded) {
    using namespace Dal::AAD;
    Clear(*Tape());
    {
        auto mode = SetNumResultsForAAD(true, 2);
        Number_ x = 3.0;
        auto* inputNode = &*std::prev(Tape()->nodes_.End());
        Number_ y = x * 2.0;
        auto* outputNode = &*std::prev(Tape()->nodes_.End());
        outputNode->Adjoint(0) = std::numeric_limits<double>::quiet_NaN();
        outputNode->Adjoint(1) = 0.0;
        PropagateToStart(*Tape());
        ASSERT_TRUE(std::isnan(inputNode->Adjoint(0)));
        ASSERT_DOUBLE_EQ(inputNode->Adjoint(1), 0.0);
    }
    Clear(*Tape());
}
#endif

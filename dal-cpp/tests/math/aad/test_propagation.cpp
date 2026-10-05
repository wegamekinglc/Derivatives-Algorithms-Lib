//
// Created by Codex on 2026/10/04.
//

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <utility>

#include <dal/math/aad/aad.hpp>
#include <dal/platform/platform.hpp>

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

TEST(AADPropagationTest, TestSubnormalAndSignedZeroSeeds) {
    const double subnormal = std::numeric_limits<double>::denorm_min();
    for (double seed : {subnormal, -subnormal, 0.0, -0.0}) {
        const auto result = ScaledIdentity(1.0, seed, true);
        ASSERT_EQ(result.second, seed);
    }
}

TEST(AADPropagationTest, TestVectorWidthsConsumeIntermediatesAndAccumulateAliasedInputs) {
    using namespace Dal::AAD;
    for (size_t width : {1U, 2U, 3U, 4U, 5U, 7U, 8U, 10U, 15U, 16U, 17U, 33U}) {
        SCOPED_TRACE(::testing::Message() << "width=" << width);
        Clear(*Tape());
        {
            auto mode = SetNumResultsForAAD(true, width);
            Number_ x = 2.0;
            auto* inputNode = &*std::prev(Tape()->nodes_.End());
            Number_ y = 3.0 * x * x + 5.0;
            auto* outputNode = &*std::prev(Tape()->nodes_.End());
            for (int sweep = 1; sweep <= 2; ++sweep) {
                for (size_t j = 0; j < width; ++j)
                    outputNode->Adjoint(j) = j % 4 == 0 ? 0.0 : static_cast<double>(j + 1);
                PropagateToStart(*Tape());
                for (size_t j = 0; j < width; ++j) {
                    const double seed = j % 4 == 0 ? 0.0 : static_cast<double>(j + 1);
                    ASSERT_DOUBLE_EQ(inputNode->Adjoint(j), 12.0 * seed * sweep);
                    ASSERT_DOUBLE_EQ(outputNode->Adjoint(j), 0.0);
                }
            }
        }
        Clear(*Tape());
    }
}

TEST(AADPropagationTest, TestVectorSubnormalSeedsRemainObservable) {
    using namespace Dal::AAD;
    Clear(*Tape());
    {
        auto mode = SetNumResultsForAAD(true, 2);
        Number_ x = 3.0;
        auto* inputNode = &*std::prev(Tape()->nodes_.End());
        Number_ y = x * 1.0;
        auto* outputNode = &*std::prev(Tape()->nodes_.End());
        const double subnormal = std::numeric_limits<double>::denorm_min();
        outputNode->Adjoint(0) = subnormal;
        outputNode->Adjoint(1) = -subnormal;
        PropagateToStart(*Tape());
        ASSERT_EQ(inputNode->Adjoint(0), subnormal);
        ASSERT_EQ(inputNode->Adjoint(1), -subnormal);
    }
    Clear(*Tape());
}

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

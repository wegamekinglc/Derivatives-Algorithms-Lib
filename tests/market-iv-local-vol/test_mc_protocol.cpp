#include <gtest/gtest.h>

#include <limits>

#include <dal/math/random/pseudorandom.hpp>
#include <dal/platform/platform.hpp>

#include "mcprotocol.hpp"

TEST(MarketIvQualificationTest, TestFixedSlotsRejectOverflowAndSeparateBatches) {
    ASSERT_EQ(DalTest::ScalarOffset(1, 0, 9), (size_t{1} << 20) * 9);
    ASSERT_LT(DalTest::ScalarOffset(0, (size_t{1} << 20) - 1, 9) + 8, DalTest::ScalarOffset(1, 0, 9));
    ASSERT_THROW(DalTest::ScalarOffset(16, 0, 9), Dal::Exception_);
    ASSERT_THROW(DalTest::ScalarOffset(0, size_t{1} << 20, 9), Dal::Exception_);
    ASSERT_THROW(DalTest::ScalarOffset(15, 0, std::numeric_limits<size_t>::max()), Dal::Exception_);
}

TEST(MarketIvQualificationTest, TestFineBrownianAggregationPreservesNestedAnchors) {
    const Dal::Vector_<> anchors{0., 1. / 36500., 91. / 365., 273. / 365., 1., 2., 2.25};
    const auto coarse = DalTest::NestedTimeline(anchors, 0), fine = DalTest::NestedTimeline(anchors, 1);
    ASSERT_EQ(fine.size() - 1, 2 * (coarse.size() - 1));
    const auto aggregate = DalTest::Aggregate(fine, coarse, Dal::Vector_<>(fine.size() - 1, 1.));
    for (size_t i = 0; i < aggregate.size(); ++i) {
        ASSERT_DOUBLE_EQ(fine[2 * i], coarse[i]);
        ASSERT_NEAR(aggregate[i], std::sqrt(2.), 1e-10);
    }
}

TEST(MarketIvQualificationTest, TestBatchMeanStandardErrorUsesFifteenDegreesOfFreedom) {
    std::array<double, 16> means{};
    for (size_t i = 0; i < means.size(); ++i)
        means[i] = static_cast<double>(i);
    const auto estimate = DalTest::Estimate(means);
    ASSERT_DOUBLE_EQ(estimate.first, 7.5);
    ASSERT_NEAR(estimate.second, std::sqrt(340. / 240.), 1e-14);
}

TEST(MarketIvQualificationTest, TestScalarSeekingMatchesEveryReservedBatch) {
    for (size_t batch = 0; batch < DalTest::BATCHES; ++batch) {
        auto paths = Dal::New(Dal::RNGType_("MRG32"), 1024, 7, true);
        auto scalar = Dal::New(Dal::RNGType_("MRG32"), 1024, 1, true);
        paths->SkipNormalTo(batch * DalTest::MAX_PATHS + 123);
        scalar->SkipNormalTo(DalTest::ScalarOffset(batch, 123, 7));
        Dal::Vector_<> left(7), right(7);
        paths->FillNormal(&left);
        scalar->FillNormal(&right);
        for (size_t i = 0; i < left.size(); ++i)
            ASSERT_DOUBLE_EQ(left[i], right[i]);
    }
}

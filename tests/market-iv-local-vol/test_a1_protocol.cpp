#include <gtest/gtest.h>

#include <fstream>

#include <rapidjson/istreamwrapper.h>

#include <dal/math/random/pseudorandom.hpp>

#include "a1protocol.hpp"
#include "capturecommon.hpp"

TEST(MarketIvA1Test, TestBatchMeanStandardErrorUsesFifteenDegreesOfFreedom) {
    std::array<double, 16> means{};
    for (size_t i = 0; i < means.size(); ++i)
        means[i] = static_cast<double>(i);
    const auto estimate = DalTest::Estimate(means);
    ASSERT_DOUBLE_EQ(estimate.first, 7.5);
    ASSERT_NEAR(estimate.second, std::sqrt(340. / 240.), 1e-14);
}

TEST(MarketIvA1Test, TestCanonicalRequestPreservesDimensionsAndNestedBrownianFields) {
    std::ifstream input(DAL_A1_REQUEST);
    rapidjson::IStreamWrapper stream(input);
    rapidjson::Document request;
    request.ParseStream<rapidjson::kParseFullPrecisionFlag>(stream);
    ASSERT_FALSE(request.HasParseError());
    const auto anchors = DalTest::Axis(request["protocol"]["anchors"]);
    const auto fine = DalTest::A1Timeline(anchors, 3);
    ASSERT_EQ(fine.size() - 1, size_t{8736});
    Dal::Vector_<> normals(fine.size() - 1);
    for (size_t j = 0; j < normals.size(); ++j)
        normals[j] = std::sqrt(fine[j + 1] - fine[j]);
    for (size_t level = 0; level < 4; ++level) {
        const auto times = DalTest::A1Timeline(anchors, level);
        ASSERT_EQ(times, DalTest::Axis(request["protocol"]["timelines"][level]));
        ASSERT_EQ(times.size() - 1, size_t{1092} * (size_t{1} << level));
        for (size_t j = 1; j < times.size(); ++j)
            ASSERT_LE(times[j] - times[j - 1], 1. / (384 * (size_t{1} << level)));
        for (const double anchor : anchors)
            ASSERT_TRUE(std::binary_search(times.begin(), times.end(), anchor));
        const auto coarse = DalTest::Aggregate(fine, times, normals);
        for (size_t j = 0; j < coarse.size(); ++j)
            ASSERT_NEAR(coarse[j], std::sqrt(times[j + 1] - times[j]), 1e-12);
    }
}

TEST(MarketIvA1Test, TestUniqueSegmentOverrideRejectsWrongIndexAndEndpoint) {
    ASSERT_EQ(DalTest::A1CoarseCount(60, 0x1.0006f5248e2e8p-4, 0x1.2006f5248e2e8p-4), size_t{4});
    ASSERT_THROW(DalTest::A1CoarseCount(59, 0x1.0006f5248e2e8p-4, 0x1.2006f5248e2e8p-4), Dal::Exception_);
    ASSERT_THROW(DalTest::A1CoarseCount(60, 0x1.0006f5248e2e8p-4, std::nextafter(0x1.2006f5248e2e8p-4, 1.)), Dal::Exception_);
    ASSERT_THROW(DalTest::A1Address(0, 0, 0, 0, 8728), Dal::Exception_);
}

TEST(MarketIvA1Test, TestFormalPrefixesExtendTinyAndNeverRegenerateItsFirstPath) {
    ASSERT_EQ(DalTest::A1PreviousPrefix(16), size_t{1});
    ASSERT_EQ(DalTest::A1PreviousPrefix(17), size_t{1} << 16);
    ASSERT_EQ(DalTest::A1PreviousPrefix(20), size_t{1} << 19);
    ASSERT_THROW(DalTest::A1PreviousPrefix(15), Dal::Exception_);
}

TEST(MarketIvA1Test, TestEveryFamilyBatchScalarSeekingAndCheckedUpperBound) {
    ASSERT_EQ(DalTest::A1Address(1, 15, (size_t{1} << 20) - 1, 8735), size_t{293131517951});
    ASSERT_THROW(DalTest::A1Address(2, 0, 0), Dal::Exception_);
    ASSERT_THROW(DalTest::A1Address(1, 15, 0, 0, std::numeric_limits<size_t>::max()), Dal::Exception_);
    for (size_t f = 0; f < 2; ++f)
        for (size_t b = 0; b < 16; ++b) {
            auto vector = Dal::New(Dal::RNGType_("MRG32"), 1024, 8736, true);
            auto scalar = Dal::New(Dal::RNGType_("MRG32"), 1024, 1, true);
            vector->SkipNormalTo((f * 16 + b) * DalTest::MAX_PATHS + 123);
            scalar->SkipNormalTo(DalTest::A1Address(f, b, 123));
            Dal::Vector_<> left(8736), right(8736);
            vector->FillNormal(&left);
            scalar->FillNormal(&right);
            ASSERT_EQ(left, right);
        }
}

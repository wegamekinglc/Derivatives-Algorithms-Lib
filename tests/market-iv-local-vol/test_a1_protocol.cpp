#include <gtest/gtest.h>

#include <fstream>

#include <rapidjson/istreamwrapper.h>

#include <dal/math/random/pseudorandom.hpp>

#include "a1protocol.hpp"
#include "capturecommon.hpp"

TEST(MarketIvA1Test, TestCanonicalRequestPreservesDimensionsAndNestedBrownianFields) {
    std::ifstream input(DAL_A1_REQUEST);
    rapidjson::IStreamWrapper stream(input);
    rapidjson::Document request;
    request.ParseStream<rapidjson::kParseFullPrecisionFlag>(stream);
    ASSERT_FALSE(request.HasParseError());
    const auto anchors = DalTest::Axis(request["protocol"]["anchors"]);
    const auto fine = DalTest::A1Timeline(anchors, 3);
    Dal::Vector_<> normals(fine.size() - 1);
    for (size_t j = 0; j < normals.size(); ++j)
        normals[j] = std::sqrt(fine[j + 1] - fine[j]);
    for (size_t level = 0; level < 4; ++level) {
        const auto times = DalTest::A1Timeline(anchors, level);
        ASSERT_EQ(times, DalTest::Axis(request["protocol"]["timelines"][level]));
        const auto coarse = DalTest::Aggregate(fine, times, normals);
        for (size_t j = 0; j < coarse.size(); ++j)
            ASSERT_NEAR(coarse[j], std::sqrt(times[j + 1] - times[j]), 1e-12);
    }
}

TEST(MarketIvA1Test, TestEveryFamilyBatchScalarSeekingAndCheckedUpperBound) {
    ASSERT_EQ(DalTest::A1Address(1, 15, (size_t{1} << 20) - 1, 8727), size_t{292863082495});
    ASSERT_THROW(DalTest::A1Address(2, 0, 0), Dal::Exception_);
    ASSERT_THROW(DalTest::A1Address(1, 15, 0, 0, std::numeric_limits<size_t>::max()), Dal::Exception_);
    for (size_t f = 0; f < 2; ++f)
        for (size_t b = 0; b < 16; ++b) {
            auto vector = Dal::New(Dal::RNGType_("MRG32"), 1024, 8728, true);
            auto scalar = Dal::New(Dal::RNGType_("MRG32"), 1024, 1, true);
            vector->SkipNormalTo((f * 16 + b) * DalTest::MAX_PATHS + 123);
            scalar->SkipNormalTo(DalTest::A1Address(f, b, 123));
            Dal::Vector_<> left(8728), right(8728);
            vector->FillNormal(&left);
            scalar->FillNormal(&right);
            ASSERT_EQ(left, right);
        }
}

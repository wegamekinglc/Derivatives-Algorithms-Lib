#include <gtest/gtest.h>

#include <climits>
#include <dal/math/random/pseudorandom.hpp>
#include <dal/math/specialfunctions.hpp>
#include <dal/script/xccymcbasket_internal.hpp>

using namespace Dal;

TEST(XccyMcBasketRangeTest, TestScalarAdapterReusesPathStorage) {
    XccyMcBasketInternal::ScalarNormals_ adapter(1024, 5, {0, 2, 0, 10});
    const auto& first = adapter.Next();
    const auto* storage = first.data();
    const auto firstValues = first;
    const auto& second = adapter.Next();
    ASSERT_EQ(second.data(), storage);
    ASSERT_NE(second, firstValues);
    ASSERT_EQ(adapter.Cursor(), 10U);
    ASSERT_NO_THROW(adapter.Finish());
}

TEST(XccyMcBasketRangeTest, TestScalarAdapterSeeksNonAlignedAbsoluteCoordinate) {
    const size_t first = 70778880;
    ASSERT_EQ(first % 55, 40U);
    XccyMcBasketInternal::ScalarNormals_ adapter(1024, 55, {0, 3, first, first + 165});
    auto reference = New(RNGType_("MRG32"), 1024, 1, true);
    reference->SkipNormalTo(first);
    for (size_t p = 0; p < 3; ++p) {
        Vector_<> expected(55);
        reference->FillNormal(&expected);
        ASSERT_EQ(adapter.Next(), expected);
        ASSERT_EQ(adapter.Cursor(), first + (p + 1) * 55);
    }
    ASSERT_NO_THROW(adapter.Finish());
    ASSERT_THROW(static_cast<void>(adapter.Next()), CalibrationError_);
}

TEST(XccyMcBasketRangeTest, TestSerialUniformOracleAndMissingNormalCursor) {
    for (const size_t first : {0U, 1U, 13U, 57U}) {
        XccyMcBasketInternal::ScalarNormals_ adapter(1024, 5, {0, 2, first, first + 10});
        auto reference = New(RNGType_("MRG32"), 1024, 1, true);
        for (size_t i = 0; i < first; ++i)
            static_cast<void>(reference->NextUniform());
        const auto firstVector = adapter.Next();
        for (const double value : firstVector)
            ASSERT_EQ(value, InverseNCDF(reference->NextUniform(), true, true));
        ASSERT_THROW(adapter.Finish(), CalibrationError_);
        static_cast<void>(adapter.Next());
        ASSERT_NO_THROW(adapter.Finish());
    }
    XccyMcBasketInternal::ScalarNormals_ deterministic(1024, 0, {7, 9, 0, 0});
    ASSERT_TRUE(deterministic.Next().empty());
    ASSERT_TRUE(deterministic.Next().empty());
    ASSERT_NO_THROW(deterministic.Finish());
}

TEST(XccyMcBasketRangeTest, TestCheckedArithmeticAndAllDeterministicStorageBounds) {
    using namespace XccyMcBasketInternal;
    const size_t limit = std::numeric_limits<size_t>::max();
    ASSERT_EQ(Add(limit - 1, 1), limit);
    ASSERT_THROW(static_cast<void>(Add(limit, 1)), CalibrationError_);
    ASSERT_THROW(static_cast<void>(Multiply(limit, 2)), CalibrationError_);
    ASSERT_EQ(Multiply(2359296, 1820, UINT32_MAX), 4293918720U);
    ASSERT_THROW(static_cast<void>(Multiply(2359296, 1825, UINT32_MAX)), CalibrationError_);
    ASSERT_THROW(static_cast<void>(Add(4293918720U, 2359296U * 5, UINT32_MAX)), CalibrationError_);
    XccyMcSettings_ settings;
    settings.batches_ = settings.pathsPerBatch_ = 2;
    ASSERT_THROW(Capacity(static_cast<size_t>(INT_MAX), 0, settings), CalibrationError_);
    ASSERT_THROW(Capacity(1, static_cast<size_t>(INT_MAX), settings), CalibrationError_);
    ASSERT_THROW(Storage(limit, sizeof(double)), CalibrationError_);
}

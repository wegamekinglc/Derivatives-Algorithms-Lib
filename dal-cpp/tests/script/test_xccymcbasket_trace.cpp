#include <gtest/gtest.h>

#include <dal/math/specialfunctions.hpp>

#include "xccymcbasketfixture.hpp"
#include "xccymcbasketrngoracle.hpp"

using namespace Dal;
using namespace Dal::TestXccyBasket;
using namespace Dal::XccyMcBasketInternal;

TEST(XccyMcBasketTraceTest, TestActualAdapterAtAllCanonicalInitialAndRefinedBatchStarts) {
    const Vector_<size_t> dimensions{30, 55, 80}, offsets{0, 70778880, 200540160};
    const Vector_<size_t> phaseStarts{0, 262144, 1310720}, initialCounts{4096, 16384, 16384};
    size_t checked = 0;
    for (size_t i = 0; i < 3; ++i)
        for (size_t phase = 0; phase < 3; ++phase)
            for (size_t refinement : {1U, 2U, 4U})
                for (size_t b = 0; b < 16; ++b) {
                    const size_t path = phaseStarts[phase] + b * initialCounts[phase] * refinement;
                    const size_t coordinate = offsets[i] + path * dimensions[i];
                    ScalarNormals_ adapter(1024, dimensions[i], {path, path + 1, coordinate, coordinate + dimensions[i]});
                    ScalarOracle_ oracle(1024, coordinate);
                    const auto values = adapter.Next();
                    for (const double value : values) {
                        ASSERT_EQ(value, InverseNCDF(oracle.NextUniform(), true, true));
                        ++checked;
                    }
                    ASSERT_NO_THROW(adapter.Finish());
                }
    ASSERT_EQ(checked, 23760U);
}

TEST(XccyMcBasketTraceTest, TestSourceObservationUsesJoinDatedSlotsAndScalarCoordinates) {
    const auto spec = ModelSpec();
    const auto model = Data(spec);
    const auto members = Members();
    const auto context = Context(spec, Empty());
    const PreparedXccyMcBasket_ plan(members, context, model, Settings(2, 2));
    for (const auto& member : plan.Allocation().members_) {
        const auto& requests = member.observations_.Requests();
        ASSERT_EQ(requests.size(), member.requests_.size());
        for (const auto& request : requests) {
            ASSERT_FALSE(request.uses_.empty());
            ASSERT_TRUE(request.modelSlot_);
            ASSERT_EQ(member.grid_[request.modelSlot_->sampleId_], request.key_.fixingTime_.Date());
            for (const auto& use : request.uses_) {
                ASSERT_FALSE(use.indexOriginal_.empty());
                ASSERT_EQ(use.fixingDate_.has_value(), !use.legacySpot_);
                ASSERT_EQ(use.fixingDate_.value_or(request.key_.fixingTime_.Date()), request.key_.fixingTime_.Date());
            }
            const size_t sample = request.modelSlot_->sampleId_;
            if (sample == 0)
                continue;
            const auto& range = member.ranges_[0];
            for (size_t slot = 0; slot < 5; ++slot) {
                const size_t c = member.firstCoordinate_ + range.firstPath_ * member.mapping_.dimension_ + 5 * (sample - 1) + slot;
                ASSERT_GE(c, range.firstCoordinate_);
                ASSERT_LT(c, range.coordinateEnd_);
                ASSERT_EQ((c - member.firstCoordinate_) % member.mapping_.dimension_, 5 * (sample - 1) + slot);
            }
        }
    }
}

TEST(XccyMcBasketTraceTest, TestAllNineFullReservationsAndX4CapacityAreFixed) {
    const auto spec = ModelSpec();
    const auto model = Data(spec);
    auto settings = Settings(16, 65536);
    const PreparedXccyMcBasket_ plan(Members(), Context(spec, Empty()), model, settings);
    const Vector_<Vector_<size_t>> expected{
        {0, 7864320, 39321600, 70778880}, {70778880, 85196800, 142868480, 200540160}, {200540160, 221511680, 305397760, 389283840}};
    for (size_t i = 0; i < 3; ++i)
        for (size_t phase = 0; phase < 3; ++phase) {
            ASSERT_EQ(plan.Allocation().members_[i].phaseRanges_[phase].firstCoordinate_, expected[i][phase]);
            ASSERT_EQ(plan.Allocation().members_[i].phaseRanges_[phase].coordinateEnd_, expected[i][phase + 1]);
        }
    ASSERT_EQ(plan.Allocation().members_[1].firstCoordinate_ % 55, 40U);
    ASSERT_EQ(plan.Allocation().members_[2].ranges_.back().coordinateEnd_, 305397760U);
}

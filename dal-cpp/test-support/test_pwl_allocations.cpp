#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/curve/ycpwlf.hpp>
#include "bcg_allocation_probe.hpp"

using namespace Dal;

TEST(DiscountPWLFTest, TestQueriesDoNotAllocateAfterConstructionOrUpdate) {
    const Vector_<Date_> knots = {Date_(2025, 1, 1), Date_(2026, 1, 1), Date_(2028, 1, 1)};
    Tape::DiscountPWLF_<double> curve("pwl", "USD", knots, {0.031, 0.043, 0.027}, {0.034, 0.039, 0.028});
    const Vector_<Date_> dates = {Date_(2024, 6, 1), knots.front(), Date_(2025, 8, 1), knots[1], Date_(2027, 4, 1), knots.back(), Date_(2029, 1, 1)};
    const Vector_<> dx(curve.NX(), 0.0001);
    for (int state = 0; state < 2; ++state) {
        if (state != 0)
            curve.ApplyDX(dx.begin(), 1.0);
        BcgAllocationProbePrivate_::Reset_();
        BcgAllocationProbePrivate_::Measurement_ measurement;
        double sum = 0.0;
        for (const auto& from : dates)
            for (const auto& to : dates)
                sum += curve(from, to);
        const auto snapshot = measurement.Finish_();
        ASSERT_GT(sum, 0.0);
        ASSERT_TRUE(snapshot.balanced_);
        ASSERT_FALSE(snapshot.failedClosed_);
        ASSERT_EQ(snapshot.allocationRequests_, 0u) << "curve state " << state;
    }
}

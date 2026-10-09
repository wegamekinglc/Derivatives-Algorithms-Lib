//
// Created by Codex on 2026/10/09.
//

#include <gtest/gtest.h>

#include <dal/math/aad/bumpoveraad.hpp>
#include <dal/math/aad/native.hpp>

#include "bcg_allocation_probe.hpp"

using namespace Dal;
using namespace Dal::AAD;
using namespace Dal::BcgAllocationProbePrivate_;

TEST(AADBumpOverAADAllocationTest, TestEveryRequestAllocationFailureRestoresModeAndRecovers) {
    BumpOverAADRequest_ request;
    request.directions_ = Matrix_<>(1, 1, 1.0);
    request.steps_ = {0.1};
    const Vector_<> point{2.0};
    const NativeScalarFunction_ function = [](RecordingScope_*, const Vector_<Number_>& x) -> Number_ { return x[0] * x[0]; };
    const auto callerMode = SetNumResultsForAAD(true, 4);
    (void)EvaluateBumpOverAAD(function, point, request);
    Reset_();
    Measurement_ measurement;
    (void)EvaluateBumpOverAAD(function, point, request);
    const auto snapshot = measurement.Finish_();
    ASSERT_TRUE(snapshot.balanced_);
    ASSERT_GT(snapshot.allocationRequests_, 0);
    for (size_t allocation = 0; allocation < snapshot.allocationRequests_; ++allocation) {
        SCOPED_TRACE(allocation);
        FailAfter_(allocation);
        bool injected = false;
        try {
            (void)EvaluateBumpOverAAD(function, point, request);
        } catch (const std::bad_alloc&) {
            injected = true;
        } catch (...) {
            CancelFailure_();
            throw;
        }
        CancelFailure_();
        ASSERT_TRUE(injected);
        ASSERT_TRUE(Tape()->multi_);
        ASSERT_EQ(Tape()->numAdj_, 4);
        ASSERT_NEAR(EvaluateBumpOverAAD(function, point, request).HessianProducts()(0, 0), 2.0, 1e-10);
    }
}

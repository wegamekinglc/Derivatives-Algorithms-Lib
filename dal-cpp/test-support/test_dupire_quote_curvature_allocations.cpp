//
// Created by Codex on 2026/10/10.
//

#include <gtest/gtest.h>

#include <dal/math/aad/native.hpp>
#include <dal/model/dupirecurvature.hpp>

#include "../tests/math/aad/models/flat_ivs.hpp"
#include "../tests/model/dupireriskinputs.hpp"
#include "bcg_allocation_probe.hpp"

namespace AAD = Dal::AAD;
namespace Probe = Dal::BcgAllocationProbePrivate_;

TEST(DupireQuoteCurvatureAllocationTest, TestEveryRequestAllocationFailureRestoresWideModeAndRecovers) {
    const AAD::FlatIVS_ base(100.0, 0.05, 0.02, 0.2);
    const auto calibration = Dal::CalibrateDupireWithRisk(base, Dal::Test::SmallRiskInputs());
    AAD::BumpOverAADRequest_ request;
    request.directions_ = Dal::Matrix_<>(1, 6, 0.0);
    request.directions_(0, 0) = 1.0;
    request.steps_ = {5e-5};
    const AAD::NativeScalarFunction_ objective = [](AAD::RecordingScope_*, const Dal::Vector_<AAD::Number_>& x) {
        return x[x.size() - 6] * x[x.size() - 6];
    };
    const auto mode = AAD::SetNumResultsForAAD(true, 4);
    const auto evaluate = [&]() { return Dal::EvaluateDupireQuoteCurvature(objective, calibration, request); };
    (void)evaluate();
    Probe::Reset_();
    Probe::Measurement_ measurement;
    (void)evaluate();
    const auto snapshot = measurement.Finish_();
    ASSERT_TRUE(snapshot.balanced_);
    ASSERT_GT(snapshot.allocationRequests_, 0);
    for (size_t allocation = 0; allocation < snapshot.allocationRequests_; ++allocation) {
        SCOPED_TRACE(allocation);
        Probe::FailAfter_(allocation);
        bool injected = false;
        try {
            (void)evaluate();
        } catch (const std::bad_alloc&) {
            injected = true;
        } catch (...) {
            Probe::CancelFailure_();
            throw;
        }
        Probe::CancelFailure_();
        ASSERT_TRUE(injected);
        ASSERT_TRUE(AAD::Tape()->multi_);
        ASSERT_EQ(AAD::Tape()->numAdj_, 4);
        ASSERT_NEAR(evaluate().HessianProducts()(0, 0), 2.0, 1e-10);
    }
}

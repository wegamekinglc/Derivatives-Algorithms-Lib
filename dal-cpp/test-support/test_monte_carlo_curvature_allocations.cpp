//
// Created by Codex on 2026/10/09.
//

#include <gtest/gtest.h>

#include <memory>

#include <dal/concurrency/threadpool.hpp>
#include <dal/math/aad/native.hpp>
#include <dal/script/montecarlocurvature.hpp>

#include "bcg_allocation_probe.hpp"

namespace AAD = Dal::AAD;
namespace Script = Dal::Script;
namespace Probe = Dal::BcgAllocationProbePrivate_;

namespace {
    struct ScopedThreads_ {
        Dal::ThreadPool_* pool_ = Dal::ThreadPool_::GetInstance();
        size_t threads_ = pool_->NumThreads();
        bool active_ = pool_->IsActive();
        ScopedThreads_() { pool_->Start(1, true); }
        ~ScopedThreads_() {
            pool_->Start(threads_, true);
            if (!active_)
                pool_->Stop();
        }
    };
} // namespace

TEST(MonteCarloCurvatureAllocationTest, TestEveryCallerAllocationFailureRestoresModeAndRecovers) {
    const ScopedThreads_ threads;
    Script::ScriptValuationSettings_ valuation;
    valuation.evaluationDate_ = Dal::Date_(2026, 10, 1);
    const Script::ScriptProductData_ product("", {Dal::Cell_(Dal::Date_(2026, 10, 1))}, {"s = FIX(EQ[DAL196_TEST]) pay PAYS s * s"});
    const auto prepared =
        std::make_shared<const Script::BlackScholesSegmentedPreparation_>(Script::PrepareBlackScholesSegmentedScript(product, valuation));
    const Script::BlackScholesSegmentedPath_ kernel(prepared);
    AAD::BumpOverAADRequest_ bumps;
    bumps.directions_ = Dal::Matrix_<>(1, 4, 0.0);
    bumps.directions_(0, 0) = 1.0;
    bumps.steps_ = {0.25};
    const Dal::Vector_<> point{100.0, 0.2, 0.03, 0.01};
    const auto callerMode = AAD::SetNumResultsForAAD(true, 4);
    const auto evaluate = [&]() { return Script::EvaluateBlackScholesMonteCarloCurvature(kernel, point, 1, bumps); };
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
        ASSERT_EQ(evaluate().HessianProducts()(0, 0), 2.0);
    }
}

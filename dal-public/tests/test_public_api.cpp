//
// Created by wegam on 2026/5/30.
//

#include <gtest/gtest.h>

#include <dal/concurrency/threadpool.hpp>
#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/storage/globals.hpp>

#include <dal-public/src/global.hpp>
#include <dal-public/src/models.hpp>
#include <dal-public/src/script.hpp>
#include <dal-public/src/value.hpp>

#include <chrono>
#include <cmath>
#include <future>

using Dal::Date_;

TEST(PublicApiTest, TestEvaluationDateRoundTrip) {
    // Set a known date and read it back
    Date_ d(2026, 5, 30);
    Dal::SetEvaluationDate(d);
    Date_ result = Dal::GetEvaluationDate();

    // Use free functions Year(), Month(), Day() for Date_ access
    ASSERT_EQ(Dal::Date::Year(d), Dal::Date::Year(result));
    ASSERT_EQ(Dal::Date::Month(d), Dal::Date::Month(result));
    ASSERT_EQ(Dal::Date::Day(d), Dal::Date::Day(result));
}

TEST(PublicApiTest, TestPublicHeaderIncludeLinks) {
    // This test simply verifies the public types compile and link.
    // If we get here, the test binary linked against dal_public successfully.
    ASSERT_TRUE(true);
}

TEST(PublicApiTest, TestMonteCarloRejectsNonPositivePathCounts) {
    const Dal::Handle_<Dal::ScriptProductData_> product;
    const Dal::Handle_<Dal::ModelData_> model;

    ASSERT_THROW(Dal::ValueByMonteCarlo(product, model, 0), Dal::Exception_);
    ASSERT_THROW(Dal::ValueByMonteCarlo(product, model, -1), Dal::Exception_);
}

TEST(PublicApiTest, TestMonteCarloAcceptsOnePath) {
    const Date_ previousEvaluationDate = Dal::GetEvaluationDate();
    Dal::SetEvaluationDate(Date_(2022, 9, 25));

    const Dal::Vector_<Dal::Cell_> dates = {
        Dal::Cell_("STRIKE"),
        Dal::Cell_(Date_(2023, 9, 25)),
    };
    const Dal::Vector_<Dal::String_> events = {
        Dal::String_("100.0"),
        Dal::String_("call pays MAX(spot() - STRIKE, 0.0)"),
    };
    const auto product = Dal::NewScriptProduct(Dal::String_("one_path"), dates, events);
    const auto model = Dal::NewBSModelData(Dal::String_("one_path_model"), 100.0, 0.2, 0.05, 0.02);

    const auto result = Dal::ValueByMonteCarlo(product, model, 1);
    const bool pvIsFinite = std::isfinite(result.at(Dal::String_("PV")));
    Dal::SetEvaluationDate(previousEvaluationDate);

    ASSERT_TRUE(pvIsFinite);
}

TEST(PublicApiTest, TestMonteCarloOwnsEvaluationDateBarrierForEntireValuation) {
    using namespace std::chrono_literals;

    const Date_ originalEvaluationDate = Dal::GetEvaluationDate();
    const Date_ valuationDate(2022, 9, 25);
    const Date_ nextDate(2022, 9, 26);
    Dal::SetEvaluationDate(valuationDate);

    Dal::ThreadPool_* threadPool = Dal::ThreadPool_::GetInstance();
    threadPool->Start(1, true);
    std::promise<void> valuationBlocked;
    std::promise<void> releaseValuation;
    const std::shared_future<void> release = releaseValuation.get_future().share();
    auto blocker = threadPool->SpawnTask([&]() {
        valuationBlocked.set_value();
        release.wait();
        return true;
    });

    const Dal::Vector_<Dal::Cell_> dates = {
        Dal::Cell_("STRIKE"),
        Dal::Cell_(Date_(2023, 9, 25)),
    };
    const Dal::Vector_<Dal::String_> events = {
        Dal::String_("100.0"),
        Dal::String_("call pays MAX(spot() - STRIKE, 0.0)"),
    };
    const auto product = Dal::NewScriptProduct(Dal::String_("date_barrier"), dates, events);
    const auto model = Dal::NewBSModelData(Dal::String_("date_barrier_model"), 100.0, 0.2, 0.05, 0.02);
    auto valuation = std::async(std::launch::async, [&]() { return Dal::ValueByMonteCarlo(product, model, 1); });
    valuationBlocked.get_future().wait();

    bool valuationOwnsBarrier = false;
    {
        Dal::XGLOBAL::ValuationMutationGuard_ probe(std::try_to_lock);
        valuationOwnsBarrier = !probe.OwnsLock();
    }
    const Date_ observedDuringValuation = Dal::GetEvaluationDate();

    std::promise<void> setterStarted;
    auto setter = std::async(std::launch::async, [&]() {
        setterStarted.set_value();
        Dal::SetEvaluationDate(nextDate);
    });
    setterStarted.get_future().wait();
    const bool setterCompletedDuringValuation = setter.wait_for(0s) == std::future_status::ready;

    releaseValuation.set_value();
    const auto result = valuation.get();
    const bool blockerCompleted = blocker.get();
    setter.get();
    const Date_ dateAfterValuation = Dal::GetEvaluationDate();
    threadPool->Stop();
    Dal::SetEvaluationDate(originalEvaluationDate);

    ASSERT_TRUE(valuationOwnsBarrier);
    ASSERT_EQ(observedDuringValuation, valuationDate);
    ASSERT_FALSE(setterCompletedDuringValuation);
    ASSERT_TRUE(blockerCompleted);
    ASSERT_TRUE(std::isfinite(result.at(Dal::String_("PV"))));
    ASSERT_EQ(dateAfterValuation, nextDate);
}

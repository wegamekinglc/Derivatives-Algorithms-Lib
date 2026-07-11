//
// Created by wegam on 2022/6/12.
//

#include <gtest/gtest.h>

#include <dal/concurrency/threadpool.hpp>
#include <dal/platform/platform.hpp>
#include <dal/storage/globals.hpp>
#include <dal/time/date.hpp>
#include <dal/time/dateutils.hpp>

#include <algorithm>
#include <chrono>
#include <future>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

using namespace Dal;

static_assert(std::is_move_constructible_v<XGLOBAL::ScopedOverride_<Date_>>);
static_assert(!std::is_move_assignable_v<XGLOBAL::ScopedOverride_<Date_>>);

TEST(StorageTest, TestSetAccountingDate) {
    Date_ dt(2022, 6, 11);
    auto global = XGLOBAL::SetAccountingDateInScope(dt);
    Date_ gdt = Global::Dates_::AccountingDate();
    ASSERT_EQ(gdt, dt);
}

TEST(StorageTest, TestSetEvaluationDate) {
    Date_ dt(2022, 6, 10);
    auto global = XGLOBAL::SetEvaluationDateInScope(dt);
    Date_ gdt = Global::Dates_::EvaluationDate();
    ASSERT_EQ(gdt, dt);
}

TEST(StorageTest, TestSetAccountingDateInScope) {
    Date_ dt(2022, 6, 9);
    auto global = XGLOBAL::SetAccountingDateInScope(Date::Today());
    {
        auto ret(XGLOBAL::SetAccountingDateInScope(dt));
        Date_ gdt = Global::Dates_::AccountingDate();
        ASSERT_EQ(gdt, dt);
    }
    Date_ gdt = Global::Dates_::AccountingDate();
    ASSERT_EQ(gdt, Date::Today());
}

TEST(StorageTest, TestSetEvaluationDateInScope) {
    Date_ dt(2022, 6, 8);
    auto global = XGLOBAL::SetEvaluationDateInScope(Date::Today());
    {
        auto ret(XGLOBAL::SetEvaluationDateInScope(dt));
        Date_ gdt = Global::Dates_::EvaluationDate();
        ASSERT_EQ(gdt, dt);
    }
    Date_ gdt = Global::Dates_::EvaluationDate();
    ASSERT_EQ(gdt, Date::Today());
}

TEST(StorageTest, TestEvaluationDateScopeOwnsBarrierButAllowsConcurrentRead) {
    using namespace std::chrono_literals;

    const Date_ original = Global::Dates_::EvaluationDate();
    const Date_ stable(2024, 1, 2);
    const Date_ next(2024, 1, 3);
    std::future<void> setter;
    Date_ observed;
    bool setterCompletedInsideScope = false;

    {
        auto dateOverride = XGLOBAL::SetEvaluationDateInScope(stable);
        auto barrierProbe = std::async(std::launch::async, []() {
            XGLOBAL::ValuationMutationGuard_ probe(std::try_to_lock);
            return probe.OwnsLock();
        });
        ASSERT_FALSE(barrierProbe.get());

        std::promise<void> setterStarted;
        setter = std::async(std::launch::async, [&]() {
            setterStarted.set_value();
            Global::Dates_::SetEvaluationDate(next);
        });
        setterStarted.get_future().wait();

        auto getter = std::async(std::launch::async, []() { return Global::Dates_::EvaluationDate(); });
        observed = getter.get();
        setterCompletedInsideScope = setter.wait_for(0s) == std::future_status::ready;
    }

    setter.get();
    const Date_ afterScope = Global::Dates_::EvaluationDate();
    Global::Dates_::SetEvaluationDate(original);

    ASSERT_EQ(observed, stable);
    ASSERT_FALSE(setterCompletedInsideScope);
    ASSERT_EQ(afterScope, next);
}

TEST(StorageTest, TestEvaluationDateScopeRestoresAfterMoveAndException) {
    const Date_ original = Global::Dates_::EvaluationDate();
    const Date_ outerDate(2024, 2, 1);
    const Date_ nestedDate(2024, 2, 2);
    const Date_ exceptionDate(2024, 2, 3);

    {
        auto outer = XGLOBAL::SetEvaluationDateInScope(outerDate);
        {
            auto nested = XGLOBAL::SetEvaluationDateInScope(nestedDate);
            auto moved = std::move(nested);
            ASSERT_EQ(Global::Dates_::EvaluationDate(), nestedDate);
        }
        ASSERT_EQ(Global::Dates_::EvaluationDate(), outerDate);

        try {
            auto exceptional = XGLOBAL::SetEvaluationDateInScope(exceptionDate);
            throw 42;
        } catch (int value) {
            ASSERT_EQ(value, 42);
        }
        ASSERT_EQ(Global::Dates_::EvaluationDate(), outerDate);
    }

    ASSERT_EQ(Global::Dates_::EvaluationDate(), original);
}

TEST(StorageTest, TestEvaluationDateWorkersObserveOneStableDateWhileSetterWaits) {
    if (std::thread::hardware_concurrency() < 2)
        GTEST_SKIP() << "requires a background pool worker";

    using namespace std::chrono_literals;

    const Date_ original = Global::Dates_::EvaluationDate();
    const Date_ stable(2024, 3, 1);
    const Date_ next(2024, 3, 2);
    ThreadPool_* threadPool = ThreadPool_::GetInstance();
    threadPool->Start(2, true);

    std::future<void> setter;
    std::vector<Date_> observed(16);
    bool setterBlocked = false;
    bool allTasksSucceeded = true;
    bool allWorkersStable = false;
    {
        auto dateOverride = XGLOBAL::SetEvaluationDateInScope(stable);
        std::promise<void> setterStarted;
        setter = std::async(std::launch::async, [&]() {
            setterStarted.set_value();
            Global::Dates_::SetEvaluationDate(next);
        });
        setterStarted.get_future().wait();

        Vector_<TaskHandle_> futures;
        futures.reserve(observed.size());
        for (size_t i = 0; i < observed.size(); ++i) {
            futures.push_back(threadPool->SpawnTask([&, i]() {
                observed[i] = Global::Dates_::EvaluationDate();
                return true;
            }));
        }
        for (auto& future : futures)
            allTasksSucceeded = future.get() && allTasksSucceeded;

        setterBlocked = setter.wait_for(0s) != std::future_status::ready;
        allWorkersStable = std::all_of(observed.begin(), observed.end(), [&](const Date_& date) { return date == stable; });
    }

    setter.get();
    const Date_ afterScope = Global::Dates_::EvaluationDate();
    threadPool->Stop();
    Global::Dates_::SetEvaluationDate(original);

    ASSERT_TRUE(setterBlocked);
    ASSERT_TRUE(allTasksSucceeded);
    ASSERT_TRUE(allWorkersStable);
    ASSERT_EQ(afterScope, next);
}

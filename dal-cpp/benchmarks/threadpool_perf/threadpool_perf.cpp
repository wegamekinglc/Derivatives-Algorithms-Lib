//
// Created by wegam on 2026/7/11.
//
// Thread-pool micro-benchmark.
// Tracks dispatch/join throughput and Start/drain/Stop lifecycle cost so
// regressions in the concurrent queue or worker management surface directly.

#include <dal/platform/platform.hpp>
#include <dal/benchmarks/bench.hpp>
#include <dal/concurrency/threadpool.hpp>
#include <vector>

using namespace Dal;

namespace {
    constexpr int kNumTasks = 100000;
    constexpr int kWorkers = 4;
} // namespace

int main() {
    constexpr int kRepeats = 10;
    Bench::PrintHeader();
    ThreadPool_* pool = ThreadPool_::GetInstance();

    // Throughput: spawn kNumTasks trivial tasks across kWorkers and join.
    // The work is intentionally negligible so the measurement isolates queue
    // push/pop, task dispatch, and future hand-off overhead.
    {
        long sink = 0;
        pool->Start(kWorkers, true);
        auto r = Bench::Run("Spawn+join 100K tasks (4 workers)", [&]() {
            std::vector<TaskHandle_> handles;
            handles.reserve(kNumTasks);
            for (int i = 0; i < kNumTasks; ++i)
                handles.emplace_back(pool->SpawnTask([]() -> bool { return true; }));
            for (auto& handle : handles)
                sink += handle.get() ? 1 : 0;
        }, 3, kRepeats);
        Bench::Print(r);
        Bench::DoNotOptimize(&sink);
        pool->Stop();
    }

    // Lifecycle: a full Start + drain + Stop round-trip per iteration. Covers
    // thread creation, queue teardown, and join cost on top of the dispatch path.
    {
        long sink = 0;
        auto r = Bench::Run("Start+drain+Stop round-trip (4 workers)", [&]() {
            pool->Start(kWorkers, true);
            std::vector<TaskHandle_> handles;
            handles.reserve(kNumTasks);
            for (int i = 0; i < kNumTasks; ++i)
                handles.emplace_back(pool->SpawnTask([]() -> bool { return true; }));
            for (auto& handle : handles)
                sink += handle.get() ? 1 : 0;
            pool->Stop();
        }, 2, kRepeats);
        Bench::Print(r);
        Bench::DoNotOptimize(&sink);
    }

    return 0;
}

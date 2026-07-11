//
// Created by wegam on 2026/7/11.
//
// Stack primitive micro-benchmarks.
// Isolates the push / pop / grow / reset cost of the Stack_ and StaticStack_
// templates that the AAD tape hits once per recorded node and once per sweep
// node, so a regression in the low-level container is visible without going
// through the full tape.

#include <dal/platform/platform.hpp>
#include <dal/benchmarks/bench.hpp>
#include <dal/math/stacks.hpp>

using namespace Dal;

namespace {
    constexpr int kPushCount = 100000;
} // namespace

int main() {
    constexpr int kRepeats = 100;
    Bench::PrintHeader();

    // Push with doubling growth (Stack_::Grow path).
    {
        double sink = 0.0;
        auto r = Bench::Run("Stack_ push 100K (grow)", [&]() {
            Stack_<double, 128> s;
            for (int i = 0; i < kPushCount; ++i)
                s.Push(static_cast<double>(i));
            sink += s.Top();
        }, 3, kRepeats);
        Bench::Print(r);
        Bench::DoNotOptimize(&sink);
    }

    // Push then drain via TopAndPop.
    {
        double sink = 0.0;
        auto r = Bench::Run("Stack_ push+pop 100K", [&]() {
            Stack_<double, 128> s;
            for (int i = 0; i < kPushCount; ++i)
                s.Push(static_cast<double>(i));
            for (int i = 0; i < kPushCount; ++i)
                sink += s.TopAndPop();
        }, 3, kRepeats);
        Bench::Print(r);
        Bench::DoNotOptimize(&sink);
    }

    // Push then Reset (rewind the stack pointer without popping element-by-element).
    {
        double sink = 0.0;
        auto r = Bench::Run("Stack_ push+reset 100K", [&]() {
            Stack_<double, 128> s;
            for (int i = 0; i < kPushCount; ++i)
                s.Push(static_cast<double>(i));
            sink += s.Top();
            s.Reset();
        }, 3, kRepeats);
        Bench::Print(r);
        Bench::DoNotOptimize(&sink);
    }

    // StaticStack_ push+pop within fixed capacity (stack-allocated, no growth).
    {
        constexpr int kBatches = 1000;
        double sink = 0.0;
        auto r = Bench::Run("StaticStack_ push+pop x128 x1K", [&]() {
            StaticStack_<double, 128> s;
            for (int b = 0; b < kBatches; ++b) {
                for (int i = 0; i < 128; ++i)
                    s.Push(static_cast<double>(i));
                for (int i = 0; i < 128; ++i)
                    sink += s.TopAndPop();
            }
        }, 3, kRepeats);
        Bench::Print(r);
        Bench::DoNotOptimize(&sink);
    }

    return 0;
}

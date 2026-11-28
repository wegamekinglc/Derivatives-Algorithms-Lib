#include "ledger.hpp"

#include <cassert>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>

#include <dlfcn.h>

int main() {
    using Snapshot = TapePerfS0::AllocationSnapshot_;
    const auto handle = [](const char* name) {
        void* symbol = dlsym(RTLD_DEFAULT, name);
        assert(symbol);
        return symbol;
    };
    const auto start = reinterpret_cast<void (*)()>(handle("dal_s0_ledger_start"));
    const auto reset = reinterpret_cast<void (*)()>(handle("dal_s0_ledger_reset_peak"));
    const auto snapshot = reinterpret_cast<void (*)(Snapshot*)>(handle("dal_s0_ledger_snapshot"));
    const auto allocate = reinterpret_cast<void* (*)(size_t)>(handle("malloc"));
    const auto zero = reinterpret_cast<void* (*)(size_t, size_t)>(handle("calloc"));
    const auto resize = reinterpret_cast<void* (*)(void*, size_t)>(handle("realloc"));
    const auto align = reinterpret_cast<void* (*)(size_t, size_t)>(handle("aligned_alloc"));
    const auto release = reinterpret_cast<void (*)(void*)>(handle("free"));
    const auto state = [&] {
        Snapshot result{};
        snapshot(&result);
        return result;
    };

    start();
    void* a = allocate(64);
    void* b = zero(3, 32);
    void* c = ::operator new(128);
    void* d = ::operator new(256, std::align_val_t{64});
    void* e = align(64, 512);
    void* f = nullptr;
    assert(posix_memalign(&f, 64, 128) == 0);
    assert(a && b && c && d && e && f);
    assert(state().live_ == 1184 && state().peak_ == 1184 && state().allocations_ == 6 && state().aligned_ == 3 && state().alignedBytes_ == 896);
    release(a);
    release(b);
    ::operator delete(c);
    ::operator delete(d, std::align_val_t{64});
    release(e);
    release(f);
    assert(state().live_ == 0 && state().frees_ == 6);

    a = allocate(33);
    reset(); // Preserve already-live bytes at the hot boundary.
    assert(a && state().peak_ == 33);
    void* resized = resize(a, 40);
    assert(resized == a && state().live_ == 40 && state().peak_ == 40 && state().reallocInPlace_ == 1);
    release(resized);
    assert(state().live_ == 0);

    a = allocate(128);
    b = allocate(128);
    reset();
    constexpr size_t large = 1024 * 1024;
    resized = resize(a, large);
    const auto moved = state();
    assert(resized && resized != a && moved.live_ == large + 128 && moved.peak_ == large + 256 && moved.reallocMoved_ == 1);
    const auto tooLarge = std::numeric_limits<size_t>::max() - 4095;
    assert(!resize(resized, tooLarge) && state().reallocFailed_ == 1);
    assert(!allocate(tooLarge) && !zero(tooLarge, 2));
    assert(state().live_ == moved.live_ && state().peak_ == moved.peak_ && state().allocations_ == moved.allocations_);
    release(resized);
    release(b);
    const auto end = state();
    assert(end.live_ == 0 && end.allocations_ == end.frees_);
    std::cout << "ordinary_and_aligned_peak=1184\nhot_start=33\ninplace_peak=40\nmoved_live=" << moved.live_ << "\nmoved_overlap_peak=" << moved.peak_
              << "\nrealloc_failures=" << end.reallocFailed_ << "\nend_live=" << end.live_ << "\nallocations=" << end.allocations_
              << "\nfrees=" << end.frees_ << '\n';
}

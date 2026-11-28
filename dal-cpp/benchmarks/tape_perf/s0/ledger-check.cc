#include "ledger.hpp"

#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <stdexcept>

#include <dlfcn.h>

namespace {
    using Snapshot = TapePerfS0::AllocationSnapshot_;

    void Require(bool condition, const char* message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    template <class F_> F_ Symbol(const char* name) {
        void* symbol = dlsym(RTLD_DEFAULT, name);
        Require(symbol != nullptr, name);
        return reinterpret_cast<F_>(symbol);
    }

    struct Ledger_ {
        void (*start_)() = Symbol<void (*)()>("dal_s0_ledger_start");
        void (*reset_)() = Symbol<void (*)()>("dal_s0_ledger_reset_peak");
        void (*snapshot_)(Snapshot*) = Symbol<void (*)(Snapshot*)>("dal_s0_ledger_snapshot");
        void* (*allocate_)(size_t) = Symbol<void* (*)(size_t)>("malloc");
        void* (*zero_)(size_t, size_t) = Symbol<void* (*)(size_t, size_t)>("calloc");
        void* (*resize_)(void*, size_t) = Symbol<void* (*)(void*, size_t)>("realloc");
        void* (*align_)(size_t, size_t) = Symbol<void* (*)(size_t, size_t)>("aligned_alloc");
        void (*release_)(void*) = Symbol<void (*)(void*)>("free");

        Snapshot State() const {
            Snapshot result{};
            snapshot_(&result);
            return result;
        }
    };

    void CheckOrdinaryAndAligned(const Ledger_& ledger) {
        void* a = ledger.allocate_(64);
        void* b = ledger.zero_(3, 32);
        void* c = ::operator new(128);
        void* d = ::operator new(256, std::align_val_t{64});
        void* e = ledger.align_(64, 512);
        void* f = nullptr;
        Require(posix_memalign(&f, 64, 128) == 0, "posix_memalign failed");
        for (void* allocation : {a, b, c, d, e, f})
            Require(allocation != nullptr, "allocation failed");
        const auto active = ledger.State();
        Require(active.live_ == 1184 && active.peak_ == 1184, "incorrect ordinary and aligned peak");
        Require(active.allocations_ == 6, "incorrect combined allocation count");
        Require(active.aligned_ == 3 && active.alignedBytes_ == 896, "incorrect aligned accounting");
        ledger.release_(a);
        ledger.release_(b);
        ::operator delete(c);
        ::operator delete(d, std::align_val_t{64});
        ledger.release_(e);
        ledger.release_(f);
        const auto end = ledger.State();
        Require(end.live_ == 0 && end.frees_ == 6, "ordinary and aligned free counts");
    }

    void CheckInPlace(const Ledger_& ledger) {
        void* a = ledger.allocate_(33);
        ledger.reset_();
        Require(a && ledger.State().peak_ == 33, "hot reset must preserve live allocations");
        void* resized = ledger.resize_(a, 40);
        const auto state = ledger.State();
        Require(resized == a && state.live_ == 40 && state.peak_ == 40 && state.reallocInPlace_ == 1, "incorrect in-place realloc accounting");
        ledger.release_(resized);
        Require(ledger.State().live_ == 0, "in-place allocation was not released");
    }

    void CheckAllocationFailures(const Ledger_& ledger, void* resized, const Snapshot& moved) {
        const auto tooLarge = std::numeric_limits<size_t>::max() - 4095;
        Require(!ledger.resize_(resized, tooLarge) && ledger.State().reallocFailed_ == 1, "failed realloc was not recorded");
        Require(!ledger.allocate_(tooLarge) && !ledger.zero_(tooLarge, 2), "oversized malloc or calloc succeeded");
        const auto after = ledger.State();
        Require(after.live_ == moved.live_ && after.peak_ == moved.peak_ && after.allocations_ == moved.allocations_,
                "failed allocations changed live bytes, peak or allocation count");
    }

    Snapshot CheckMoved(const Ledger_& ledger) {
        void* a = ledger.allocate_(128);
        void* b = ledger.allocate_(128);
        ledger.reset_();
        constexpr size_t large = 1024 * 1024;
        void* resized = ledger.resize_(a, large);
        const auto moved = ledger.State();
        Require(resized && resized != a && moved.reallocMoved_ == 1, "expected moved realloc");
        Require(moved.live_ == large + 128 && moved.peak_ == large + 256, "moved realloc must charge overlapping old and new bytes");
        CheckAllocationFailures(ledger, resized, moved);
        ledger.release_(resized);
        ledger.release_(b);
        return moved;
    }
} // namespace

int main() {
    const Ledger_ ledger;
    ledger.start_();
    CheckOrdinaryAndAligned(ledger);
    CheckInPlace(ledger);
    const auto moved = CheckMoved(ledger);
    const auto end = ledger.State();
    Require(end.live_ == 0 && end.allocations_ == end.frees_, "ledger is not balanced");
    std::cout << "ordinary_and_aligned_peak=1184\nhot_start=33\ninplace_peak=40\nmoved_live=" << moved.live_ << "\nmoved_overlap_peak=" << moved.peak_
              << "\nrealloc_failures=" << end.reallocFailed_ << "\nend_live=" << end.live_ << "\nallocations=" << end.allocations_
              << "\nfrees=" << end.frees_ << '\n';
}

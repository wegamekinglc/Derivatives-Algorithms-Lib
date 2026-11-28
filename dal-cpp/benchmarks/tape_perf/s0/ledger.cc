#include "ledger.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdlib>

extern "C" {
void* __libc_malloc(size_t);
void* __libc_calloc(size_t, size_t);
void* __libc_realloc(void*, size_t);
void* __libc_memalign(size_t, size_t);
void __libc_free(void*);
}

namespace {
    // One measured thread per process. The table and the book itself allocate nothing.
    __thread bool tracking __attribute__((tls_model("initial-exec"))) = false;
    constexpr size_t TABLE_SIZE = 1 << 17;
    constexpr uintptr_t DELETED = 1;
    struct Entry_ {
        uintptr_t address_;
        size_t bytes_;
    };
    Entry_ entries[TABLE_SIZE]{};
    TapePerfS0::AllocationSnapshot_ stats{};

    Entry_* InsertionSlot(bool insert, Entry_* deleted, Entry_* empty) {
        if (!insert)
            return nullptr;
        if (deleted)
            return deleted;
        if (!empty)
            std::abort();
        return empty;
    }

    Entry_* Find(uintptr_t key, bool insert) {
        size_t slot = ((key >> 4) ^ (key >> 20)) & (TABLE_SIZE - 1);
        Entry_* deleted = nullptr;
        for (size_t i = 0; i < TABLE_SIZE; ++i) {
            auto& entry = entries[slot];
            if (entry.address_ == key)
                return &entry;
            if (entry.address_ == 0)
                return InsertionSlot(insert, deleted, &entry);
            if (entry.address_ == DELETED && !deleted)
                deleted = &entry;
            slot = (slot + 1) & (TABLE_SIZE - 1);
        }
        return InsertionSlot(insert, deleted, nullptr);
    }

    void Peak() { stats.peak_ = std::max(stats.peak_, stats.live_); }

    void Add(void* ptr, size_t bytes) {
        if (!tracking || !ptr)
            return;
        auto* entry = Find(reinterpret_cast<uintptr_t>(ptr), true);
        if (entry->address_ > DELETED)
            std::abort();
        *entry = {reinterpret_cast<uintptr_t>(ptr), bytes};
        ++stats.allocations_;
        stats.live_ += bytes;
        stats.largest_ = std::max(stats.largest_, bytes);
        Peak();
    }

    void Remove(Entry_* entry) {
        if (!entry)
            return;
        stats.live_ -= entry->bytes_;
        ++stats.frees_;
        *entry = {DELETED, 0};
    }

    void FailedResize(Entry_* old, bool hadPointer, size_t bytes) {
        // glibc releases a non-null pointer when realloc(ptr, 0) returns null.
        if (hadPointer && bytes == 0)
            Remove(old);
        else
            ++stats.reallocFailed_;
    }

    void ResizeInPlace(Entry_* old, size_t bytes) {
        stats.live_ = stats.live_ - old->bytes_ + bytes;
        old->bytes_ = bytes;
        ++stats.reallocInPlace_;
        stats.largest_ = std::max(stats.largest_, bytes);
        Peak();
    }

    bool PowerOfTwo(size_t n) { return n != 0 && (n & (n - 1)) == 0; }

    void* Aligned(size_t alignment, size_t bytes) {
        void* result = __libc_memalign(alignment, bytes);
        if (tracking && result) {
            ++stats.aligned_;
            stats.alignedBytes_ += bytes;
        }
        Add(result, bytes);
        return result;
    }
} // namespace

extern "C" {
void dal_s0_ledger_start() { tracking = true; }
void dal_s0_ledger_reset_peak() { stats.peak_ = stats.live_; }
void dal_s0_ledger_snapshot(TapePerfS0::AllocationSnapshot_* out) { *out = stats; }

void* malloc(size_t bytes) noexcept {
    void* result = __libc_malloc(bytes);
    Add(result, bytes);
    return result;
}

void* calloc(size_t count, size_t bytes) noexcept {
    void* result = __libc_calloc(count, bytes);
    if (result)
        Add(result, count * bytes);
    return result;
}

void free(void* ptr) noexcept {
    if (tracking && ptr)
        Remove(Find(reinterpret_cast<uintptr_t>(ptr), false));
    __libc_free(ptr);
}

void* realloc(void* ptr, size_t bytes) noexcept {
    if (!tracking)
        return __libc_realloc(ptr, bytes);
    const auto address = reinterpret_cast<uintptr_t>(ptr);
    const bool hadPointer = ptr != nullptr;
    auto* old = hadPointer ? Find(address, false) : nullptr;
    void* result = __libc_realloc(ptr, bytes);
    if (!result) {
        FailedResize(old, hadPointer, bytes);
    } else if (old && reinterpret_cast<uintptr_t>(result) == address) {
        ResizeInPlace(old, bytes);
    } else {
        // A moved realloc is charged as simultaneous old and new requested bytes.
        Add(result, bytes);
        if (old)
            ++stats.reallocMoved_;
        Remove(old);
    }
    return result;
}

void* aligned_alloc(size_t alignment, size_t bytes) noexcept {
    if (!PowerOfTwo(alignment)) {
        errno = EINVAL;
        return nullptr;
    }
    return Aligned(alignment, bytes);
}

// Keep the glibc interposition symbol for callers of the legacy allocator.
// flawfinder: ignore
void* memalign(size_t alignment, size_t bytes) noexcept { return Aligned(alignment, bytes); }

int posix_memalign(void** ptr, size_t alignment, size_t bytes) noexcept {
    if (alignment < sizeof(void*) || !PowerOfTwo(alignment))
        return EINVAL;
    const int oldErrno = errno;
    void* result = Aligned(alignment, bytes);
    errno = oldErrno;
    if (!result)
        return ENOMEM;
    *ptr = result;
    return 0;
}
} // extern "C"

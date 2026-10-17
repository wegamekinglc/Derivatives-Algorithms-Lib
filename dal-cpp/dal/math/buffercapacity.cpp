#include <algorithm>
#include <array>
#include <atomic>
#include <limits>
#include <map>
#include <mutex>
#include <string>
#include <thread>

#include <dal/math/bufferallocation.hpp>
#include <dal/math/buffercapacity.hpp>
#include <dal/platform/platform.hpp>
#include <dal/utilities/exceptions.hpp>

namespace Dal {
    namespace {
        std::atomic<size_t> activeBufferScopes{0};
        thread_local BufferCapacityBudget_* currentBufferBudget = nullptr;
        thread_local const void* currentBufferAttachment = nullptr;
        thread_local Detail::OwnedBufferAccount_* currentOwnedAccount = nullptr;

        class OwnedAllocationTicket_ {
            Detail::OwnedBufferAccount_* account_;
            size_t bytes_;

        public:
            explicit OwnedAllocationTicket_(size_t bytes) : account_(currentOwnedAccount), bytes_(bytes) {
                if (account_ != nullptr)
                    account_->Reserve(bytes_);
            }
            OwnedAllocationTicket_(const OwnedAllocationTicket_&) = delete;
            OwnedAllocationTicket_& operator=(const OwnedAllocationTicket_&) = delete;
            ~OwnedAllocationTicket_() noexcept {
                if (account_ != nullptr)
                    account_->Release(bytes_);
            }
            void Commit() noexcept { account_ = nullptr; }
        };

        void RequireBufferCapacity(size_t current, size_t extra, size_t limit) {
            if (extra > limit - current) {
                const auto required =
                    extra <= std::numeric_limits<size_t>::max() - current ? std::to_string(current + extra) : std::string("above-size_t-range");
                THROW("Scratch buffer capacity budget exceeded [required=" + required + ", limit=" + std::to_string(limit) + "]");
            }
        }
    } // namespace

    struct BufferCapacityBudget_::Impl_ {
        const size_t limitBytes_;
        // Worker scopes share one budget. Keep the hot-path accounting atomic so
        // allocations from different workers do not serialize on a mutex.
        std::atomic<size_t> capacityBytes_{0};
        std::atomic<size_t> peakBytes_{0};
        std::atomic<size_t> activeScopes_{0};
        // A buffer may be destroyed under a different scope than the one that
        // allocated it, so the byte count cannot be attributed from the call site.
        // Shard the address ledger so unrelated worker allocations do not share one mutex.
        static constexpr size_t ADDRESS_SHARDS = 32;
        std::array<std::mutex, ADDRESS_SHARDS> allocationsMutex_;
        std::array<std::map<std::uintptr_t, size_t>, ADDRESS_SHARDS> allocations_;

        explicit Impl_(size_t limit) : limitBytes_(limit) {}

        [[nodiscard]] static size_t Shard(std::uintptr_t address) noexcept { return (address >> 4) % ADDRESS_SHARDS; }
    };

    struct BufferCapacityScope_::Attachment_ {
        BufferCapacityBudget_* budget_;
        const size_t fixedBytes_;
        const std::thread::id owner_ = std::this_thread::get_id();
        BufferCapacityBudget_* previousBudget_ = Detail::CurrentBufferBudget();
        const void* previousAttachment_ = currentBufferAttachment;

        Attachment_(BufferCapacityBudget_* budget, size_t bytes) : budget_(budget), fixedBytes_(bytes) {}
    };

    BufferCapacityBudget_::BufferCapacityBudget_(size_t limitBytes) : impl_(std::make_unique<Impl_>(limitBytes)) {}

    BufferCapacityBudget_::~BufferCapacityBudget_() noexcept {
        if (impl_->activeScopes_.load(std::memory_order_relaxed) != 0)
            std::terminate();
    }

    size_t BufferCapacityBudget_::LimitBytes() const { return impl_->limitBytes_; }

    size_t BufferCapacityBudget_::CapacityBytes() const { return impl_->capacityBytes_.load(std::memory_order_relaxed); }

    size_t BufferCapacityBudget_::PeakCapacityBytes() const { return impl_->peakBytes_.load(std::memory_order_relaxed); }

    void BufferCapacityBudget_::Reserve(size_t bytes) {
        auto current = impl_->capacityBytes_.load(std::memory_order_relaxed);
        size_t updated = 0;
        for (;;) {
            RequireBufferCapacity(current, bytes, impl_->limitBytes_);
            updated = current + bytes;
            if (impl_->capacityBytes_.compare_exchange_weak(current, updated, std::memory_order_relaxed))
                break;
        }
        auto peak = impl_->peakBytes_.load(std::memory_order_relaxed);
        while (peak < updated && !impl_->peakBytes_.compare_exchange_weak(peak, updated, std::memory_order_relaxed))
            ;
    }

    void BufferCapacityBudget_::Cancel(size_t bytes) noexcept {
        const auto previous = impl_->capacityBytes_.fetch_sub(bytes, std::memory_order_relaxed);
        if (previous < bytes)
            std::terminate();
    }

    void BufferCapacityBudget_::Commit(const void* allocation, size_t bytes) {
        const auto address = reinterpret_cast<std::uintptr_t>(allocation);
        const auto shard = Impl_::Shard(address);
        const std::lock_guard<std::mutex> lock(impl_->allocationsMutex_[shard]);
        REQUIRE(impl_->allocations_[shard].emplace(address, bytes).second, "Scratch buffer allocation: address is already admitted");
    }

    size_t BufferCapacityBudget_::Detach(std::uintptr_t allocation) noexcept {
        const auto shard = Impl_::Shard(allocation);
        const std::lock_guard<std::mutex> lock(impl_->allocationsMutex_[shard]);
        const auto found = impl_->allocations_[shard].find(allocation);
        if (found != impl_->allocations_[shard].end()) {
            const auto bytes = found->second;
            impl_->allocations_[shard].erase(found);
            return bytes;
        }
        return 0;
    }

    BufferCapacityScope_::BufferCapacityScope_(BufferCapacityBudget_* budget, size_t fixedPayloadBytes)
        : BufferCapacityScope_(budget, fixedPayloadBytes, false) {}

    BufferCapacityScope_ BufferCapacityScope_::ForWorker(BufferCapacityBudget_* budget, size_t fixedPayloadBytes) {
        return BufferCapacityScope_(budget, fixedPayloadBytes, true);
    }

    BufferCapacityScope_::BufferCapacityScope_(BufferCapacityBudget_* budget, size_t fixedPayloadBytes, bool reuseAttachment) {
        REQUIRE(budget != nullptr, "Scratch buffer scope: budget must not be null");
        const auto* previous = Detail::CurrentBufferBudget();
        REQUIRE(previous == nullptr || (reuseAttachment && previous == budget), "Scratch buffer scope: nested scope is unsupported");
        auto attachment = std::make_unique<Attachment_>(budget, fixedPayloadBytes);
        budget->Reserve(fixedPayloadBytes);
        ++budget->impl_->activeScopes_;
        attachment_ = std::move(attachment);
        // Publish before attaching; retire only after restoring the previous attachment.
        activeBufferScopes.fetch_add(1, std::memory_order_relaxed);
        Detail::CurrentBufferBudget() = budget;
        currentBufferAttachment = attachment_.get();
    }

    BufferCapacityScope_::~BufferCapacityScope_() noexcept { Close(); }

    void BufferCapacityScope_::Close() {
        if (!attachment_)
            return;
        REQUIRE(attachment_->owner_ == std::this_thread::get_id(), "Scratch buffer scope: close must run on the owning thread");
        REQUIRE(currentBufferAttachment == attachment_.get(), "Scratch buffer scope: close must follow attachment order");
        auto* budget = attachment_->budget_;
        budget->Cancel(attachment_->fixedBytes_);
        --budget->impl_->activeScopes_;
        Detail::CurrentBufferBudget() = attachment_->previousBudget_;
        currentBufferAttachment = attachment_->previousAttachment_;
        activeBufferScopes.fetch_sub(1, std::memory_order_relaxed);
        attachment_.reset();
    }

    namespace Detail {
        OwnedBufferScope_::OwnedBufferScope_(OwnedBufferAccount_* account) : account_(account) {
            REQUIRE(account_ != nullptr, "Owned buffer scope: account must not be null");
            REQUIRE(currentOwnedAccount == nullptr, "Owned buffer scope: nested ownership is unsupported");
            activeBufferScopes.fetch_add(1, std::memory_order_relaxed);
            currentOwnedAccount = account_;
        }

        OwnedBufferScope_::~OwnedBufferScope_() noexcept {
            if (currentOwnedAccount != account_)
                std::terminate();
            currentOwnedAccount = nullptr;
            activeBufferScopes.fetch_sub(1, std::memory_order_relaxed);
        }

        namespace {
            template <class A_, class D_>
#if defined(__GNUC__) || defined(__clang__)
            __attribute__((cold, noinline))
#elif defined(_MSC_VER)
            __declspec(noinline)
#endif
            auto AllocateWithBudgetContext(size_t bytes, A_ allocate, D_ deallocate) {
                OwnedAllocationTicket_ owned(bytes);
                auto* budget = currentBufferBudget;
                auto* result = budget == nullptr ? allocate() : AllocateTrackedBuffer(budget, bytes, 1, allocate, deallocate);
                owned.Commit();
                return result;
            }

            template <class A_, class D_> auto AllocateStorage(size_t bytes, A_ allocate, D_ deallocate) {
                return activeBufferScopes.load(std::memory_order_relaxed) == 0 || bytes == 0 ? allocate()
                                                                                             : AllocateWithBudgetContext(bytes, allocate, deallocate);
            }

            template <class D_>
#if defined(__GNUC__) || defined(__clang__)
            __attribute__((cold, noinline))
#elif defined(_MSC_VER)
            __declspec(noinline)
#endif
            void DeallocateWithBudgetContext(void* allocation, size_t bytes, D_ deallocate) noexcept {
                // The address ledger also covers buffers allocated outside this scope:
                // an unknown address cancels nothing instead of underflowing the budget.
                if (auto* budget = currentBufferBudget)
                    DeallocateTrackedBuffer(budget, allocation, deallocate);
                else
                    deallocate(allocation);
                if (currentOwnedAccount != nullptr)
                    currentOwnedAccount->Release(bytes);
            }

            template <class D_> void DeallocateStorage(void* allocation, size_t bytes, D_ deallocate) noexcept {
                if (activeBufferScopes.load(std::memory_order_relaxed) == 0)
                    deallocate(allocation);
                else
                    DeallocateWithBudgetContext(allocation, bytes, deallocate);
            }
        } // namespace

        BufferCapacitySuspension_::BufferCapacitySuspension_() noexcept
            : previousBudget_(CurrentBufferBudget()), previousAttachment_(currentBufferAttachment), owner_(std::this_thread::get_id()) {
            CurrentBufferBudget() = nullptr;
            currentBufferAttachment = nullptr;
        }

        BufferCapacitySuspension_::~BufferCapacitySuspension_() noexcept {
            if (owner_ != std::this_thread::get_id() || CurrentBufferBudget() != nullptr || currentBufferAttachment != nullptr)
                std::terminate();
            CurrentBufferBudget() = previousBudget_;
            currentBufferAttachment = previousAttachment_;
        }

        BufferCapacityBudget_*& CurrentBufferBudget() noexcept { return currentBufferBudget; }

        void* AllocateBufferStorage(size_t bytes) {
            return AllocateStorage(bytes, [bytes] { return ::operator new(bytes); }, [](void* allocation) { ::operator delete(allocation); });
        }

        void* AllocateBufferStorage(size_t bytes, std::align_val_t alignment) {
            return AllocateStorage(
                bytes, [bytes, alignment] { return ::operator new(bytes, alignment); },
                [alignment](void* allocation) { ::operator delete(allocation, alignment); });
        }

        void DeallocateBufferStorage(void* allocation, size_t bytes) noexcept {
#if defined(__cpp_sized_deallocation)
            DeallocateStorage(allocation, bytes, [bytes](void* storage) { ::operator delete(storage, bytes); });
#else
            DeallocateStorage(allocation, bytes, [](void* storage) { ::operator delete(storage); });
#endif
        }

        void DeallocateBufferStorage(void* allocation, size_t bytes, std::align_val_t alignment) noexcept {
#if defined(__cpp_sized_deallocation)
            DeallocateStorage(allocation, bytes, [bytes, alignment](void* storage) { ::operator delete(storage, bytes, alignment); });
#else
            DeallocateStorage(allocation, bytes, [alignment](void* storage) { ::operator delete(storage, alignment); });
#endif
        }

        BufferAllocationTicket_::BufferAllocationTicket_(BufferCapacityBudget_* budget, size_t count, size_t elementBytes) : budget_(budget) {
            REQUIRE(elementBytes > 0 && count <= std::numeric_limits<size_t>::max() / elementBytes,
                    "Scratch buffer allocation: payload extent overflow");
            bytes_ = count * elementBytes;
            budget_->Reserve(bytes_);
        }

        BufferAllocationTicket_::~BufferAllocationTicket_() noexcept {
            if (budget_ != nullptr)
                budget_->Cancel(bytes_);
        }

        void BufferAllocationTicket_::Commit(const void* allocation) {
            budget_->Commit(allocation, bytes_);
            budget_ = nullptr;
        }

        BufferDeallocationTicket_::BufferDeallocationTicket_(BufferCapacityBudget_* budget, const void* allocation) noexcept
            : budget_(budget), bytes_(budget->Detach(reinterpret_cast<std::uintptr_t>(allocation))) {}

        BufferDeallocationTicket_::~BufferDeallocationTicket_() noexcept { budget_->Cancel(bytes_); }
    } // namespace Detail
} // namespace Dal

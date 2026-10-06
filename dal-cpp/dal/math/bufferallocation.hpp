//
// Created by Codex on 2026/10/06.
//

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <string>
#include <type_traits>

namespace Dal {
    class String_;
    class BufferCapacityBudget_;
    namespace exception {
        class XStackInfo_;
    } // namespace exception

    namespace Detail {
        BufferCapacityBudget_*& CurrentBufferBudget() noexcept;

        class BufferAllocationTicket_ {
            BufferCapacityBudget_* budget_;
            size_t bytes_;

        public:
            BufferAllocationTicket_(BufferCapacityBudget_* budget, size_t count, size_t elementBytes);
            ~BufferAllocationTicket_() noexcept;
            BufferAllocationTicket_(const BufferAllocationTicket_&) = delete;
            BufferAllocationTicket_& operator=(const BufferAllocationTicket_&) = delete;
            void Commit(const void* allocation);
        };

        void ReleaseBufferAllocation(std::uintptr_t allocation) noexcept;

        template <class A_, class D_> auto AllocateBuffer(size_t count, size_t elementBytes, A_ allocate, D_ deallocate) {
            auto* budget = CurrentBufferBudget();
            if (budget == nullptr || count == 0)
                return allocate();
            BufferAllocationTicket_ ticket(budget, count, elementBytes);
            auto* allocation = allocate();
            try {
                ticket.Commit(allocation);
            } catch (...) {
                deallocate(allocation);
                throw;
            }
            return allocation;
        }

        template <class T_, class D_> void DeallocateBuffer(T_* allocation, D_ deallocate) noexcept {
            const auto identity = reinterpret_cast<std::uintptr_t>(allocation);
            deallocate(allocation);
            if (CurrentBufferBudget() != nullptr)
                ReleaseBufferAllocation(identity);
        }

        inline void* AllocateBufferObject(size_t bytes) {
            return AllocateBuffer(1, bytes, [bytes] { return ::operator new(bytes); }, [](void* allocation) { ::operator delete(allocation); });
        }

        inline void* AllocateBufferObject(size_t bytes, std::align_val_t alignment) {
            return AllocateBuffer(
                1, bytes, [bytes, alignment] { return ::operator new(bytes, alignment); },
                [alignment](void* allocation) { ::operator delete(allocation, alignment); });
        }

        template <class... A_> void* AllocateBufferObjectNothrow(size_t bytes, A_... arguments) noexcept {
            try {
                return AllocateBufferObject(bytes, arguments...);
            } catch (...) {
                return nullptr;
            }
        }

        inline void DeallocateBufferObject(void* allocation) noexcept {
            DeallocateBuffer(allocation, [](void* storage) { ::operator delete(storage); });
        }

        inline void DeallocateBufferObject(void* allocation, std::align_val_t alignment) noexcept {
            DeallocateBuffer(allocation, [alignment](void* storage) { ::operator delete(storage, alignment); });
        }

        template <class T_> class BufferAllocator_ {
        public:
            using value_type = T_;
            using size_type = size_t;
            using difference_type = std::ptrdiff_t;
            using is_always_equal = std::true_type;
            using propagate_on_container_move_assignment = std::true_type;
            template <class U_> struct rebind {
                using other = BufferAllocator_<U_>;
            };

            BufferAllocator_() noexcept = default;
            template <class U_> BufferAllocator_(const BufferAllocator_<U_>&) noexcept {}

            [[nodiscard]] T_* allocate(size_t count) {
                std::allocator<T_> allocator;
                return AllocateBuffer(
                    count, sizeof(T_), [&allocator, count] { return allocator.allocate(count); },
                    [&allocator, count](T_* allocation) { allocator.deallocate(allocation, count); });
            }

            void deallocate(T_* allocation, size_t count) noexcept {
                DeallocateBuffer(allocation, [count](T_* storage) { std::allocator<T_>{}.deallocate(storage, count); });
            }
        };

        template <class T_, class U_> bool operator==(const BufferAllocator_<T_>&, const BufferAllocator_<U_>&) noexcept { return true; }
        template <class T_, class U_> bool operator!=(const BufferAllocator_<T_>&, const BufferAllocator_<U_>&) noexcept { return false; }

        template <class T_> struct TrackBuffer_ : std::bool_constant<!std::is_same_v<T_, String_> && !std::is_same_v<T_, exception::XStackInfo_>> {};

        template <class C_, class R_, class A_> struct TrackBuffer_<std::basic_string<C_, R_, A_>> : std::false_type {};

        template <class T_> using VectorAllocator_ = std::conditional_t<TrackBuffer_<T_>::value, BufferAllocator_<T_>, std::allocator<T_>>;
    } // namespace Detail
} // namespace Dal

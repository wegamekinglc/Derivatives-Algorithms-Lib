//
// Created by Codex on 2026/10/7.
//

#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

namespace Dal::AAD {
    class Number_;
    class Tape_;
    class RecordingScope_;
    class NativeInputSlots_;
    class TapNode_;

    class EventStorageTicket_ {
        Tape_* tape_;
        size_t bytes_;

    public:
        EventStorageTicket_(Tape_* tape, size_t bytes);
        ~EventStorageTicket_() noexcept;
        EventStorageTicket_(const EventStorageTicket_&) = delete;
        EventStorageTicket_& operator=(const EventStorageTicket_&) = delete;
        void Commit() noexcept { tape_ = nullptr; }
    };

    class ReverseEvent_ {
    public:
        virtual ~ReverseEvent_() noexcept = default;
        virtual void Reverse(bool multi, size_t width) = 0;
    };

    struct ReverseEventDeleter_ {
        Tape_* tape_ = nullptr;
        size_t bytes_ = 0;
        void operator()(ReverseEvent_* event) const noexcept;
    };

    using ReverseEventHandle_ = std::unique_ptr<ReverseEvent_, ReverseEventDeleter_>;

    struct NativeRecordingIdentity_ {
        std::uint64_t recording_;
        bool multi_;
        size_t width_;
    };

    struct NativeRecordedOperation_ {
        [[nodiscard]] static Tape_* Begin(RecordingScope_* recording);
        static void ValidateInput(const NativeInputSlots_& slots, const Number_& input);
        static void Prepare(Tape_* tape);
        static void Commit(RecordingScope_* recording, ReverseEventHandle_ event);
        static void Fail(RecordingScope_* recording);
        static void ReserveStorage(Tape_* tape, size_t bytes);
        static void ReleaseStorage(Tape_* tape, size_t bytes) noexcept;
        static void ObserveScratch(Tape_* tape, size_t bytes) noexcept;
        [[nodiscard]] static NativeRecordingIdentity_ AccuracyRecording(RecordingScope_* recording, bool reverse);

        template <class E_, class... A_> static std::unique_ptr<E_, ReverseEventDeleter_> MakeEvent(Tape_* tape, A_&&... arguments) {
            EventStorageTicket_ ticket(tape, sizeof(E_));
            auto* event = new E_(std::forward<A_>(arguments)...);
            ticket.Commit();
            return std::unique_ptr<E_, ReverseEventDeleter_>(event, ReverseEventDeleter_{tape, sizeof(E_)});
        }
    };

    template <class T_> class ReverseEventAllocator_ {
        template <class U_> friend class ReverseEventAllocator_;
        Tape_* tape_;

    public:
        using value_type = T_;
        using is_always_equal = std::false_type;
        explicit ReverseEventAllocator_(Tape_* tape) noexcept : tape_(tape) {}
        template <class U_> ReverseEventAllocator_(const ReverseEventAllocator_<U_>& other) noexcept : tape_(other.tape_) {}
        [[nodiscard]] T_* allocate(size_t count) {
            if (count > static_cast<size_t>(std::numeric_limits<std::ptrdiff_t>::max()) / sizeof(T_))
                throw std::bad_array_new_length();
            EventStorageTicket_ ticket(tape_, count * sizeof(T_));
            auto* result = std::allocator<T_>().allocate(count);
            ticket.Commit();
            return result;
        }
        void deallocate(T_* allocation, size_t count) noexcept {
            std::allocator<T_>().deallocate(allocation, count);
            NativeRecordedOperation_::ReleaseStorage(tape_, count * sizeof(T_));
        }
        template <class U_> bool operator==(const ReverseEventAllocator_<U_>& other) const noexcept { return tape_ == other.tape_; }
        template <class U_> bool operator!=(const ReverseEventAllocator_<U_>& other) const noexcept { return !(*this == other); }
    };

    // Capture-only index: finish input validation before allocating output slots.
    class NativeInputSlots_ {
        friend struct NativeRecordedOperation_;
        using Range_ = std::pair<const TapNode_*, const TapNode_*>;
        Tape_* tape_;
        std::vector<Range_, ReverseEventAllocator_<Range_>> ranges_;

    public:
        explicit NativeInputSlots_(Tape_* tape);
        NativeInputSlots_(const NativeInputSlots_&) = delete;
        NativeInputSlots_& operator=(const NativeInputSlots_&) = delete;
    };
} // namespace Dal::AAD

/*
 * Modified by wegamekinglc on 2020/12/13.
 * Written by Antoine Savine in 2018
 * This code is the strict IP of Antoine Savine
 * License to use and alter this code for personal and commercial applications
 * is freely granted to any person or company who purchased a copy of the book
 * Modern Computational Finance: AAD and Parallel Simulations
 * Antoine Savine
 * Wiley, 2018
 * As long as this comment is preserved at the top of the file
 */

#pragma once

#include <memory>

#if defined(DAL_USE_XAD_AAD) || defined(DAL_USE_CODIPACK_AAD) || defined(DAL_USE_ADEPT_AAD)
#error External AAD backend macros are no longer supported; rebuild DAL and consumers with native AAD
#endif

#if defined(DAL_ENABLE_AAD_LIFETIME_DIAGNOSTICS)
#include <atomic>
#include <limits>
#endif

#include <dal/math/aad/blocklist.hpp>
#include <dal/math/aad/node.hpp>
#include <dal/math/aad/reverseevent.hpp>

namespace Dal::AAD {
    class Number_;
    class ReverseEvent_;
    struct NativeRecordedOperation_;
    constexpr size_t BLOCK_SIZE = 16384;
    constexpr size_t ADJ_SIZE = 32768;
    constexpr size_t DATA_SIZE = 65536;

    namespace NativeThread {
        inline thread_local bool eventValidationRequired = false;
    } // namespace NativeThread

    class Tape_ {
    public:
        explicit Tape_(bool = true) : multi_(false), numAdj_(1), pad_{}, reverseEvents_(nullptr, ReverseEventsDeleter_{this}) {}
        ~Tape_() noexcept;
        Tape_(const Tape_&) = delete;
        Tape_& operator=(const Tape_&) = delete;
        Tape_(Tape_&&) = delete;
        Tape_& operator=(Tape_&&) = delete;

        using Iterator_ = BlockList_<TapNode_, BLOCK_SIZE>::Iterator_;

        bool multi_;
        size_t numAdj_;
        BlockList_<double, ADJ_SIZE> adjointsMulti_;
        BlockList_<double, DATA_SIZE> ders_;
        BlockList_<double*, DATA_SIZE> argPtrs_;
        BlockList_<TapNode_, BLOCK_SIZE> nodes_;
        // Cold event ownership occupies the remainder of the original padding.
        char pad_[64 - 6 * sizeof(void*)];

        friend auto SetNumResultsForAAD(bool, size_t);
        friend struct NumResultsResetterForAAD_;
        friend class Number_;
        friend void Clear(Tape_& tape);
        friend void Mark(Tape_& tape);
        friend void RewindToMark(Tape_& tape);
        friend void Rewind(Tape_& tape);
        friend void PropagateMarkToStart(Tape_& tape);
        friend void PropagateToStart(Tape_& tape);
        friend void PropagateToMark(Tape_& tape);
        friend void ZeroAdjoints(Tape_& tape);

        template <size_t N_> TapNode_* RecordNode() { return AllocateNode<N_>(); }

        [[nodiscard]] bool HasReverseEventState() const { return reverseFailed_ || reverseEvents_ != nullptr; }
        void RequireReverseEventState(const char* operation) const;
        [[nodiscard]] size_t ReverseEventCount() const;
        [[nodiscard]] size_t ReverseEventCapacityBytes() const { return eventCapacityBytes_; }
        [[nodiscard]] size_t ReverseScratchPeakBytes() const { return eventScratchPeakBytes_; }

    private:
        struct ThreadDefault_ {};
        explicit Tape_(ThreadDefault_) : Tape_() { threadDefault_ = true; }
        friend Tape_* Tape();
        struct ReverseEvents_;
        struct ReverseEventsDeleter_ {
            Tape_* tape_;
            void operator()(ReverseEvents_* events) const noexcept;
        };
        std::unique_ptr<ReverseEvents_, ReverseEventsDeleter_> reverseEvents_;
        bool reverseFailed_ = false;
        size_t eventCapacityBytes_ = 0;
        size_t eventScratchPeakBytes_ = 0;
        bool threadDefault_ = false; // GCC spills recording values when these constructor flags are adjacent.
        friend struct NativeRecordedOperation_;

        void UpdateThreadEventValidation() const noexcept {
            if (threadDefault_)
                NativeThread::eventValidationRequired = HasReverseEventState();
        }
        void RequireReverseEventMutation(const char* operation) const;
        void PrepareReverseEvent();
        void AppendReverseEvent(ReverseEventHandle_ event);
        void PropagateEventWindow(Iterator_ end, Iterator_ begin, bool fromMark, bool toMark, void (*propagate)(Tape_&, Iterator_, Iterator_));

#if defined(DAL_ENABLE_AAD_LIFETIME_DIAGNOSTICS)
        struct NodeBinding_ {
            std::uint64_t owner_ = 0;
            std::uint64_t epoch_ = 0;
            std::uint64_t ordinal_ = 0;
            std::uint64_t generation_ = 0;
            bool multi_ = false;
            size_t width_ = 0;
        };

        friend struct NativeLifetimeTestAccess_;
        static std::uint64_t ClaimLifetimeIdentity(std::atomic<std::uint64_t>* next);
        static std::uint64_t NewLifetimeIdentity();
        const std::uint64_t lifetimeIdentity_ = NewLifetimeIdentity();
        std::uint64_t lifetimeEpoch_ = 1;
        std::uint64_t lifetimeGeneration_ = 0;
        std::uint64_t liveNodes_ = 0;
        std::uint64_t markedNodes_ = 0;
        bool lifetimeFailed_ = false;
        bool graphMulti_ = false;
        size_t graphWidth_ = 1;

        [[noreturn]] void RejectLifetime(const char* operation, const char* constraint, const NodeBinding_* binding = nullptr) const;
        void RequireLiveGraph(const char* operation) const;
        void CheckNodeAllocation() const;
        void BeginLifetimeReset();
        void ValidateBinding(const NodeBinding_& binding, const TapNode_* node, const char* operation) const;
        void ValidateLiveSlot(const NodeBinding_& binding, const TapNode_* node, const char* operation) const;
        [[nodiscard]] NodeBinding_ CaptureBinding(const TapNode_* node) const {
            return {lifetimeIdentity_, lifetimeEpoch_, node->lifetimeOrdinal_, node->lifetimeGeneration_, multi_, numAdj_};
        }
#endif

        template <size_t N_> FORCE_INLINE TapNode_* AllocateNode() {
#if defined(DAL_ENABLE_AAD_LIFETIME_DIAGNOSTICS)
            CheckNodeAllocation();
            try {
#endif
                TapNode_* node = nodes_.EmplaceBack(N_);
                if (multi_) {
                    node->pAdjoints_ = adjointsMulti_.EmplaceBackMulti(numAdj_);
                    std::fill_n(node->pAdjoints_, numAdj_, 0.0);
                }

                if constexpr (static_cast<bool>(N_)) {
                    node->pDerivatives_ = ders_.EmplaceBackMulti<N_>();
                    node->pAdjPtrs_ = argPtrs_.EmplaceBackMulti<N_>();
                }
#if defined(DAL_ENABLE_AAD_LIFETIME_DIAGNOSTICS)
                if (liveNodes_ == 0) {
                    graphMulti_ = multi_;
                    graphWidth_ = numAdj_;
                }
                node->lifetimeOrdinal_ = liveNodes_++;
                node->lifetimeGeneration_ = ++lifetimeGeneration_;
#endif
                return node;
#if defined(DAL_ENABLE_AAD_LIFETIME_DIAGNOSTICS)
            } catch (...) {
                lifetimeFailed_ = true;
                throw;
            }
#endif
        }
    };

    // Keep three-input allocation out of model loops without duplicating the allocator.
    template <>
#if defined(_MSC_VER)
    __declspec(noinline)
#elif defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 9
    __attribute__((noipa))
#elif defined(__GNUC__) || defined(__clang__)
    __attribute__((noinline))
#endif
    inline TapNode_* Tape_::RecordNode<3>() {
        return AllocateNode<3>();
    }

} // namespace Dal::AAD

namespace Dal::AAD {
    void Clear(Tape_& tape);
    void Mark(Tape_& tape);
    void RewindToMark(Tape_& tape);
    void Rewind(Tape_& tape);
    void PropagateMarkToStart(Tape_& tape);
    void PropagateToStart(Tape_& tape);
    void PropagateToMark(Tape_& tape);
    void NewRecording(Tape_& tape);
    void Activate(Tape_& tape);
    void Deactivate(Tape_& tape);
} // namespace Dal::AAD

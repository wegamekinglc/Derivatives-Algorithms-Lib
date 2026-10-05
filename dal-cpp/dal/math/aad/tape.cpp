//
// Created by wegam on 2023/2/18.
//

#if defined(DAL_ENABLE_AAD_LIFETIME_DIAGNOSTICS)
#include <string>
#endif

#include <dal/math/aad/expr.hpp>
#include <dal/math/aad/tape.hpp>

namespace Dal::AAD {

#if defined(DAL_ENABLE_AAD_LIFETIME_DIAGNOSTICS)
    std::uint64_t Tape_::ClaimLifetimeIdentity(std::atomic<std::uint64_t>* nextIdentity) {
        auto identity = nextIdentity->load(std::memory_order_relaxed);
        while (identity != 0) {
            const auto next = identity == std::numeric_limits<std::uint64_t>::max() ? 0 : identity + 1;
            if (nextIdentity->compare_exchange_weak(identity, next, std::memory_order_relaxed))
                return identity;
        }
        THROW("Tape.LifetimeIdentity: identities exhausted");
    }

    std::uint64_t Tape_::NewLifetimeIdentity() {
        static std::atomic<std::uint64_t> nextIdentity{1};
        return ClaimLifetimeIdentity(&nextIdentity);
    }

    void Tape_::RejectLifetime(const char* operation, const char* constraint, const NodeBinding_* binding) const {
        std::string message =
            std::string(operation) + ": " + constraint + " [owner=" + std::to_string(lifetimeIdentity_) + ", epoch=" + std::to_string(lifetimeEpoch_);
        if (binding != nullptr)
            message += ", boundOwner=" + std::to_string(binding->owner_) + ", boundEpoch=" + std::to_string(binding->epoch_) +
                       ", slot=" + std::to_string(binding->ordinal_) + ", generation=" + std::to_string(binding->generation_);
        THROW(message + "]");
    }

    void Tape_::RequireLiveGraph(const char* operation) const {
        if (lifetimeFailed_)
            RejectLifetime(operation, "requires reset after incomplete allocation or reset");
        if (liveNodes_ != 0 && (graphMulti_ != multi_ || graphWidth_ != numAdj_))
            RejectLifetime(operation, "recorded mode or width does not match tape storage mode");
    }

    void Tape_::CheckNodeAllocation() const {
        RequireLiveGraph("Tape.RecordNode");
        if (liveNodes_ == std::numeric_limits<std::uint64_t>::max())
            RejectLifetime("Tape.RecordNode", "node count exhausted");
        if (lifetimeGeneration_ == std::numeric_limits<std::uint64_t>::max())
            RejectLifetime("Tape.RecordNode", "allocation generation exhausted");
    }

    void Tape_::BeginLifetimeReset() {
        if (lifetimeEpoch_ == std::numeric_limits<std::uint64_t>::max())
            RejectLifetime("Tape.Reset", "recording epoch exhausted");
        ++lifetimeEpoch_;
        liveNodes_ = 0;
        markedNodes_ = 0;
        lifetimeFailed_ = true;
    }

    void Tape_::ValidateBinding(const NodeBinding_& binding, const TapNode_* node, const char* operation) const {
        if (node == nullptr)
            RejectLifetime(operation, "number has no tape node", &binding);
        if (binding.owner_ != lifetimeIdentity_)
            RejectLifetime(operation, "number belongs to another tape lifetime", &binding);
        if (binding.epoch_ != lifetimeEpoch_)
            RejectLifetime(operation, "number belongs to a discarded recording epoch", &binding);
        RequireLiveGraph(operation);
        ValidateLiveSlot(binding, node, operation);
    }

    void Tape_::ValidateLiveSlot(const NodeBinding_& binding, const TapNode_* node, const char* operation) const {
        if (binding.ordinal_ >= liveNodes_)
            RejectLifetime(operation, "number slot was discarded by suffix restore", &binding);
        if (binding.multi_ != multi_ || binding.width_ != numAdj_)
            RejectLifetime(operation, "number mode or width does not match tape storage mode", &binding);
        if (node->lifetimeOrdinal_ != binding.ordinal_ || node->lifetimeGeneration_ != binding.generation_)
            RejectLifetime(operation, "number allocation generation was replaced", &binding);
    }
#endif

    void TapNode_::PropagateNonFiniteResults(double* destination, const double* source, double derivative, size_t numAdj) {
        PropagateResults<true>(destination, source, derivative, numAdj);
    }

    namespace {
        auto Begin(Tape_& tape) -> Tape_::Iterator_ { return tape.nodes_.Begin(); }

        auto End(Tape_& tape) -> Tape_::Iterator_ { return tape.nodes_.End(); }

        auto MarkIt(Tape_& tape) -> Tape_::Iterator_ { return tape.nodes_.Mark(); }

        template <bool M_, size_t R_ = 0>
#if defined(__GNUC__) || defined(__clang__)
        __attribute__((noinline))
#elif defined(_MSC_VER)
        __declspec(noinline)
#endif
        void PropagateAdjoints(Tape_& tape, Tape_::Iterator_ propagateFrom, Tape_::Iterator_ propagateTo) {
            const auto sweep = [propagateFrom, propagateTo](const auto& propagateNode) {
                auto it = propagateFrom;
                while (it != propagateTo) {
                    propagateNode(*it);
                    --it;
                }
                propagateNode(*it);
            };
            if constexpr (M_)
                sweep([numAdj = tape.numAdj_](TapNode_& node) { node.PropagateAll<R_>(numAdj); });
            else
                sweep([](TapNode_& node) { node.PropagateOne(); });
        }

        template <size_t R_, size_t... S_> void PropagateMulti(Tape_& tape, Tape_::Iterator_ propagateFrom, Tape_::Iterator_ propagateTo) {
            if (tape.numAdj_ == R_)
                PropagateAdjoints<true, R_>(tape, propagateFrom, propagateTo);
            else if constexpr (sizeof...(S_) != 0)
                PropagateMulti<S_...>(tape, propagateFrom, propagateTo);
            else
                PropagateAdjoints<true>(tape, propagateFrom, propagateTo);
        }

        // Select the mode and width once per sweep; keep the empty-window guard outside the hot loops.
        FORCE_INLINE void PropagateWindow(Tape_& tape, Tape_::Iterator_ propagateEnd, Tape_::Iterator_ propagateTo) {
            if (propagateEnd != propagateTo) {
                const auto propagateFrom = std::prev(propagateEnd);
                if (tape.multi_)
                    PropagateMulti<1, 2, 4, 8, 10, 16>(tape, propagateFrom, propagateTo);
                else
                    PropagateAdjoints<false>(tape, propagateFrom, propagateTo);
            }
        }

        template <class F_> void ForEachBlock(Tape_& tape, F_&& fn) {
            if (tape.multi_)
                fn(tape.adjointsMulti_);
            fn(tape.ders_);
            fn(tape.argPtrs_);
            fn(tape.nodes_);
        }

        // adjointsMulti_ is cleared unconditionally so a tape toggled from multi
        // to non-multi (SetNumResultsForAAD) does not retain stale adjoints.
        template <class F_> void ForEachBlockAll(Tape_& tape, F_&& fn) {
            fn(tape.adjointsMulti_);
            fn(tape.ders_);
            fn(tape.argPtrs_);
            fn(tape.nodes_);
        }
    } // namespace

    void PropagateMarkToStart(Tape_& tape) {
#if defined(DAL_ENABLE_AAD_LIFETIME_DIAGNOSTICS)
        tape.RequireLiveGraph("Tape.PropagateMarkToStart");
#endif
        PropagateWindow(tape, MarkIt(tape), Begin(tape));
    }

    void PropagateToStart(Tape_& tape) {
#if defined(DAL_ENABLE_AAD_LIFETIME_DIAGNOSTICS)
        tape.RequireLiveGraph("Tape.PropagateToStart");
#endif
        PropagateWindow(tape, End(tape), Begin(tape));
    }

    void PropagateToMark(Tape_& tape) {
#if defined(DAL_ENABLE_AAD_LIFETIME_DIAGNOSTICS)
        tape.RequireLiveGraph("Tape.PropagateToMark");
#endif
        PropagateWindow(tape, End(tape), MarkIt(tape));
    }

    void Clear(Tape_& tape) {
#if defined(DAL_ENABLE_AAD_LIFETIME_DIAGNOSTICS)
        tape.BeginLifetimeReset();
#endif
        ForEachBlockAll(tape, [](auto& block) { block.Clear(); });
#if defined(DAL_ENABLE_AAD_LIFETIME_DIAGNOSTICS)
        tape.lifetimeFailed_ = false;
#endif
    }

    void Mark(Tape_& tape) {
#if defined(DAL_ENABLE_AAD_LIFETIME_DIAGNOSTICS)
        tape.RequireLiveGraph("Tape.Mark");
#endif
        ForEachBlock(tape, [](auto& block) { block.SetMark(); });
#if defined(DAL_ENABLE_AAD_LIFETIME_DIAGNOSTICS)
        tape.markedNodes_ = tape.liveNodes_;
#endif
    }

    void Rewind(Tape_& tape) {
#if defined(DAL_ENABLE_AAD_LIFETIME_DIAGNOSTICS)
        tape.BeginLifetimeReset();
        ForEachBlockAll(tape, [](auto& block) {
            block.Rewind();
            block.SetMark();
        });
        tape.lifetimeFailed_ = false;
#else
        ForEachBlock(tape, [](auto& block) { block.Rewind(); });
#endif
    }

    void RewindToMark(Tape_& tape) {
#if defined(DAL_ENABLE_AAD_LIFETIME_DIAGNOSTICS)
        tape.RequireLiveGraph("Tape.RewindToMark");
        tape.liveNodes_ = tape.markedNodes_;
#endif
        ForEachBlock(tape, [](auto& block) { block.RewindToMark(); });
    }

    void NewRecording(Tape_&) {}
    void Activate(Tape_&) {}
    void Deactivate(Tape_&) {}
} // namespace Dal::AAD

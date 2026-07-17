//
// Created by wegam on 2023/2/18.
//

#include <dal/math/aad/expr.hpp>
#include <dal/math/aad/tape.hpp>

#if !defined(DAL_USE_XAD_AAD) && !defined(DAL_USE_CODIPACK_AAD) && !defined(DAL_USE_ADEPT_AAD)
namespace Dal::AAD {

    namespace {
        auto Begin(Tape_& tape) -> Tape_::Iterator_ {
            return tape.nodes_.Begin();
        }

        auto End(Tape_& tape) -> Tape_::Iterator_ {
            return tape.nodes_.End();
        }

        auto MarkIt(Tape_& tape) -> Tape_::Iterator_ {
            return tape.nodes_.Mark();
        }

        void PropagateAdjoints(Tape_::Iterator_ propagateFrom, Tape_::Iterator_ propagateTo) {
            auto it = propagateFrom;
            while (it != propagateTo) {
                it->PropagateOne();
                --it;
            }
            it->PropagateOne();
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
        PropagateAdjoints(std::prev(MarkIt(tape)), Begin(tape));
    }

    void PropagateToStart(Tape_& tape) {
        PropagateAdjoints(std::prev(End(tape)), Begin(tape));
    }

    void PropagateToMark(Tape_& tape) {
        PropagateAdjoints(std::prev(End(tape)), MarkIt(tape));
    }

    void Clear(Tape_& tape) {
        ForEachBlockAll(tape, [](auto& block) { block.Clear(); });
    }

    void Mark(Tape_& tape) {
        ForEachBlock(tape, [](auto& block) { block.SetMark(); });
    }

    void Rewind(Tape_& tape) {
        ForEachBlock(tape, [](auto& block) { block.Rewind(); });
    }

    void RewindToMark(Tape_& tape) {
        ForEachBlock(tape, [](auto& block) { block.RewindToMark(); });
    }

    void NewRecording(Tape_&) {}
    void Activate(Tape_&) {}
    void Deactivate(Tape_&) {}
} // namespace Dal::AAD
#elif defined(DAL_USE_ADEPT_AAD)

namespace Dal::AAD {

    void Clear(Tape_& tape) {
        tape.new_recording();
        tape.start_ = tape.Position();
        tape.mark_ = tape.start_;
    }

    void Mark(Tape_& tape) {
        tape.mark_ = tape.Position();
    }

    void Rewind(Tape_& tape) {
        tape.reset_to(tape.start_.statements_, tape.start_.operations_);
    }

    void RewindToMark(Tape_& tape) {
        tape.reset_to(tape.mark_.statements_, tape.mark_.operations_);
    }

    void PropagateMarkToStart(Tape_& tape) {
        tape.compute_adjoint(tape.mark_.statements_, tape.start_.statements_);
    }

    void PropagateToStart(Tape_& tape) {
        tape.compute_adjoint(tape.Position().statements_, tape.start_.statements_);
    }

    void PropagateToMark(Tape_& tape) {
        tape.compute_adjoint(tape.Position().statements_, tape.mark_.statements_);
    }

    void NewRecording(Tape_& tape) {
        tape.new_recording();
        tape.start_ = tape.Position();
        tape.mark_ = tape.start_;
    }

    void Activate(Tape_& tape) {
        if (!tape.is_active())
            tape.activate();
    }

    void Deactivate(Tape_& tape) {
        tape.deactivate();
    }
} // namespace Dal::AAD
#elif defined(DAL_USE_XAD_AAD)

namespace Dal::AAD {

    void Clear(Tape_& tape) {
        tape.tape_.clearAll();
        tape.start_ = tape.tape_.getPosition();
        tape.mark_ = tape.start_;
    }

    void Mark(Tape_& tape) {
        tape.mark_ = tape.tape_.getPosition();
    }

    void Rewind(Tape_& tape) {
        tape.tape_.resetTo(tape.start_);
    }

    void RewindToMark(Tape_& tape) {
        tape.tape_.resetTo(tape.mark_);
    }

    void PropagateMarkToStart(Tape_& tape) {
        RewindToMark(tape);
        tape.tape_.computeAdjointsTo(tape.start_);
    }

    void PropagateToStart(Tape_& tape) {
        tape.tape_.computeAdjointsTo(tape.start_);
    }

    void PropagateToMark(Tape_& tape) {
        tape.tape_.computeAdjointsTo(tape.mark_);
    }

    void NewRecording(Tape_& tape) {
        tape.tape_.newRecording();
        tape.start_ = tape.tape_.getPosition();
        tape.mark_ = tape.start_;
    }

    void Activate(Tape_& tape) {
        if (!tape.tape_.isActive())
            tape.tape_.activate();
    }

    void Deactivate(Tape_& tape) {
        tape.tape_.deactivate();
    }
} // namespace Dal::AAD
#elif defined(DAL_USE_CODIPACK_AAD)
namespace Dal::AAD {

    void Clear(Tape_& tape) {
        tape.tape_.reset();
        tape.tape_.setActive();
        tape.start_ = tape.tape_.getPosition();
        tape.mark_ = tape.start_;
    }

    void Mark(Tape_& tape) {
        tape.mark_ = tape.tape_.getPosition();
    }

    void Rewind(Tape_& tape) {
        tape.tape_.resetTo(tape.start_);
    }

    void RewindToMark(Tape_& tape) {
        tape.tape_.resetTo(tape.mark_);
    }

    void PropagateMarkToStart(Tape_& tape) {
        tape.tape_.evaluate(tape.mark_, tape.start_);
    }

    void PropagateToStart(Tape_& tape) {
        tape.tape_.evaluate(tape.tape_.getPosition(), tape.start_);
    }

    void PropagateToMark(Tape_& tape) {
        tape.tape_.evaluate(tape.tape_.getPosition(), tape.mark_);
    }

    void NewRecording(Tape_& tape) {
        tape.start_ = tape.tape_.getPosition();
        tape.mark_ = tape.start_;
    }

    void Activate(Tape_& tape) {
        tape.tape_.setActive();
    }

    void Deactivate(Tape_& tape) {
        tape.tape_.setPassive();
    }
} // namespace Dal::AAD
#endif

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
    }

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
        tape.adjointsMulti_.Clear();
        tape.ders_.Clear();
        tape.argPtrs_.Clear();
        tape.nodes_.Clear();
    }

    void Mark(Tape_& tape) {
        if (Tape_::multi_)
            tape.adjointsMulti_.SetMark();
        tape.ders_.SetMark();
        tape.argPtrs_.SetMark();
        tape.nodes_.SetMark();
    }

    void Rewind(Tape_& tape) {
        if (Tape_::multi_)
            tape.adjointsMulti_.Rewind();
        tape.ders_.Rewind();
        tape.argPtrs_.Rewind();
        tape.nodes_.Rewind();
    }

    void RewindToMark(Tape_& tape) {
        if (Tape_::multi_)
            tape.adjointsMulti_.RewindToMark();
        tape.ders_.RewindToMark();
        tape.argPtrs_.RewindToMark();
        tape.nodes_.RewindToMark();
    }

    void NewRecording(Tape_&) {}
    void Activate(Tape_&) {}
    void Deactivate(Tape_&) {}
} // namespace Dal::AAD
#elif defined(DAL_USE_ADEPT_AAD)

namespace Dal::AAD {

    void Clear(Tape_& tape) {
        tape.tape_.new_recording();
        tape.start_ = tape.tape_.Position();
        tape.mark_ = tape.start_;
    }

    void Mark(Tape_& tape) {
        tape.mark_ = tape.tape_.Position();
    }

    void Rewind(Tape_& tape) {
        tape.tape_.ResetTo(tape.start_);
    }

    void RewindToMark(Tape_& tape) {
        tape.tape_.ResetTo(tape.mark_);
    }

    void PropagateMarkToStart(Tape_& tape) {
        tape.tape_.ReverseRange(tape.mark_, tape.start_);
    }

    void PropagateToStart(Tape_& tape) {
        tape.tape_.ReverseRange(tape.tape_.Position(), tape.start_);
    }

    void PropagateToMark(Tape_& tape) {
        tape.tape_.ReverseRange(tape.tape_.Position(), tape.mark_);
    }

    void NewRecording(Tape_& tape) {
        tape.tape_.new_recording();
        tape.start_ = tape.tape_.Position();
        tape.mark_ = tape.start_;
    }

    void Activate(Tape_& tape) {
        if (!tape.tape_.is_active())
            tape.tape_.activate();
    }

    void Deactivate(Tape_& tape) {
        tape.tape_.deactivate();
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

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
#include <dal/math/aad/expr.hpp>

namespace Dal::AAD {

    struct NumResultsResetterForAAD_ {
        ~NumResultsResetterForAAD_() {
            Tape_::multi_ = false;
            TapNode_::numAdj_ = 1;
        }
    };

    FORCE_INLINE auto SetNumResultsForAAD(bool multi = false, size_t num_results = 1) {
        Tape_::multi_ = multi;
        TapNode_::numAdj_ = num_results;
        return std::make_unique<NumResultsResetterForAAD_>();
    }

    template <class IT_> FORCE_INLINE void PutOnTape(IT_ begin, IT_ end) {
        std::for_each(begin, end, [](Number_& n) { n.PutOnTape(); });
    }

    using Position_ = typename Tape_::Position_;

    FORCE_INLINE void SetGradient(Number_& res, double gradient) {
        res.setGradient(gradient);
    }

    FORCE_INLINE double GetGradient(const Number_& res) {
        return res.getGradient();
    }

    FORCE_INLINE void Evaluate(Tape_* tape) {
        Tape_::evaluate(tape->getPosition(), tape->getZeroPosition());
    }

    FORCE_INLINE void Evaluate(Tape_* tape, const Position_& pos) {
        Tape_::evaluate(tape->getPosition(), pos);
    }

    FORCE_INLINE void ResetToPos(Tape_* tape, const Position_& pos) {
        tape->resetTo(pos, false);
    }

    FORCE_INLINE void Reset(Tape_* tape) {
        tape->reset();
    }

    FORCE_INLINE void Clear(Tape_* tape) {
        return tape->Clear();
    }

    FORCE_INLINE void SetActive(Tape_* tape) {
        tape->setActive();
    }

    FORCE_INLINE Position_ GetPosition(Tape_& tape) {
        return tape.getPosition();
    }

} // namespace Dal

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
#include <dal/math/aad/expr.hpp>

namespace Dal::AAD {

    void RequireRecordingModeChangeAllowed();

    struct NumResultsResetterForAAD_ {
        Tape_* tape_;
        bool oldMulti_;
        size_t oldNumAdj_;
        NumResultsResetterForAAD_(Tape_* tape, bool oldMulti, size_t oldNumAdj) : tape_(tape), oldMulti_(oldMulti), oldNumAdj_(oldNumAdj) {}
        ~NumResultsResetterForAAD_() {
            tape_->multi_ = oldMulti_;
            tape_->numAdj_ = oldNumAdj_;
        }
    };

    // Contract: one mode per sweep — all nodes on a tape must be recorded under a single multi_ setting between Clear()s.
    FORCE_INLINE auto SetNumResultsForAAD(bool multi = false, size_t numResults = 1) {
        REQUIRE(numResults > 0 && numResults <= ADJ_SIZE, "SetNumResultsForAAD: numResults out of range");
        RequireRecordingModeChangeAllowed();
        Tape_* tape = Tape();
        bool oldMulti = tape->multi_;
        size_t oldNumAdj = tape->numAdj_;
        tape->multi_ = multi;
        tape->numAdj_ = numResults;
        return std::make_unique<NumResultsResetterForAAD_>(tape, oldMulti, oldNumAdj);
    }

    template <class IT_> FORCE_INLINE void PutOnTape(IT_ begin, IT_ end) {
        std::for_each(begin, end, [](Number_& n) { PutOnTape(n); });
    }

    FORCE_INLINE void Clear(Tape_* tape) { return Clear(*tape); }

    FORCE_INLINE void RegisterIndependent(Number_& n, double v) { n = v; }

    FORCE_INLINE void ZeroAdjoints(Tape_& tape) {
#if defined(DAL_ENABLE_AAD_LIFETIME_DIAGNOSTICS)
        tape.RequireLiveGraph("Tape.ZeroAdjoints");
#endif
        const auto zeroNodes = [&tape](const auto& zero) {
            size_t remaining = tape.nodes_.OccupiedSlots();
            auto it = tape.nodes_.Begin();
            while (remaining != 0) {
                zero(*it);
                if (--remaining != 0)
                    ++it;
            }
        };
        if (tape.multi_)
            zeroNodes([width = tape.numAdj_](TapNode_& node) {
                node.Adjoint() = 0.0;
                std::fill_n(&node.Adjoint(0), width, 0.0);
            });
        else
            zeroNodes([](TapNode_& node) { node.Adjoint() = 0.0; });
    }

} // namespace Dal::AAD

namespace Dal::AAD {
    inline Number_ PayoffRoot(const Number_& payoff, const Number_& activeZero) {
        auto end = Tape()->nodes_.End();
        // Only a terminal node recorded after the mark is already a path-local root.
        if (end != Tape()->nodes_.Mark() && &Adjoint(payoff) == &std::prev(end)->Adjoint())
            return payoff;
        return payoff + activeZero;
    }
} // namespace Dal::AAD

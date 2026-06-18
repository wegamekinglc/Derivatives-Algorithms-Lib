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

#if !defined(DAL_USE_XAD_AAD) && !defined(DAL_USE_CODIPACK_AAD) && !defined(DAL_USE_ADEPT_AAD)

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
        std::for_each(begin, end, [](Number_& n) { PutOnTape(n); });
    }

    FORCE_INLINE void Clear(Tape_* tape) {
        return Clear(*tape);
    }

    // Register n as a tape-tracked independent holding value v. Native: Number_::operator=(double)
    // (expr.hpp) records a fresh node -- that IS the registration. Mirrors the curve calibration
    // independent-registration step behind a backend-neutral name.
    FORCE_INLINE void RegisterIndependent(Number_& n, double v) { n = v; }

    // Zero every node's adjoint, leaving the recorded graph intact. Used between single-result
    // reverse sweeps so each row's seed propagates from a clean slate.
    FORCE_INLINE void ZeroAdjoints(Tape_& tape) {
        for (auto it = tape.nodes_.Begin(); it != tape.nodes_.End(); ++it)
            it->Adjoint() = 0.0;
    }

} // namespace Dal::AAD
#elif defined(DAL_USE_ADEPT_AAD)

#include <algorithm>

namespace Dal::AAD {

    template <class IT_> FORCE_INLINE void PutOnTape(IT_ begin, IT_ end) {
        std::for_each(begin, end, [](Number_& n) { PutOnTape(n); });
    }

    FORCE_INLINE void Clear(Tape_* tape) {
        return Clear(*tape);
    }

    // Register n as an independent holding value v. Adept records the assignment as a statement on
    // the active stack; no separate registerInput call is needed.
    FORCE_INLINE void RegisterIndependent(Number_& n, double v) { n = v; }

    // Zero every gradient between single-result sweeps. NOT a no-op on Adept: the compute_adjoint
    // override (tape.hpp) zeroes only each consumed statement's LHS, then accumulates into operands
    // whose gradients are never cleared -- row 2's seed would land on row 1's residue and corrupt
    // the Jacobian. ZeroGradientArray clears the live gradient array while keeping
    // gradients_initialized_ true so the override's THROW guard stays satisfied.
    FORCE_INLINE void ZeroAdjoints(Tape_& tape) { tape.ZeroGradientArray(); }

} // namespace Dal::AAD
#elif defined(DAL_USE_XAD_AAD)

#include <algorithm>
#include <dal/utilities/exceptions.hpp>

namespace Dal::AAD {

    template <class IT_> FORCE_INLINE void PutOnTape(IT_ begin, IT_ end) {
        std::for_each(begin, end, [](Number_& n) { PutOnTape(n); });
    }

    FORCE_INLINE void Clear(Tape_* tape) {
        return Clear(*tape);
    }

    // Register n as an independent holding value v. XAD requires registerInput to run BEFORE the
    // recording window opens (NewRecording): registering an input AFTER NewRecording silently drops
    // it and yields an all-zero Jacobian column, so callers must RegisterIndependent first, then
    // NewRecording, then the forward pass. The facade asserts the tape is active (clearAll does not
    // deactivate a tape constructed with activate=true), so a passive tape fails loud here.
    FORCE_INLINE void RegisterIndependent(Number_& n, double v) {
        auto* t = Tape();
        REQUIRE(t->tape_.isActive(), "Dal::AAD::RegisterIndependent: XAD tape is not active");
        t->tape_.registerInput(n);
        xad::value(n) = v;
    }

    FORCE_INLINE void ZeroAdjoints(Tape_& tape) { tape.tape_.clearDerivatives(); }

} // namespace Dal::AAD
#elif defined(DAL_USE_CODIPACK_AAD)

#include <algorithm>

namespace Dal::AAD {

    template <class IT_> FORCE_INLINE void PutOnTape(IT_ begin, IT_ end) {
        std::for_each(begin, end, [](Number_& n) { PutOnTape(n); });
    }

    FORCE_INLINE void Clear(Tape_* tape) {
        return Clear(*tape);
    }

    // Register n as an independent holding value v on the active CoDiPack tape.
    FORCE_INLINE void RegisterIndependent(Number_& n, double v) {
        Tape()->tape_.registerInput(n);
        n.setValue(v);
    }

    // Zero every adjoint (no-arg clearAdjoints zeroes up to the largest created index), graph intact.
    FORCE_INLINE void ZeroAdjoints(Tape_& tape) { tape.tape_.clearAdjoints(); }

} // namespace Dal::AAD
#endif

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

    // Per-backend recording + gradient-zeroing contract: see docs/methodology/aad.md §Backends.
    FORCE_INLINE void RegisterIndependent(Number_& n, double v) { n = v; }

    // Per-backend recording + gradient-zeroing contract: see docs/methodology/aad.md §Backends.
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

    // Per-backend recording + gradient-zeroing contract: see docs/methodology/aad.md §Backends.
    FORCE_INLINE void RegisterIndependent(Number_& n, double v) { n = v; }

    // Per-backend recording + gradient-zeroing contract: see docs/methodology/aad.md §Backends.
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

    // Per-backend recording + gradient-zeroing contract: see docs/methodology/aad.md §Backends.
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

    // Per-backend recording + gradient-zeroing contract: see docs/methodology/aad.md §Backends.
    FORCE_INLINE void RegisterIndependent(Number_& n, double v) {
        Tape()->tape_.registerInput(n);
        n.setValue(v);
    }

    FORCE_INLINE void ZeroAdjoints(Tape_& tape) { tape.tape_.clearAdjoints(); }

} // namespace Dal::AAD
#endif

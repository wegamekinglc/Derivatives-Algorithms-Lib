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

#include <dal/platform/platform.hpp>

#ifdef USE_CODI
#include <codi.hpp>

namespace Dal::AAD {
#ifndef NDEBUG
    using Number_ = codi::RealReverse;
#else
    using Number_ = codi::RealReverseUnchecked;
#endif
    using Tape_ = typename Number_::Tape;
    using Position_ = typename Tape_::Position;
}
#endif

#ifdef USE_AADET
#include <dal/math/aad/expr.hpp>

namespace Dal::AAD {

    struct NumResultsResetterForAAD_ {
        ~NumResultsResetterForAAD_() {
            Tape_::multi_ = false;
            TapNode_::num_adj_ = 1;
        }
    };

    inline auto SetNumResultsForAAD(bool multi = false, const size_t& num_results = 1) {
        Tape_::multi_ = multi;
        TapNode_::num_adj_ = num_results;
        return std::make_unique<NumResultsResetterForAAD_>();
    }

    template <class IT_> inline void PutOnTape(IT_ begin, IT_ end) {
        std::for_each(begin, end, [](Number_& n) { n.PutOnTape(); });
    }

    using Position_ = typename Tape_::Position_;

} // namespace Dal
#endif
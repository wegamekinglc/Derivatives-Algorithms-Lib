//
// Created by dal-implementer on 2026/6/14.
//

#pragma once

#include <dal/math/interp/interp.hpp>
#include <dal/math/interp/interpcubic.hpp>

namespace Dal {
    struct MixedSchemeSpec_ {
        double cutoffYf_;
        Interp::Boundary_ cubicLhs_ = Interp::Boundary_(2, 0.0);
        Interp::Boundary_ cubicRhs_ = Interp::Boundary_(2, 0.0);
    };

    std::unique_ptr<Interp1_> NewMixedLogDF(const String_& name,
                            const Vector_<>& yf,
                            const Vector_<>& logDF,
                            const MixedSchemeSpec_& spec);
} // namespace Dal

//
// Created by dal-implementer on 2026/6/20.
//

#pragma once

#include <dal/curve/jointycctx.hpp>
#include <dal/curve/ycinstrument.hpp>

namespace Dal {
    namespace Tape {
        // Phase B templated joint rate base. SIBLING of Tape::Rate_<T_> (which is bound to
        // YCCtx_<T_> and reads a single curve). The joint analogue: operator() takes a
        // JointCurveBlock_<T_> routing context (Gap 1) and performs BOTH the discount read at the
        // leg's collateral AND the forecast read at (forecastTenor_, collateral_) in the T_ domain
        // (Gap 3). Phase A's Tape::Rate_<T_>, YCCtx_<T_>, and the four Phase A rate subclasses are
        // UNTOUCHED (NG2).
        //
        // The projection-capable subclasses (DepositRateProj_<T_>, ForwardRateProj_<T_> covering
        // FRA + Future, SwapRateProj_<T_>) live alongside the Phase A templated rates in
        // ycinstrument.cpp (so they can reuse the file-local BuildLegPeriods helper). They are
        // constructed by the Tape::ProjectionRateAt<T_>(inst) factory, dispatched via dynamic_cast.
        template <class T_>
        struct JointRate_ {
            virtual ~JointRate_() = default;
            virtual T_ operator()(const JointCurveBlock_<T_>& block) const = 0;
        };

        // Build the projection-capable templated rate for an instrument, or an empty handle if the
        // instrument type has no projection-rate subclass. Defined in ycinstrument.cpp alongside
        // the Phase A PrecomputeT factories (reuses the file-local BuildLegPeriods helper).
        template <class T_>
        Handle_<JointRate_<T_>> ProjectionRateAt(const YCInstrument_& inst);
    } // namespace Tape
} // namespace Dal

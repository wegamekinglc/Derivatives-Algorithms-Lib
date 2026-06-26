//
// Created by dal-implementer on 2026/6/20.
//

#pragma once

#include <dal/curve/jointycctx.hpp>
#include <dal/curve/ycinstrument.hpp>

namespace Dal {
    namespace Tape {
        template <class T_>
        struct JointRate_ {
            virtual ~JointRate_() = default;
            virtual T_ operator()(const JointCurveBlock_<T_>& block) const = 0;
        };

        template <class T_>
        Handle_<JointRate_<T_>> ProjectionRateAt(const YCInstrument_& inst);
    } // namespace Tape
} // namespace Dal

//
// Created by wegam on 2023/3/26.
//

#pragma once

#include <dal/curve/yccomponent.hpp>
#include <dal/currency/currency.hpp>

namespace Dal {
    class Date_;
    class YCComponent_;

    // Phase A templatization: DiscountCurve_ is now an alias of Tape::DiscountCurve_<double>. The
    // template exists so the calibration AAD-tape path can build a parallel Tape::DiscountCurve_<Number_>
    // whose operator() reads through the native tape. The double specialization stays the hot path
    // (the F loop, the bumped fallback) and its virtual signature is identical to the pre-Phase-A
    // DiscountCurve_::operator()(Date_, Date_). Existing consumers (dynamic_cast<DiscountLogDF_*>,
    // Handle_<DiscountCurve_>, CurveWithBase_<DiscountCurve_>) keep working unchanged because the
    // alias IS the type. The number-type-templated primitives live in namespace Tape (the AAD-tape
    // Jacobian layer), instantiated as <double> for the canonical curves and <Number_> for AAD.
    namespace Tape {
        template <class T_>
        class DiscountCurve_ : public YCComponent_ {
        public:
            const Ccy_ ccy_;
            explicit DiscountCurve_(const String_& name, const String_& ccy) : YCComponent_("DiscountCurve", name), ccy_(ccy) {}
            virtual T_ operator()(const Date_& from, const Date_& to) const = 0;
        };
    } // namespace Tape

    using DiscountCurve_ = Tape::DiscountCurve_<double>;
} // namespace Dal

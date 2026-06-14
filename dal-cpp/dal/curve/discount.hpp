//
// Created by wegam on 2023/3/26.
//

#pragma once

#include <dal/curve/yccomponent.hpp>
#include <dal/currency/currency.hpp>

namespace Dal {
    class Date_;
    class YCComponent_;

    // Phase A templatization: DiscountCurve_ is now an alias of DiscountCurveT_<double>. The
    // template exists so the calibration AAD-tape path can build a parallel DiscountCurveT_<Number_>
    // whose operator() reads through the native tape. The double specialization stays the hot path
    // (the F loop, the bumped fallback) and its virtual signature is identical to the pre-Phase-A
    // DiscountCurve_::operator()(Date_, Date_). Existing consumers (dynamic_cast<DiscountLogDF_*>,
    // Handle_<DiscountCurve_>, CurveWithBase_<DiscountCurve_>) keep working unchanged because the
    // alias IS the type.
    template <class T_>
    class DiscountCurveT_ : public YCComponent_ {
    public:
        const Ccy_ ccy_;
        explicit DiscountCurveT_(const String_& name, const String_& ccy) : YCComponent_("DiscountCurve", name), ccy_(ccy) {}
        virtual T_ operator()(const Date_& from, const Date_& to) const = 0;
    };

    using DiscountCurve_ = DiscountCurveT_<double>;
} // namespace Dal

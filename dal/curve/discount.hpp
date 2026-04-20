//
// Created by wegam on 2023/3/26.
//

#pragma once

#include <dal/curve/yccomponent.hpp>
#include <dal/currency/currency.hpp>

namespace Dal {
    class Date_;
    class YCComponent_;

    class DiscountCurve_: public YCComponent_ {
    public:
        const Ccy_ ccy_;
        explicit DiscountCurve_(const String_& name, const String_ &ccy);
        virtual double operator()(const Date_& from, const Date_& to) const = 0;
    };
} // namespace Dal


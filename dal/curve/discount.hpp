//
// Created by wegam on 2023/3/26.
//

#pragma once

#include <dal/time/date.hpp>
#include <dal/curve/yccomponent.hpp>

namespace Dal {
    class DiscountCurve_: public YCComponent_ {
    public:
        explicit DiscountCurve_(const String_& name);
        virtual double operator()(const Date_& from, const Date_& to) const = 0;
    };
}

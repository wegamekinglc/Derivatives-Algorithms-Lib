//
// Created by wegam on 2022/10/2.
//

#pragma once
#include <dal/math/vectors.hpp>

namespace Dal {

    namespace Date {
        class Increment_;
    }

    class Holidays_;
    struct Cell_;
    class Date_;
    using Schedule_ = Vector_<Date_>;

    Schedule_ MakeSchedule(const Date_& start,
                           const Cell_& maturity,
                           const Holidays_& hols,
                           const Handle_<Date::Increment_>& tenor);
}

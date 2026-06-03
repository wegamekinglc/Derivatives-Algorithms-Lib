//
// Created by wegam on 2024/8/11.
//

#pragma once

#include <dal/time/date.hpp>
#include <dal/math/vectors.hpp>

namespace Dal {
    Vector_<std::tuple<Date_, Date_, Date_>> ParseSchedule(const Vector_<String_>& tokens);
}

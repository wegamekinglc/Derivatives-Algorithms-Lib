//
// Created by Claude on 2026/6/16.
//

#pragma once

#include <dal/math/vectors.hpp>
#include <dal/time/date.hpp>

namespace Dal::Target {
    // ECB TARGET2 closing days for [startYear, endYear]. Weekends are excluded from the
    // list (the calendar engine treats Sat/Sun as non-business days separately).
    // Closed days: 1 Jan, Good Friday, Easter Monday, 1 May, 25 Dec, 26 Dec.
    Vector_<Date_> Holidays(int startYear, int endYear);
} // namespace Dal::Target

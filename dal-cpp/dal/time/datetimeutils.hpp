//
// Created by wegam on 2020/10/25.
//

#pragma once

namespace Dal {
    class String_;
    class DateTime_;

    namespace DateTime {
        bool IsDateTimeString(const String_& src);
        DateTime_ FromString(const String_& src); // date, space, colon-separated numbers
    }                                             // namespace DateTime
} // namespace Dal
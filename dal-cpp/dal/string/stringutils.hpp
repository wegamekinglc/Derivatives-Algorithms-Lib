//
// Created by wegamekinglc on 2020/5/2.
//

#pragma once

#include <dal/string/strings.hpp>

namespace Dal {
    namespace String {
        bool ToBool(const String_& src);
        Vector_<bool> ToBoolVector(const String_& src);
    } // namespace String
} // namespace Dal

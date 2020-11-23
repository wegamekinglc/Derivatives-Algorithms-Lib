//
// Created by wegam on 2020/10/25.
//

#pragma once

#include <boost/optional.hpp>

// syntactic sugar for null default
template<class T_>
const T_& operator+(const boost::optional<T_>& opt) {
    return opt.get_value_or(T_());
}


// construct from a pointer
template<class T_>
boost::optional<T_> as_optional(const T_* p) {
    boost::optional<T_> ret_val;
    if (p)
        ret_val = *p; return ret_val;
}
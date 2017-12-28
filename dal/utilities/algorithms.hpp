//
// Created by Cheng Li on 17-12-19.
//

#pragma once

#include <algorithm>
#include <dal/platform/platform.hpp>
#include <dal/utilities/asserts.hpp>

#define ASSIGN(p, v)                                                                                                   \
    if (!(p))                                                                                                          \
        ;                                                                                                              \
    else                                                                                                               \
        *(p) = (v)


template<class CS_, class OP_, class CD_> void Transform(const CS_& src, OP_ op, CD_* dst)
{
    DAL_ASSERT(dst && src.size() == dst->size(), "src size is not compatible with dst size");
    std::transform(src.begin(), src.end(), dst->begin(), op);
}


template <typename C, typename OP> auto Apply(OP op, const C& src)->Vector_<decltype(op(*src.begin()))>
{
    typedef Vector_<decltype(op(*src.begin()))> vector_t;
    vector_t ret_val(src.size());
    Transform(src, op, &ret_val);
    return ret_val;
}

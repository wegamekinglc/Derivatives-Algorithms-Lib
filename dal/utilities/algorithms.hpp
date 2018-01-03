//
// Created by Cheng Li on 17-12-19.
//

#pragma once

#include <algorithm>
#include <type_traits>
#include <dal/platform/platform.hpp>
#include <dal/utilities/asserts.hpp>

#define ASSIGN(p, v)                                                                                                   \
    if (!(p))                                                                                                          \
        ;                                                                                                              \
    else                                                                                                               \
        *(p) = (v)


template<class CS_, class OP_, class CD_> void Transform(const CS_& src, OP_ op, CD_* dst)
{
    DAL_ASSERT(dst && src.size() == dst->size(), "dst is null or src size is not compatible with dst size");
    std::transform(src.begin(), src.end(), dst->begin(), op);
}


template <typename C, typename OP> auto Apply(OP op, const C& src)->Vector_<decltype(op(*src.begin()))>
{
    using vector_t = Vector_<std::remove_reference_t<std::remove_const_t<decltype(op(*src.begin()))>>>;
    vector_t ret_val(src.size());
    Transform(src, op, &ret_val);
    return ret_val;
}


template <class CS_, class CD_>  void Copy(const CS_& src, CD_* dst)
{
    DAL_ASSERT(dst && src.size() == dst->size(), "dst is null or src size is not compatible with dst size");
    std::copy(src.begin(), src.end(), dst->begin());
};


template <class C_> Vector_<typename C_::value_type> Copy(const C_& src)
{
    using vector_t = Vector_<typename C_::value_type>;
    vector_t  ret_val(src.size());
    std::copy(src.begin(), src.end(), ret_val.begin());
    return ret_val;
}
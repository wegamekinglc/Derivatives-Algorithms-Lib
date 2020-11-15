//
// Created by wegam on 2020/11/16.
//

#pragma once

#include <dal/platform/platform.hpp>

namespace Dal {

    int AsInt(double src);
    int AsInt(std::ptrdiff_t src);
    int NearestInt(double src);

    template <class C_, class OP_>
    auto Accumulate2(const C_& src, const OP_& op) {
        using val_type = decltype(op(0, *src.begin()));
        return std::accumulate(src.begin(), src.end(), val_type(0), op);
    }
    template <class C_>
    typename C_::value_type Accumulate(const C_& src) {
        return Accumulate2(src, std::plus<typename C_::value_type>());
    }

    template <class C1_, class C2_>
    typename C1_::value_type InnerProduct(const C1_& src1, const C2_& src2) {
        using value_type = typename C1_::value_type;
        return std::inner_product(src1.begin(), src1.end(), src2.begin(), value_type());
    }

    namespace Vector {
        Vector_<> L1Normalized(const Vector_<>& base);
        Vector_<> L2Normalized(const Vector_<>& base);
    }
}
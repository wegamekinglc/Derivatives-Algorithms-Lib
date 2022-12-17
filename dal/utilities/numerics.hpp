//
// Created by wegam on 2020/11/16.
//

#pragma once

#include <dal/math/operators.hpp>
#include <dal/platform/platform.hpp>
#include <dal/utilities/algorithms.hpp>
#include <numeric>

namespace Dal {

    int AsInt(double src);
    int AsInt(std::ptrdiff_t src);
    int NearestInt(double src);

    template <class C_, class OP_> auto Accumulate(const C_& src, const OP_& op) {
        using val_type = decltype(op(0.0, *src.begin()));
        return std::accumulate(src.begin(), src.end(), val_type(0), op);
    }

    template <class C_> auto Accumulate(const C_& src) { return Accumulate(src, Plus); }

    template <class C1_, class C2_> auto InnerProduct(const C1_& src1, const C2_& src2) {
        using value_type = typename C1_::value_type;
        return std::inner_product(src1.begin(), src1.end(), src2.begin(), value_type());
    }

    namespace Vector {
        template <class T_> Vector_<T_> L1Normalized(const Vector_<T_>& base) {
            using val_type = typename Vector_<T_>::value_type;
            auto func = [](val_type x, val_type y) { return x + Fabs(y); };
            auto l1 = Accumulate(base, func);
            auto func2 = [&l1](val_type x) { return x / (l1 + 1e-14); };
            return Apply(func2, base);
        }

        template <class T_> Vector_<T_> L2Normalized(const Vector_<T_>& base) {
            using val_type = typename Vector_<T_>::value_type;
            auto func = [](val_type x, val_type y) { return x + y * y; };
            auto l2 = Sqrt(static_cast<val_type>(Accumulate(base, func)));
            auto func2 = [&l2](val_type x) { return x / (l2 + 1e-14); };
            return Apply(func2, base);
        }

        template <class T_> Vector_<T_> Centralized(const Vector_<T_>& base) {
            using val_type = typename Vector_<T_>::value_type;
            size_t n = base.size();
            auto mean = Accumulate(base) / n;
            auto func = [&mean](val_type x) { return x - mean; };
            return Apply(func, base);
        }

        template <class T_> auto Covariance(const Vector_<T_>& src1, const Vector_<T_>& src2) {
            REQUIRE(src1.size() == src2.size(), "src1 size is not equal to src2 size");
            auto n = src1.size();
            auto s1 = Centralized(src1);
            auto s2 = Centralized(src2);
            return InnerProduct(s1, s2) / n;
        }

        template <class T_> auto Correlation(const Vector_<T_>& src1, const Vector_<T_>& src2) {
            REQUIRE(src1.size() == src2.size(), "src1 size is not equal to src2 size");
            auto s1 = L2Normalized(Centralized(src1));
            auto s2 = L2Normalized(Centralized(src2));
            return InnerProduct(s1, s2);
        }
    } // namespace Vector
} // namespace Dal
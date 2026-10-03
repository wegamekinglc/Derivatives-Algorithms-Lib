//
// Created by wegam on 2020/11/16.
//

#pragma once

#include <numeric>
#include <type_traits>

#include <dal/math/matrix/matrixs.hpp>
#include <dal/math/operators.hpp>
#include <dal/math/simdkernels.hpp>
#include <dal/math/vectors.hpp>
#include <dal/platform/platform.hpp>
#include <dal/utilities/algorithms.hpp>

namespace Dal {

    int AsInt(double src);
    int AsInt(std::ptrdiff_t src);
    int NearestInt(double src);

    template <class C_, class OP_> auto Accumulate(const C_& src, const OP_& op) {
        using val_type = decltype(op(0.0, *src.begin()));
        return std::accumulate(src.begin(), src.end(), val_type(0), op);
    }

    template <class C_> auto Accumulate(const C_& src) {
        using value_t = typename C_::value_type;
        return Accumulate(src, [](const value_t& x, const value_t& y) { return x + y; });
    }

    // double fast paths; preferred over the templates above for the contiguous storage we own
    inline double Accumulate(const Vector_<double>& src) { return Math::Sum(Math::DoubleData(src), src.size()); }

    namespace NumericsDetail {
        template <class C_>
        inline constexpr bool IS_CONTIGUOUS_DOUBLE =
            std::is_same_v<C_, Vector_<double>> || std::is_same_v<C_, Matrix_<double>::Row_> || std::is_same_v<C_, Matrix_<double>::ConstRow_>;
    } // namespace NumericsDetail

    template <class C1_, class C2_> auto InnerProduct(const C1_& src1, const C2_& src2) {
        if constexpr (NumericsDetail::IS_CONTIGUOUS_DOUBLE<C1_> && NumericsDetail::IS_CONTIGUOUS_DOUBLE<C2_>) {
            return Math::Dot(Math::DoubleData(src1), Math::DoubleData(src2), src1.size());
        } else {
            using value_type = typename C1_::value_type;
            return std::inner_product(src1.begin(), src1.end(), src2.begin(), value_type());
        }
    }

    namespace Vector {
        template <class T_> Vector_<T_> L1Normalized(const Vector_<T_>& base) {
            using val_type = typename Vector_<T_>::value_type;
            auto func = [](val_type x, val_type y) { return x + abs(y); };
            auto l1 = Accumulate(base, func);
            auto func2 = [&l1](val_type x) { return x / (l1 + 1e-14); };
            return Apply(func2, base);
        }

        template <class T_> Vector_<T_> L2Normalized(const Vector_<T_>& base) {
            using val_type = typename Vector_<T_>::value_type;
            auto func = [](val_type x, val_type y) { return x + y * y; };
            auto l2 = sqrt(static_cast<val_type>(Accumulate(base, func)));
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

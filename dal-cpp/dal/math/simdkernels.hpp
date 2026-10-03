//
// Created by wegamekinglc on 2026/10/3.
//

#pragma once

#include <cstddef>

namespace Dal::Math {
    // Empty ranges have no storage to dereference.
    template <class C_> [[nodiscard]] const double* DoubleData(const C_& src) { return src.size() == 0 ? nullptr : &*src.begin(); }

    [[nodiscard]] double Sum(const double* src, size_t n);
    [[nodiscard]] double Dot(const double* lhs, const double* rhs, size_t n);
    void Axpy(double scale, const double* src, double* dst, size_t n);
} // namespace Dal::Math

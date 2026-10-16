//
// Created by Codex on 2026/10/16.
//

#if defined(DAL_NORMAL_AVX2)
#if defined(_MSC_VER)
#pragma fp_contract(off)
#endif

#include <dal/math/normalavx2.hpp>

namespace Dal::Math::Detail {
    void InverseNormalAvx2(double* values, size_t n) { InverseNormalVector<Avx2_<false>>(values, n); }
} // namespace Dal::Math::Detail
#endif

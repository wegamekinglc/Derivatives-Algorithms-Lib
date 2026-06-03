//
// Created by wegam on 2022/12/18.
//

#pragma once

#include <dal/math/matrix/sparse.hpp>

namespace Dal::Sparse {
    FORCE_INLINE void AddBinomial(Square_* dst, double scale, int i, int j, double w_i, double w_j) {
        dst->Add(i, i, scale * w_i * w_i);
        dst->Add(i, j, scale * w_i * w_j);
        dst->Add(j, i, scale * w_i * w_j);
        dst->Add(j, j, scale * w_j * w_j);
    }

    FORCE_INLINE void AddCoupling(Square_* dst, int i, int j, double amount) {
        AddBinomial(dst, amount, i, j, 1.0, -1.0);
    }
}

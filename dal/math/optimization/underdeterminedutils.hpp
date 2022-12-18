//
// Created by wegam on 2022/12/18.
//

#pragma once

#include <dal/math/matrix/sparse.hpp>
#include <dal/math/matrix/banded.hpp>
#include <dal/time/datetime.hpp>

namespace Dal::Underdetermined {
    // add couplings to a sparse matrix
    void SelfCouplePWC(Sparse::Square_ *weights,
                       const Vector_ <DateTime_> &knots,
                       double tau_smoothing,
                       int offset = 0);

    Sparse::TriDiagonal_ *WeightsPWC(const Vector_<DateTime_> &knots, double tau_s) {
        std::unique_ptr <Sparse::TriDiagonal_> ret_val(new Sparse::TriDiagonal_(knots.size()));
        SelfCouplePWC(ret_val.get(), knots, tau_s, 0);
        return ret_val.release();
    }
}

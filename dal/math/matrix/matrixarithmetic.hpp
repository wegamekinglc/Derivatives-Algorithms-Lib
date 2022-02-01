//
// Created by Cheng Li on 2018/8/14.
//

#pragma once
#include <dal/math/vectors.hpp>
#include <dal/platform/platform.hpp>

namespace Dal {
    namespace Matrix {
        Vector_<> Vols(const Matrix_<>& cov, Matrix_<>* corr = nullptr);
    }
} // namespace Dal
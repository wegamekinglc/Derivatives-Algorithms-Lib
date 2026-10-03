//
// Created by wegamekinglc on 2026/10/3.
//

#pragma once

#include <Eigen/Dense>
#include <dal/math/matrix/matrixs.hpp>

namespace Dal::Matrix {
    using EigenMatrix = Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>;

    // zero-copy Eigen views over Matrix_<>'s dense row-major storage
    inline Eigen::Map<const EigenMatrix> EigenView(const Matrix_<>& m) { return Eigen::Map<const EigenMatrix>(m.Data(), m.Rows(), m.Cols()); }
    inline Eigen::Map<EigenMatrix> EigenView(Matrix_<>& m) { return Eigen::Map<EigenMatrix>(m.Data(), m.Rows(), m.Cols()); }
} // namespace Dal::Matrix

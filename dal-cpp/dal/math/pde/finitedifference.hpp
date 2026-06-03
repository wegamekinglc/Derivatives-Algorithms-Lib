//
// Created by wegam on 2023/2/25.
//

#pragma once

#include <dal/math/vectors.hpp>
#include <dal/math/matrix/banded.hpp>
#include <dal/math/matrix/matrixs.hpp>
#include <dal/math/pde/meshers/fdm1dmesher.hpp>

namespace Dal::PDE {
    Sparse::TriDiagonal_* Dx(const FDM1DMesher_& x);
    Sparse::TriDiagonal_* Dxx(const FDM1DMesher_& x);
}

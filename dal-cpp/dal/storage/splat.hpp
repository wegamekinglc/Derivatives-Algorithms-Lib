//
// Created by wegam on 2020/11/15.
//

#pragma once

#include <dal/math/matrix/matrixs.hpp>

namespace Dal {
    struct Cell_;
    class Storable_;

    Matrix_<Cell_> Splat(const Storable_& src);
    void SplatFile(const String_& fileName, const Storable_& src);
    Handle_<Storable_> UnSplat(const Matrix_<Cell_>& src, bool quiet);
    Handle_<Storable_> UnSplatFile(const String_& fileName, bool quiet);
} // namespace Dal

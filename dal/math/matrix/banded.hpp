//
// Created by wegam on 2021/2/22.
//

#pragma once

#include <dal/platform/platform.hpp>
#include <dal/math/matrix/matrixs.hpp>

namespace Dal {
    namespace Sparse {
        class Square_;
        Square_* NewBandDiagonal(int size, int n_above, int n_below);
    } // namespace Sparse

    class LowerBandAccumulator_ {
        Matrix_<> val_;

    public:
        LowerBandAccumulator_(int size, int n_below);
        void Add(const Vector_<>& v, int offset);

        void SolveLeft(const Vector_<>& b, Vector_<>* x) const;
        void SolveRight(const Vector_<>& b, Vector_<>* x) const;
    };
} // namespace Dal

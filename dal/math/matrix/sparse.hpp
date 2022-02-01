//
// Created by wegam on 2021/2/22.
//

#pragma once

#include <dal/math/matrix/decompositions.hpp>

namespace Dal::Sparse {

    class SymmetricDecomposition_ : public SymmetricMatrixDecomposition_ {
    public:
        // form J^T A^{-1} J for given J
        virtual void QForm(const Matrix_<>& j_mat, SquareMatrix_<>* dst) const;
    };

    class Square_ : noncopyable {
    public:
        virtual int Size() const = 0;
        virtual void MultiplyLeft(const Vector_<>& x, Vector_<>* b) const = 0;
        virtual void MultiplyRight(const Vector_<>& x, Vector_<>* b) const = 0;

        virtual bool IsSymmetric() const = 0;
        virtual SquareMatrixDecomposition_* Decompose() const = 0;
        SymmetricDecomposition_* DecomposeSymmetric() const;
        virtual const double& operator()(int i_row, int j_col) const = 0;
        virtual void Set(int i_row, int j_col, double val) = 0;
        virtual void Add(int i_row, int j_col, double val) { Set(i_row, j_col, val + operator()(i_row, j_col)); }
    };

} // namespace Dal::Sparse
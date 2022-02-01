//
// Created by wegam on 2021/2/22.
//

#pragma once

#include <dal/math/matrix/matrixs.hpp>

namespace Dal {
    template <class E_> class SquareMatrix_ {
        Matrix_<E_> val_;

    public:
        SquareMatrix_() = default;
        SquareMatrix_(int size) : val_(size, size) {}
        void Resize(int size) { val_.Resize(size, size); }

        operator const Matrix_<E_>&() const { return val_; };
        double& operator()(int i_row, int j_col) { return val_(i_row, j_col); }
        const double& operator()(int i_row, int j_col) const { return val_(i_row, j_col); }

        int Rows() const { return val_.Rows(); }
        typename Matrix_<E_>::Row_ Row(int ii) { return val_.Row(ii); }
        typename Matrix_<E_>::ConstRow_ Row(int ii) const { return val_.Row(ii); }
        int Cols() const { return val_.Cols(); }
        typename Matrix_<E_>::Col_ Col(int ii) { return val_.Col(ii); }
        typename Matrix_<E_>::ConstCol_ Col(int ii) const { return val_.Col(ii); }
    };

    namespace SquareMatrix {
        template <class E_> SquareMatrix_<E_> M1x1(const E_& val) {
            SquareMatrix_<E_> ret_val(1);
            ret_val(0, 0) = val;
            return ret_val;
        }
    } // namespace SquareMatrix
} // namespace Dal
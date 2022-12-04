#ifndef DAL_MATRIX_I
#define DAL_MATRIX_I

    %{
#include <dal/math/matrix/matrixs.hpp>
    %}

    template <class E_ = double> class Matrix_ {};

    %extend Matrix_ {
          Matrix_(int row, int col, E_ val = E_()) {
              return new Matrix_(row, col, val);
          }
          E_ __call__(int i, int j){ return (*$self)(i, j);}
    }

    %template(DoubleMatrix_) Matrix_<double>;

#endif
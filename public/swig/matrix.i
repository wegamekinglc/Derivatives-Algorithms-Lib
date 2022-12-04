#ifndef DAL_MATRIX_I
#define DAL_MATRIX_I

    %{
#include <dal/math/matrix/matrixs.hpp>
    %}

    template <class E_ = double> class Matrix_ {
      public:
        Matrix_(int row, int col, E_ val = E_());
    };

    %extend Matrix_ {
          E_ __call__(int i, int j){ return (*$self)(i, j);}
    }

    %template(DoubleMatrix_) Matrix_<double>;

#endif
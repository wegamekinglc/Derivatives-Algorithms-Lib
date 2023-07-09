//
// Created by wegam on 2021/2/20.
//

#include <dal/platform/strict.hpp>
#include <dal/math/matrix/decompositions.hpp>

#define COPY_ALIAS_AND_FORWARD(cname, func, imp)                                                                       \
    void cname::func(const Vector_<>& x, Vector_<>* b) const {                                                         \
        if (&x == b)                                                                                                   \
            imp(Vector_<>(x), b);                                                                                      \
        else                                                                                                           \
            imp(x, b);                                                                                                 \
    }

namespace Dal {
    COPY_ALIAS_AND_FORWARD(SquareMatrixDecomposition_, SolveLeft, XSolveLeft_af)
    COPY_ALIAS_AND_FORWARD(SquareMatrixDecomposition_, SolveRight, XSolveRight_af)
    COPY_ALIAS_AND_FORWARD(SquareMatrixDecomposition_, MultiplyLeft, XMultiplyLeft_af)
    COPY_ALIAS_AND_FORWARD(SquareMatrixDecomposition_, MultiplyRight, XMultiplyRight_af)
    COPY_ALIAS_AND_FORWARD(SymmetricMatrixDecomposition_, Solve, XSolve_af)
    COPY_ALIAS_AND_FORWARD(SymmetricMatrixDecomposition_, Multiply, XMultiply_af)
} // namespace Dal

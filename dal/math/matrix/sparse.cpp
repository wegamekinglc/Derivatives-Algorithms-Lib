//
// Created by wegam on 2021/2/22.
//

#include <dal/platform/platform.hpp>
#include <dal/math/matrix/sparse.hpp>
#include <dal/math/matrix/squarematrix.hpp>
#include <dal/utilities/numerics.hpp>
#include <dal/platform/strict.hpp>

namespace Dal::Sparse {
    // brute-force implementation
    void SymmetricDecomposition_::QForm(const Matrix_<>& j_mat, SquareMatrix_<>* dst) const {
        dst->Resize(j_mat.Rows());

        Vector_<> wij;
        Vector_<> row(j_mat.Cols());

        for(int ii = 0; ii < j_mat.Rows(); ++ii) {
            Copy(j_mat.Row(ii), &row);
            Solve(row, &wij);
            for(int jj = 0; jj <= ii; ++jj)
                (*dst)(ii, jj) = (*dst)(jj, ii) = InnerProduct(wij, j_mat.Row(jj));
        }
    }

    SymmetricDecomposition_* Square_::DecomposeSymmetric() const {
        std::unique_ptr<SquareMatrixDecomposition_> d(Decompose());
        if (auto ret_val = dynamic_cast<SymmetricDecomposition_*>(d.get())) {
            d.release();
            return ret_val;
        }

        REQUIRE(!IsSymmetric(), "symmetric matrix should return a type that implements QForm");
        return nullptr;
    }
}

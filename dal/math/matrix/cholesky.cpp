//
// Created by wegamekinglc on 22-12-17.
//

#include <dal/math/matrix/cholesky.hpp>
#include <dal/math/matrix/squarematrix.hpp>
#include <dal/math/matrix/decompositions.hpp>
#include <dal/math/matrix/decompositionsmisc.hpp>
#include <dal/math/operators.hpp>
#include <dal/utilities/functionals.hpp>
#include <dal/platform/strict.hpp>

namespace Dal {

    namespace {
        double CholeskyImpl(const SquareMatrix_<>& a, SquareMatrix_<>* out, double regularization = Dal::EPSILON) {
            const int n = a.Rows();
            double meanDiag = 0.0;
            for (int ii = 0; ii < n; ++ii) {
                auto rowI = out->Row(ii);
                auto beginI = rowI.begin();
                auto pa_ij = beginI;
                for (int jj = 0; jj < ii; ++jj, ++pa_ij) {
                    const double lockedIn = std::inner_product(beginI, pa_ij, out->Row(jj).begin(), 0.0);
                    const double needMore = a(jj, ii) - lockedIn;
                    *pa_ij = needMore == 0.0 ? 0.0 : needMore * (*out)(jj, jj) /
                                                     (Square((*out)(jj, jj)) + Square(regularization * meanDiag));
                }
                const double lockedIn = std::inner_product(out->Row(ii).begin(), out->Row(ii).begin() + ii,
                                                           out->Row(ii).begin(), 0.0);
                const double needMore = a(ii, ii) - lockedIn;
                (*out)(ii, ii) = Sqrt(Max(0.0, needMore));
                meanDiag += ((*out)(ii, ii) - meanDiag) / (1.0 + ii);
            }
            return meanDiag;
        }
    }

    SquareMatrixDecomposition_* CholeskyDecomposition(const SquareMatrix_<>& src) {
        const int n = src.Rows();
        SquareMatrix_<> out(n);
        CholeskyImpl(src, &out);
        return LowerTriangularAsDecomposition(out);
    }

    void CholeskySolve(SquareMatrix_<>* a, Vector_<Vector_<>>* b, double regularization) {
        const double meanDiag = CholeskyImpl(*a, a, regularization);
        const double reg = Square(regularization * meanDiag);
        const int n = a->Rows();
        REQUIRE(reg > 0.0, "regularization factor should be greater than 0.0");
        for (int ii = 0; ii < n; ++ii)
            (*a)(ii, ii) /= reg + Square((*a)(ii, ii));
        for (int ib = 0; ib < b->size(); ++ib) {
            Vector_<>& bb = (*b)[ib];
            for (int ii = 0; ii < n; ++ii) {
                bb[ii] -= std::inner_product(bb.begin(), bb.begin() + ii, a->Row(ii).begin(), 0.0);
                bb[ii] *= (*a)(ii, ii);
            }
            for (int ii = n - 1; ii >= 0; --ii) {
                bb[ii] *= (*a)(ii, ii);
                std::transform(bb.begin(), bb.begin() + ii, a->Row(ii).begin(), bb.begin(), LinearIncrement(-bb[ii]));
            }
        }
    }
} // namespace Dal

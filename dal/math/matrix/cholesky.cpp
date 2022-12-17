//
// Created by wegamekinglc on 22-12-17.
//

#include <dal/math/matrix/cholesky.hpp>
#include <dal/math/matrix/squarematrix.hpp>
#include <dal/math/matrix/decompositions.hpp>
#include <dal/math/operators.hpp>
#include <dal/utilities/functionals.hpp>
#include <dal/platform/strict.hpp>

namespace Dal {
    void CholeskySolve(SquareMatrix_<>* a, Vector_<Vector_<>>* b, double regularization) {
        const int n = a->Rows();
        REQUIRE(a->Cols() == n, "results cols are not same with a's rows");
        double meanDiag = 0.0;
        for (int ii = 0; ii < n; ++ii) {
            auto rowI = a->Row(ii);
            auto beginI = rowI.begin();
            auto pa_ij = beginI;
            for (int jj = 0; jj < ii; ++jj, ++pa_ij) {
                const double lockedIn = std::inner_product(beginI, pa_ij, a->Row(jj).begin(), 0.0);
                const double needMore = (*a)(jj, ii) - lockedIn;
                *pa_ij = needMore == 0.0 ? 0.0 : needMore * (*a)(jj, jj) / (Square((*a)(jj, jj)) + Square(regularization * meanDiag));
            }
            const double lockedIn = std::inner_product(a->Row(ii).begin(), a->Row(ii).begin() + ii, a->Row(ii).begin(), 0.0);
            const double needMore = (*a)(ii, ii) - lockedIn;
            (*a)(ii, ii) = Sqrt(Max(0.0, needMore));
            meanDiag += ((*a)(ii, ii) - meanDiag) / (1.0 + ii);
        }
        const double reg = Square(regularization * meanDiag);
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

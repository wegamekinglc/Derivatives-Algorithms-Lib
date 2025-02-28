//
// Created by wegamekinglc on 22-12-17.
//

#include <dal/platform/strict.hpp>
#include <dal/math/matrix/cholesky.hpp>
#include <dal/math/matrix/sparse.hpp>
#include <dal/math/matrix/squarematrix.hpp>
#include <dal/math/matrix/decompositions.hpp>
#include <dal/math/matrix/decompositionsmisc.hpp>
#include <dal/math/operators.hpp>
#include <dal/utilities/functionals.hpp>

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
                (*out)(ii, ii) = sqrt(max(0.0, needMore));
                meanDiag += ((*out)(ii, ii) - meanDiag) / (1.0 + ii);
            }
            return meanDiag;
        }

        class Cholesky_ : public Sparse::SymmetricDecomposition_ {
            SquareMatrix_<>* lower_;
            bool needKeep_;

        public:
            explicit Cholesky_(const SquareMatrix_<>& src, SquareMatrix_<>* lower = nullptr, double regularization = Dal::EPSILON) {
                if (lower) {
                    lower_ = lower;
                    needKeep_ = true;
                }
                else {
                    lower_ = new SquareMatrix_<>(src.Rows(), src.Cols());
                    needKeep_ = false;
                }

                const double meanDiag = CholeskyImpl(src, lower_, regularization);
                const double reg = Square(regularization * meanDiag);
                const int n = lower_->Rows();
                REQUIRE(reg > 0.0, "regularization factor should be greater than 0.0");
                for (int ii = 0; ii < n; ++ii)
                    (*lower_)(ii, ii) /= reg + Square((*lower_)(ii, ii));
            }

            ~Cholesky_() override {
                if (!needKeep_)
                    delete lower_;
            }

            void XMultiply_af(const Vector_<>& x, Vector_<>* b) const override {
                const int n = Size();
                Vector_<> temp(n, 0.0);
                // multiply by L^T
                for (int ii = 0; ii < n; ++ii) {
                    temp[ii] = std::inner_product(x.begin() + ii + 1, x.end(), lower_->Col(ii).begin() + ii + 1, 0.0);
                    temp[ii] += x[ii] / (*lower_)(ii, ii);
                }

                // multiply by L
                b->Resize(n);
                for (int ii = 0; ii < n; ++ii) {
                    (*b)[ii] = std::inner_product(temp.begin(), temp.begin() + ii, lower_->Row(ii).begin(), 0.0);
                    (*b)[ii] += temp[ii] / (*lower_)(ii, ii);
                }
            }

            void XSolve_af(const Vector_<>& b, Vector_<>* x) const override {
                const int n = Size();
                for (int ii = 0; ii < n; ++ii) {
                    (*x)[ii] = b[ii] - std::inner_product(x->begin(), x->begin() + ii, lower_->Row(ii).begin(), 0.0);
                    (*x)[ii] *= (*lower_)(ii, ii);
                }
                for (int ii = n - 1; ii >= 0; --ii) {
                    (*x)[ii] *= (*lower_)(ii, ii);
                    std::transform(x->begin(), x->begin() + ii, lower_->Row(ii).begin(), x->begin(), LinearIncrement(-(*x)[ii]));
                }
            }

            [[nodiscard]] int Size() const override { return lower_->Rows(); }

            Vector_<>::const_iterator MakeCorrelated(Vector_<>::const_iterator iid_begin, Vector_<>* correlated) const override {
                const int n = Size();
                correlated->Resize(n);
                for (int ii = 0; ii < n; ++ii) {
                    (*correlated)[ii] = std::inner_product(iid_begin, iid_begin + ii, lower_->Row(ii).begin(), 0.0);
                    (*correlated)[ii] += *(iid_begin + ii) / (*lower_)(ii, ii);
                }
                return correlated->begin();
            }
        };
    }

    Sparse::SymmetricDecomposition_* CholeskyDecomposition(const SquareMatrix_<>& src) {
        return new Cholesky_(src);
    }

    void CholeskySolve(SquareMatrix_<>* a, Vector_<Vector_<>>* b, double regularization) {
        Cholesky_ cholesky(*a, a, regularization);
        for (auto & bb : *b)
            cholesky.Solve(bb, &bb);
    }
} // namespace Dal
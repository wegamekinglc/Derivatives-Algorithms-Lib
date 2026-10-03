//
// Created by wegamekinglc on 22-12-17.
//

#include <memory>

#include <dal/platform/strict.hpp>
#include <dal/math/matrix/cholesky.hpp>
#include <dal/math/matrix/sparse.hpp>
#include <dal/math/matrix/squarematrix.hpp>
#include <dal/math/matrix/decompositions.hpp>
#include <dal/math/matrix/decompositionsmisc.hpp>
#include <dal/math/operators.hpp>
#include <dal/math/simdkernels.hpp>
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
                    const double lockedIn = Math::Dot(&*beginI, &*out->Row(jj).begin(), static_cast<size_t>(pa_ij - beginI));
                    const double needMore = a(jj, ii) - lockedIn;
                    *pa_ij = needMore == 0.0 ? 0.0 : needMore * (*out)(jj, jj) /
                                                     (Square((*out)(jj, jj)) + Square(regularization * meanDiag));
                }
                const double lockedIn = Math::Dot(&*beginI, &*beginI, static_cast<size_t>(ii));
                const double needMore = a(ii, ii) - lockedIn;
                (*out)(ii, ii) = sqrt(max(0.0, needMore));
                meanDiag += ((*out)(ii, ii) - meanDiag) / (1.0 + ii);
            }
            return meanDiag;
        }

        class Cholesky_ : public Sparse::SymmetricDecomposition_ {
            std::unique_ptr<SquareMatrix_<>> owned_;
            SquareMatrix_<>* lower_; // observes owned_ or the caller's in-place storage

        public:
            explicit Cholesky_(const SquareMatrix_<>& src, SquareMatrix_<>* lower = nullptr, double regularization = Dal::EPSILON)
                : owned_(lower ? nullptr : std::make_unique<SquareMatrix_<>>(src.Rows(), src.Cols())), lower_(lower ? lower : owned_.get()) {
                const double meanDiag = CholeskyImpl(src, lower_, regularization);
                const double reg = Square(regularization * meanDiag);
                const int n = lower_->Rows();
                REQUIRE(reg > 0.0, "regularization factor should be greater than 0.0");
                for (int ii = 0; ii < n; ++ii)
                    (*lower_)(ii, ii) /= reg + Square((*lower_)(ii, ii));
            }

            Cholesky_(const Cholesky_&) = delete;
            Cholesky_& operator=(const Cholesky_&) = delete;

            void XMultiply_af(const Vector_<>& x, Vector_<>* b) const override {
                const int n = Size();
                Vector_<> temp(n, 0.0);
                // multiply by L^T: row-wise scatter keeps the L reads contiguous and preserves
                // per-element accumulation order (ascending jj, diagonal last) of the column form
                for (int jj = 1; jj < n; ++jj)
                    Math::Axpy(x[jj], &*lower_->Row(jj).begin(), &*temp.begin(), static_cast<size_t>(jj));

                // multiply by L, folding the L^T diagonal in as each row completes
                b->Resize(n);
                for (int ii = 0; ii < n; ++ii) {
                    temp[ii] += x[ii] / (*lower_)(ii, ii);
                    (*b)[ii] = Math::Dot(Math::DoubleData(temp), &*lower_->Row(ii).begin(), static_cast<size_t>(ii));
                    (*b)[ii] += temp[ii] / (*lower_)(ii, ii);
                }
            }

            void XSolve_af(const Vector_<>& b, Vector_<>* x) const override {
                const int n = Size();
                x->Resize(n);
                for (int ii = 0; ii < n; ++ii) {
                    (*x)[ii] = b[ii] - Math::Dot(Math::DoubleData(*x), &*lower_->Row(ii).begin(), static_cast<size_t>(ii));
                    (*x)[ii] *= (*lower_)(ii, ii);
                }
                for (int ii = n - 1; ii >= 0; --ii) {
                    (*x)[ii] *= (*lower_)(ii, ii);
                    std::transform(x->begin(), x->begin() + ii, lower_->Row(ii).begin(), x->begin(), LinearIncrement(-(*x)[ii]));
                }
            }

            [[nodiscard]] int Size() const override { return lower_->Rows(); }

            Vector_<>::const_iterator MakeCorrelated(Vector_<>::const_iterator iidBegin, Vector_<>* correlated) const override {
                const int n = Size();
                correlated->Resize(n);
                for (int ii = 0; ii < n; ++ii) {
                    (*correlated)[ii] = Math::Dot(&*iidBegin, &*lower_->Row(ii).begin(), static_cast<size_t>(ii));
                    (*correlated)[ii] += *(iidBegin + ii) / (*lower_)(ii, ii);
                }
                return iidBegin + n;
            }
        };
    } // namespace

    std::unique_ptr<Sparse::SymmetricDecomposition_> CholeskyDecomposition(const SquareMatrix_<>& src) {
        return std::make_unique<Cholesky_>(src);
    }

    void CholeskySolve(SquareMatrix_<>* a, Vector_<Vector_<>>* b, double regularization) {
        Cholesky_ cholesky(*a, a, regularization);
        for (auto & bb : *b)
            cholesky.Solve(bb, &bb);
    }
} // namespace Dal

//
// Created by wegam on 2021/2/22.
//

#pragma once

#include <dal/platform/platform.hpp>
#include <dal/math/matrix/banded.hpp>
#include <dal/platform/strict.hpp>

#include <dal/utilities/algorithms.hpp>
#include <dal/utilities/numerics.hpp>
#include <dal/math/matrix/sparse.hpp>
#include <dal/math/matrix/squarematrix.hpp>
#include <cmath>


namespace Dal {
    namespace {
        const double ZERO = 0.0;

        void TriMultiply(const Vector_<>& x,
                         const Vector_<>& diag,
                         const Vector_<>& above,
                         const Vector_<>& below,
                         Vector_<>* r) {
            REQUIRE(x.size() == diag.size(), "");
            r->Resize(x.size());
            Transform(x, diag, std::multiplies<>(), r);
            auto pr = r->begin();
            for (auto px = x.begin() + 1, pa = above.begin(); pa != above.end(); ++px, ++pa, ++pr)
                *pr += *px * *pa;
            pr = r->begin() + 1;
            for (auto px = x.begin(), pb = below.begin(); px != below.end(); ++px, ++pb, ++pr)
                *pr += *px * *pb;
        }

        Vector_<> TridagBetaInverse(const Vector_<>& diag,
                                    const Vector_<>& above,
                                    const Vector_<>& below) {
            const int n = diag.size();
            Vector_<> ret_val(n);
            double gammaA = 0.0;
            for (int j = 0;; ++j) {
                const double beta = diag[j] - gammaA;
                REQUIRE(!IsZero(beta), "Tri-diagonal decomposition failed");
                ret_val[j] = 1.0 / beta;
                if (j == n - 1)
                    return ret_val;
                gammaA = above[j] * ret_val[j] * below[j];
            }
        }

        void TriSolve(const Vector_<>& b,
                      const Vector_<>& diag,
                      const Vector_<>& above,
                      const Vector_<>& below,
                      const Vector_<>& beta_inv,
                      Vector_<>* x) {
            const int n = diag.size();
            REQUIRE(b.size() == n, "Size must be compatible");
            x->Resize(n);
            (*x)[0] = b[0] * beta_inv[0];
            for (int j = 1; j < n; ++j)
                (*x)[j] = (b[j] - above[j - 1] * (*x)[j - 1]) * beta_inv[j];
            for (int j = n - 1; j > 0 ; --j)
                (*x)[j - 1] -= below[j - 1] * beta_inv[j - 1] * (*x)[j];
        }

        struct TriDecomp_ : SquareMatrixDecomposition_ {
            Vector_<> diag_, above_, below_;
            Vector_<> betaInv_;

            TriDecomp_(const Vector_<>& diag,
                       const Vector_<>& above,
                       const Vector_<>& below)
            : diag_(diag), above_(above), below_(below), betaInv_(TridagBetaInverse(diag, above, below)) {}

            int Size() const override { return diag_.size(); }
            void XMultiplyLeft_af(const Vector_<>& x, Vector_<>* b) const override {
                REQUIRE(x.size() == Size(), "Size should be compatible with x and the matrix");
                TriMultiply(x, diag_, above_, below_, b);
            }
            void XMultiplyRight_af(const Vector_<>& x, Vector_<>* b) const override {
                REQUIRE(x.size() == Size(), "Size should be compatible with x and the matrix");
                TriMultiply(x, diag_, below_, above_, b);
            }
            void XSolveLeft_af(const Vector_<>& b, Vector_<>* x) const override {
                REQUIRE(b.size() == Size(), "Size should be compatible with b and the matrix");
                TriSolve(b, diag_, below_, above_, betaInv_, x);
            }
            void XSolveRight_af(const Vector_<>& b, Vector_<>* x) const override {
                REQUIRE(b.size() == Size(), "Size should be compatible with b and the matrix");
                TriSolve(b, diag_, above_, below_, betaInv_, x);
            }
        };

        // Simplified version for symmetric case
        struct TriDecompSymm_ : Sparse::SymmetricDecomposition_ {
            Vector_<> diag_, above_;
            Vector_<> betaInv_;

            TriDecompSymm_(const Vector_<>& diag, const Vector_<>& above)
            : diag_(diag), above_(above), betaInv_(TridagBetaInverse(diag, above, above)) {}

            int Size() const override { return diag_.size(); }
            void XMultiply_af(const Vector_<>& x, Vector_<>* b) const override {
                REQUIRE(x.size() == Size(), "Size should be compatible with x and the matrix");
                TriMultiply(x, diag_, above_, above_, b);
            }
            void XSolve_af(const Vector_<>& b, Vector_<>* x) const override {
                REQUIRE(b.size() == Size(), "Size should be compatible with b and the matrix");
                TriSolve(b, diag_, above_, above_, betaInv_, x);
            }
            Vector_<>::const_iterator MakeCorrelated(Vector_<>::const_iterator iid_begin,
                                                     Vector_<>* correlated) const override {
                THROW("Tri-diagonal correlation matrices are not supported");
            }
        };

        template <class V_>
        auto TridiagAt(V_& diag, V_& above, V_& below, int i_row, int j_col) -> decltype(&diag[0]) {
            if (std::abs(i_row - j_col) > 1)
                return nullptr;
            if (i_row == j_col)
                return &diag[i_row];
            return i_row > j_col ? &below[j_col] : &above[i_row];
        }

        class Tridiagonal_ : public Sparse::Square_ {
            Vector_<> diag_, above_, below_;

        public:
            Tridiagonal_(int size)
                : diag_(size, 0.0), above_(size - 1, 0.0), below_(size - 1, 0.0) {}
            int Size() const override { return diag_.size(); }
            bool IsSymmetric() const override { return above_ == below_; }

            double* At(int i_row, int j_col) {
                return TridiagAt(diag_, above_, below_, i_row, j_col);
            }

            const double& operator()(int i_row, int j_col) const override {
                const double* temp = TridiagAt(diag_, above_, below_, i_row, j_col);
                return temp ? *temp : ZERO;
            }

            void Set(int i_row, int j_col, double val) override {
                double* dst = At(i_row, j_col);
                REQUIRE(dst, "out of band write to tri-diagonal");
                *dst = val;
            }

            void Add(int i_row, int j_col, double inc) override {
                double* dst = At(i_row, j_col);
                REQUIRE(dst, "out of band write to tri-diagonal");
                *dst += inc;
            }

            void MultiplyLeft(const Vector_<>& x, Vector_<>* b) const override {
                TriMultiply(x, diag_, above_, below_, b);
            }
            void MultiplyRight(const Vector_<>& x, Vector_<>* b) const override {
                TriMultiply(x, diag_, below_, above_, b);
            }
            SquareMatrixDecomposition_* Decompose() const override {
                if (IsSymmetric())
                    return new TriDecompSymm_(diag_, above_);
                return new TriDecomp_(diag_, above_, below_);
            }
        };

        /*
         * more general band diagonals
         * band diagonals are stored as in Numerical Recipes: columns below diagonal, then diagonal, then above diagonal
         */
        struct BandElements_ {
            Matrix_<> store_;
            const Matrix_<>& view_;
            int nBelow_;

            BandElements_(int size, int n_below, int n_above)
            : store_(size, 1 + n_below + n_above), view_(store_), nBelow_(n_below) {
                store_.Fill(0.0);
            }

            // also can borrow an existing matrix in which case store_ will be empty
            BandElements_(const Matrix_<>& val, int n_below)
            : view_(val), nBelow_(n_below) {}

            const double& operator()(int row, int col) const {
                const int myCol = (col - row) + nBelow_;
                if (myCol >= 0 && myCol < view_.Cols())
                    return view_(row, myCol);
                return ZERO;
            }

            double& At(int row, int col) {
                REQUIRE(!store_.Empty(), "Can't write to view-only band elements");
                const int myCol = (col - row) + nBelow_;
                REQUIRE(myCol >= 0 && myCol < view_.Cols(), "Index outside diagonal band");
                return store_(row, myCol);
            }
        };

        // first some free functions supporting implementation

        // left-multiplication
        template <bool transpose>
        void BandedMultiply(const BandElements_& val, const Vector_<>& x, Vector_<>* b) {
            REQUIRE(b != &x, "no aliasing here");
            const int n = x.size();
            REQUIRE(val.view_.Rows() == n, "Size should be compatible with x and the matrix");
            b->Resize(n);
            b->Fill(0.0);
            for (int ii = 0; ii < n; ++ii) {
                const int jStop = Min(n, ii + val.view_.Cols() - val.nBelow_);
                for (int jj = Max(0, ii - val.nBelow_); jj < jStop; ++jj) {
                    (*b)[transpose ? jj : ii] += x[transpose ? ii : jj] * val(ii, jj);
                }
            }
        }

        // this works even if x == &b
        void BandedLSolve(const BandElements_& val, const Vector_<>& b, Vector_<>* x) {
            REQUIRE(val.view_.Cols() == val.nBelow_ + 1, "n_below and cols are not matched");
            const int n = b.size();
            REQUIRE(val.view_.Rows() == n, "Size should be compatible with b and the matrix");
            x->Resize(n);
            for (int ii = 0; ii < n; ++ii) {
                double residual = b[ii];
                for (int jj = Max(0, ii - val.nBelow_); jj < ii; ++jj)
                    residual -= (*x)[jj] * val(ii, jj);
                REQUIRE(!IsZero(val(ii, ii)), "Overflow in banded L-solve");
                (*x)[ii] = residual / val(ii, ii);
            }
        }

        // this works even if x == &b
        void BandedLTransposeSolve(const BandElements_& val, const Vector_<>& b, Vector_<>* x) {
            REQUIRE(val.view_.Cols() == val.nBelow_ + 1, "n_below and cols are not matched");
            const int n = b.size();
            REQUIRE(val.view_.Rows() == n, "Size should be compatible with b and the matrix");
            x->Resize(n);
            for (int ii = n - 1; ii >= 0 ; --ii) {
                double residual = b[ii];
                for (int jj = Max(n - 1, ii + val.nBelow_); jj > ii ; --jj)
                    residual -= (*x)[jj] * val(jj, ii);
                REQUIRE(!IsZero(val(ii, ii)), "Overflow in banded L-solve");
                (*x)[ii] = residual / val(ii, ii);
            }
        }

        // decomposition
        class BandedCholesky_ : public Sparse::SymmetricDecomposition_ {
            BandElements_ val_;

        public:
            explicit BandedCholesky_(const BandElements_& llt)
            : val_(llt.view_.Rows(), llt.nBelow_, 0) {
                static const double SMALL = 1.0e-11;
                REQUIRE(llt.view_.Cols() == 2 * llt.nBelow_ + 1, "Cols should be 2 * n_below + 1");
                const int n = llt.view_.Rows();
                for (int ii = 0; ii < n; ++ii) {
                    const int iMin = Max(0, ii - llt.nBelow_);
                    for (int jj = iMin; jj <= ii ; ++jj) {
                        double residual = llt(ii, jj);
                        for (int kk = iMin; kk < jj ; ++kk)
                            residual -= val_(ii, kk) * val_(jj, kk);
                        if (jj < ii) {
                            if (IsZero(residual))
                                val_.At(ii, jj) = 0.0;
                            else {
                                REQUIRE(!IsZero(val_(jj, jj)), "Overflow");
                                val_.At(ii, jj) = residual / val_(jj, jj);
                            }
                        }
                        else {
                            REQUIRE(residual > -SMALL, "Non-positive-definite matrix");
                            val_.At(ii, ii) = std::sqrt(residual);
                        }
                    }
                }
            }

            int Size() const override { return val_.view_.Rows(); }

            void XMultiply_af(const Vector_<>& x, Vector_<>* b) const override {
                Vector_<> temp;
                BandedMultiply<true>(val_, x, &temp);
                BandedMultiply<false>(val_, temp, b);
            }

            void XSolve_af(const Vector_<>& b, Vector_<>* x) const override {
                BandedLSolve(val_, b, x);
                BandedLTransposeSolve(val_, *x, x);
            }

            Vector_<>::const_iterator MakeCorrelated(Vector_<>::const_iterator iid_begin,
                                                     Vector_<>* correlated) const override {
                const int n = Size();
                correlated->Resize(n);
                correlated->Fill(0.0);
                for (int ii = 0; ii < n; ++ii, ++iid_begin) {
                    for (int jj = Min(n - 1, ii + val_.nBelow_); jj >= ii ; --jj)
                        (*correlated)[jj] += val_(jj, ii) * (*iid_begin);
                }

                // TODO: why return the end?
                return iid_begin;
            }

            void QForm(const Matrix_<>& j_mat, SquareMatrix_<>* form) const override {
                REQUIRE(j_mat.Cols() == Size(), "j_mat size should match with this matrix's size");
                Vector_<Vector_<>> temp(j_mat.Rows());

                // compute L^{-1}
                for (int ii = 0; ii < j_mat.Rows(); ++ii) {
                    temp[ii].Resize(Size());
                    Copy(j_mat[ii], &temp[ii]);
                    BandedLSolve(val_, temp[ii], &temp[ii]);
                }

                // compute result
                form->Resize(j_mat.Rows());
                for (int io = 0; io < j_mat.Rows(); ++io)
                    for (int k = 0; k <= io ; ++k)
                        (*form)(io, k) = (*form)(k, io) = InnerProduct(temp[io], temp[k]);
            }
        };

        class Banded_ : public Sparse::Square_ {
            BandElements_ val_;

        public:
            Banded_(int size, int n_above, int n_below)
            : val_(size, n_above, n_below) {}

            int Size() const override { return val_.view_.Rows(); }
            void MultiplyLeft(const Vector_<>& x, Vector_<>* b) const override {
                BandedMultiply<false>(val_, x, b);
            }
            void MultiplyRight(const Vector_<>& x, Vector_<>* b) const override {
                BandedMultiply<true>(val_, x, b);
            }

            bool IsSymmetric() const override {
                // brute-force check
                const int n = Size();
                const int width = Max(val_.nBelow_, val_.view_.Cols() - val_.nBelow_ - 1);
                for (int ii = 0; ii < n; ++ii) {
                    for (int jj = Max(0, ii - width); jj <=Max(n - 1, ii + width) ; ++jj)
                        if (!IsZero(val_(ii, jj) - val_(jj, ii)))
                            return false;
                }
                return true;
            }
            SquareMatrixDecomposition_* Decompose() const override {
                REQUIRE(IsSymmetric(), "Cholesky decomposition requires a symmetric matrix");
                return new BandedCholesky_(val_);
            }

            const double& operator()(int row, int col) const override {
                return val_(row, col);
            }
            void Set(int row, int col, double val) override {
                val_.At(row, col) = val;
            }
            void Add(int row, int col, double val) override {
                val_.At(row, col) += val;
            }
        };
    }

    namespace Sparse {
        Square_* NewBandDiagonal(int size, int n_above, int n_below) {
            REQUIRE(size > 0, "size should be larger than 0");
            if (n_above <= 1 && n_below <= 1)
                return new Tridiagonal_(size);
            return new Banded_(size, n_above, n_below);
        }
    }

    // ----------------------------------------------------------------

    namespace {
        Vector_<> PadAtFront(const Vector_<>& src, int size) {
            REQUIRE(src.size() <= size, "Vector for pad's size should not be greater than target size");
            if (src.size() == size)
                return src;
            return Concatenate(Vector_<>(size - src.size(), 0.0), src);
        }
    }

    LowerBandAccumulator_::LowerBandAccumulator_(int size, int n_below)
    : val_(size, n_below + 1) {
        val_.Fill(0.0);
    }

    void LowerBandAccumulator_::Add(const Vector_<>& v_in, int offset) {
        REQUIRE(v_in.size() <= val_.Cols(), "Too many nonzero elements in v");
        int iRow = v_in.size() + offset;
        REQUIRE(iRow < val_.Rows(), "V is too large");

        Vector_<> v;
        while (iRow >= 0) {
            auto row = val_.Row(iRow);
            if (AllOf(row, IsZero<double>)) {
                std::copy(v.begin(), v.end(), row.end() - v.size());
                break;
            }
            if (AllOf(v, IsZero<double>))
                break;
            if (v.empty())
                v = PadAtFront(v_in, Min(iRow + 1, val_.Cols()));
            const double r = std::sqrt(Square(v.back()) + Square(row.back()));
            const double c = row.back() / r;
            const double s = v.back() / r;

            auto pr = row.begin() + (row.size() - v.size());
            for (auto pv = v.begin(); pv != v.end(); ++pv, ++pr) {
                // rotation
                const double save = *pr;
                *pv = c * (*pv) - s * (*pr);
                *pr = c * (*pr) + s * save;
            }
            ASSURE(IsZero(v.back()), "Back of v should be zero");
            --iRow;
            if (iRow >= v.size())
                std::rotate(v.begin(), v.end() - 1, v.end());
            else
                v.pop_back();
        }
    }

    void LowerBandAccumulator_::SolveLeft(const Vector_<>& b, Vector_<>* x) const {
        BandedLSolve(BandElements_(val_, val_.Cols() - 1), b, x);
    }

    void LowerBandAccumulator_::SolveRight(const Vector_<>& b, Vector_<>* x) const {
        BandedLSolve(BandElements_(val_, val_.Cols() - 1), b, x);
    }
}

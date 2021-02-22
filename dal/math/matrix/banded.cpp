//
// Created by wegam on 2021/2/22.
//

#pragma once

#include <dal/platform/platform.hpp>
#include <dal/math/matrix/banded.hpp>
#include <dal/platform/strict.hpp>

#include <dal/utilities/algorithms.hpp>
#include <dal/math/matrix/sparse.hpp>
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
    }
}

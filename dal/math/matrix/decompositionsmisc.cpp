//
// Created by wegam on 2022/12/17.
//

#include <dal/platform/strict.hpp>
#include <dal/math/matrix/decompositionsmisc.hpp>
#include <dal/utilities/algorithms.hpp>
#include <dal/math/operators.hpp>
#include <dal/math/matrix/squarematrix.hpp>
#include <dal/math/matrix/decompositions.hpp>
#include <dal/utilities/exceptions.hpp>

namespace Dal {
    namespace {
        struct ProtectedDivide_ {
            double operator()(double lhs, double rhs) const {
                REQUIRE(!IsZero(rhs), "Division by zero");
                return lhs / rhs;
            }
        };

        struct MultiplyBySqrt_ {
            double operator()(double lhs, double rhs) const {
                REQUIRE(!IsNegative(rhs), "Negative variance");
                return lhs * sqrt(max(rhs, 0.0));
            }
        };

        struct Diagonal_ : SymmetricMatrixDecomposition_ {
            Vector_<> vals_;

            explicit Diagonal_(const Vector_<> &vals) : vals_(vals) {}

            [[nodiscard]] int Size() const override { return vals_.size(); }

            void XMultiply_af(const Vector_<> &x, Vector_<> *b) const override {
                Transform(x, vals_, std::multiplies<>(), b);
            }

            void XSolve_af(const Vector_<> &b, Vector_<> *x) const override {
                NOTE("Dividing by diagonal matrix");
                Transform(b, vals_, ProtectedDivide_(), x);
            }

            Vector_<>::const_iterator
            MakeCorrelated(Vector_<>::const_iterator iid_begin, Vector_<> *devs) const override {
                const int n = Size();
                devs->Resize(n);
                for (int ii = 0; ii < n; ++ii, ++iid_begin) {
                    REQUIRE(!IsNegative(vals_[ii]), "Negative variance, can't MakeCorrelated");
                    (*devs)[ii] = sqrt(max(vals_[ii], 0.0)) * *iid_begin;
                }
                return iid_begin;
            }
        };

        struct LowerTriangular_ : SquareMatrixDecomposition_ {
            SquareMatrix_<> vals_;

            explicit LowerTriangular_(const SquareMatrix_<> &vals) : vals_(vals) {}

            [[nodiscard]] int Size() const override { return vals_.Rows(); }

            void XMultiplyLeft_af(const Vector_<> &x, Vector_<> *b) const override {
                const int n = Size();
                b->Resize(n);
                for (int ii = 0; ii < n; ++ii)
                    (*b)[ii] = std::inner_product(x.begin(), x.begin() + ii, vals_.Row(ii).begin(), 0.0);
            }

            void XMultiplyRight_af(const Vector_<> &x, Vector_<> *b) const override {
                const int n = Size();
                b->Resize(n);
                for (int ii = 0; ii < n; ++ii)
                    (*b)[ii] = std::inner_product(x.begin() + ii, x.end(), vals_.Col(ii).begin() + ii, 0.0);
            }

            void XSolveLeft_af(const Vector_<> &b, Vector_<> *x) const override {
                ProtectedDivide_ DIVIDE;
                NOTICE("Left-solve by lower triangular");
                const int n = Size();
                x->Resize(n);
                for (int ii = 0; ii < n; ++ii) {
                    auto stop = x->begin() + ii;
                    const double residual = -inner_product(x->begin(), stop, vals_.Row(ii).begin(), -b[ii]);
                    *stop = DIVIDE(residual, vals_(ii, ii));
                }
            }

            void XSolveRight_af(const Vector_<> &b, Vector_<> *x) const override {
                ProtectedDivide_ DIVIDE;
                NOTICE("Right-solve by lower triangular");
                const int n = Size();
                x->Resize(n);
                for (int ii = n - 1; ii >= 0; --ii) {
                    auto start = x->begin() + ii + 1;
                    const double residual = -inner_product(start, x->end(), vals_.Col(ii).begin() + ii + 1, -b[ii]);
                    (*x)[ii] = DIVIDE(residual, vals_(ii, ii));
                }
            }
        };
    }

    SymmetricMatrixDecomposition_* DiagonalAsDecomposition(const Vector_<> &diag) {
        return new Diagonal_(diag);
    }

    SquareMatrixDecomposition_* LowerTriangularAsDecomposition(const SquareMatrix_<> &elements) {
        return new LowerTriangular_(elements);
    }
}
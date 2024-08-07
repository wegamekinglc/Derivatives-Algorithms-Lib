//
// Created by wegamekinglc on 22-12-17.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/math/matrix/bcg.hpp>
#include <dal/math/matrix/sparse.hpp>
#include <dal/utilities/algorithms.hpp>
#include <dal/utilities/numerics.hpp>
#include <dal/utilities/functionals.hpp>

namespace Dal {
    namespace {
        struct XPrecondition_ {
            const HasPreConditioner_* a_;
            explicit XPrecondition_(const Sparse::Square_& a) : a_(dynamic_cast<const HasPreConditioner_*>(&a)) {}
            void Left(const Vector_<>& b, Vector_<>* x) const {
                if (a_)
                    a_->PreConditionerSolveLeft(b, x);
                else if (x != &b)
                    Copy(b, x);
            }
            void Right(const Vector_<>& b, Vector_<>* x) const {
                if (a_)
                    a_->PreConditionerSolveRight(b, x);
                else if (x != &b)
                    Copy(b, x);
            }
        };

        struct XSparseTransposed_ : public Sparse::Square_, public HasPreConditioner_ {
            const Sparse::Square_& a_;
            XPrecondition_ p_;
            explicit XSparseTransposed_(const Sparse::Square_& a) : a_(a), p_(a) {}

            [[nodiscard]] int Size() const override { return a_.Size(); }
            void XMultiplyLeft_af(const Vector_<>& x, Vector_<>* b) const { a_.MultiplyRight(x, b); }
            void XSolveLeft_af(const Vector_<>& b, Vector_<>* x) const {
                THROW("Unreachable: left-solve after transpose");
            }
            void PreConditionerSolveLeft(const Vector_<>& x, Vector_<>* b) const override { p_.Right(x, b); }
        };
    } // namespace

    void Sparse::CGSolve(const Sparse::Square_& A,
                         const Vector_<>& b,
                         double tol_rel,
                         double tol_abs,
                         int max_iterations,
                         Vector_<>* x) {
        const int n = A.Size();
        REQUIRE(b.size() == n && x->size() == n, "matrix size is not compatible");
        REQUIRE((IsPositive(tol_rel) || IsPositive(tol_abs)) && max_iterations > 0, "parameters is not valid");

        double tNorm = tol_rel * sqrt(InnerProduct(b, b)) + tol_abs;
        XPrecondition_ precondition(A);
        Vector_<> r(n);
        Vector_<> z(n);
        Vector_<> p(n);
        A.MultiplyLeft(*x, &r);
        Transform(b, r, std::minus<>(), &r); // r = b - Ax
        double betaPrev;
        for (int ii = 0; ii < max_iterations; ++ii) {
            precondition.Left(r, &z);
            const double beta = InnerProduct(z, r);
            p *= ii > 0 ? beta / betaPrev : 0.0;
            p += z;
            betaPrev = beta;
            A.MultiplyLeft(p, &z);
            const double alphaK = beta / InnerProduct(z, p);
            Transform(x, p, LinearIncrement(alphaK));
            Transform(&r, z, LinearIncrement(-alphaK));
            if (sqrt(InnerProduct(r, r)) <= tNorm)
                return;
        }
        THROW("Exhausted iterations in CGSolve");
    }

    void Sparse::BCGSolve(const Sparse::Square_ &A,
                          const Vector_<> &b,
                          double tol_rel,
                          double tol_abs,
                          int max_iterations, Vector_<> *x) {
        const int n = A.Size();
        REQUIRE(b.size() == n && x->size() == n, "matrix size is not compatible");
        REQUIRE((IsPositive(tol_rel) || IsPositive(tol_abs)) && max_iterations > 0, "parameters is not valid");

        double tNorm = tol_rel * sqrt(InnerProduct(b, b)) + tol_abs;
        XPrecondition_ precondition(A);
        Vector_<> r(n);
        Vector_<> rr(n);
        Vector_<> z(n);
        Vector_<> zz(n);
        Vector_<> p(n);
        Vector_<> pp(n);

        A.MultiplyLeft(*x, &r);
        Transform(b, r, std::minus<>(), &r); // r = b - Ax
        rr = r;

        double betaPrev;
        for (int ii = 0; ii < max_iterations; ++ii) {
            precondition.Left(r, &z);
            precondition.Right(rr, &zz);
            const double beta = InnerProduct(zz, r);
            const double multiply = ii > 0 ? beta / betaPrev : 0.0;
            p *= multiply;
            pp *= multiply;
            p += z;
            pp += zz;
            betaPrev = beta;
            A.MultiplyLeft(p, &z);
            A.MultiplyRight(pp, &zz);
            const double alphaK = beta / InnerProduct(z, pp);
            Transform(x, p, LinearIncrement(alphaK));
            Transform(&r, z, LinearIncrement(-alphaK));
            Transform(&rr, zz, LinearIncrement(-alphaK));
            if (sqrt(InnerProduct(r, r)) <= tNorm)
                return;
        }
        THROW("Exhausted iterations in CGSolve");
    }
} // namespace Dal
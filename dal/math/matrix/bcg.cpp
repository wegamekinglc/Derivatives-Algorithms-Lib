//
// Created by wegamekinglc on 22-12-17.
//

#include <dal/math/matrix/bcg.hpp>
#include <dal/math/matrix/sparse.hpp>
#include <dal/platform/strict.hpp>
#include <dal/utilities/algorithms.hpp>
#include <dal/utilities/numerics.hpp>
#include <dal/utilities/functionals.hpp>

namespace Dal {
    namespace {
        struct XPrecondition_ {
            const HasPreconditioner_* a_;
            explicit XPrecondition_(const Sparse::Square_& a) : a_(dynamic_cast<const HasPreconditioner_*>(&a)) {}
            void Left(const Vector_<>& b, Vector_<>* x) const {
                if (a_)
                    a_->PreconditionerSolveLeft(b, x);
                else if (x != &b)
                    Copy(b, x);
            }
            void Right(const Vector_<>& b, Vector_<>* x) const {
                if (a_)
                    a_->PreconditionerSolveRight(b, x);
                else if (x != &b)
                    Copy(b, x);
            }
        };

        struct XSparseTransposed_ : public Sparse::Square_, public HasPreconditioner_ {
            const Sparse::Square_& a_;
            XPrecondition_ p_;
            explicit XSparseTransposed_(const Sparse::Square_& a) : a_(a), p_(a) {}

            [[nodiscard]] int Size() const override { return a_.Size(); }
            void XMultiplyLeft_af(const Vector_<>& x, Vector_<>* b) const { a_.MultiplyRight(x, b); }
            void XSolveLeft_af(const Vector_<>& b, Vector_<>* x) const {
                assert(!"Unreachable: left-solve after transpose");
            }
            void PreconditionerSolveLeft(const Vector_<>& x, Vector_<>* b) const override { p_.Right(x, b); }
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

        double tNorm = tol_rel * Sqrt(InnerProduct(b, b)) + tol_abs;
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
            if (Sqrt(InnerProduct(r, r)) <= tNorm)
                return;
        }
        THROW("Exhausted iterations in CGSolve");
    }
} // namespace Dal
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

    namespace {
        // Unified Krylov iteration for CG (biConjugate = false) and BCG (true). CG aliases the
        // shadow vectors onto the real ones so the shared arithmetic stays byte-identical to
        // standalone CG; shadow maintenance is gated on the flag.
        void
        KrylovSolve_(const Sparse::Square_& A, const Vector_<>& b, double tolRel, double tolAbs, int maxIterations, bool biConjugate, Vector_<>* x) {
            const int n = A.Size();
            REQUIRE(b.size() == n && x->size() == n, "matrix size is not compatible");
            REQUIRE((IsPositive(tolRel) || IsPositive(tolAbs)) && maxIterations > 0, "parameters is not valid");

            double tNorm = tolRel * sqrt(InnerProduct(b, b)) + tolAbs;
            XPrecondition_ precondition(A);
            Vector_<> r(n);
            Vector_<> rr(n);
            Vector_<> z(n);
            Vector_<> zz(n);
            Vector_<> p(n);
            Vector_<> pp(n);

            A.MultiplyLeft(*x, &r);
            Transform(b, r, std::minus<>(), &r); // r = b - Ax
            if (biConjugate)
                rr = r;

            // CG collapses each shadow vector onto its real counterpart, so beta and alphaK
            // reduce to the standalone-CG expressions InnerProduct(z, r) and beta/InnerProduct(z, p).
            Vector_<>& zzRef = biConjugate ? zz : z;
            Vector_<>& ppRef = biConjugate ? pp : p;

            double betaPrev;
            for (int ii = 0; ii < maxIterations; ++ii) {
                precondition.Left(r, &z);
                if (biConjugate)
                    precondition.Right(rr, &zz);
                const double beta = InnerProduct(zzRef, r);
                const double multiply = ii > 0 ? beta / betaPrev : 0.0;
                p *= multiply;
                if (biConjugate)
                    pp *= multiply;
                p += z;
                if (biConjugate)
                    pp += zz;
                betaPrev = beta;
                A.MultiplyLeft(p, &z);
                if (biConjugate)
                    A.MultiplyRight(pp, &zz);
                const double alphaK = beta / InnerProduct(z, ppRef);
                Transform(x, p, LinearIncrement(alphaK));
                Transform(&r, z, LinearIncrement(-alphaK));
                if (biConjugate)
                    Transform(&rr, zz, LinearIncrement(-alphaK));
                if (sqrt(InnerProduct(r, r)) <= tNorm)
                    return;
            }
            THROW(biConjugate ? "Exhausted iterations in BCGSolve" : "Exhausted iterations in CGSolve");
        }
    } // namespace

    void Sparse::CGSolve(const Sparse::Square_& A, const Vector_<>& b, double tolRel, double tolAbs, int maxIterations, Vector_<>* x) {
        KrylovSolve_(A, b, tolRel, tolAbs, maxIterations, false, x);
    }

    void Sparse::BCGSolve(const Sparse::Square_& A, const Vector_<>& b, double tolRel, double tolAbs, int maxIterations, Vector_<>* x) {
        KrylovSolve_(A, b, tolRel, tolAbs, maxIterations, true, x);
    }
} // namespace Dal

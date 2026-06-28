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
        // Unified Krylov state for CG (biConjugate_ = false) and BCG (true). For CG, zzRef_/ppRef_
        // alias the real vectors so the shared arithmetic is byte-identical to standalone CG.
        struct KrylovState_ {
            const Sparse::Square_& A_;
            const XPrecondition_& precondition_;
            const bool biConjugate_;
            Vector_<> r_;
            Vector_<> rr_;
            Vector_<> z_;
            Vector_<> zz_;
            Vector_<> p_;
            Vector_<> pp_;
            Vector_<>& zzRef_; // aliases z for CG, zz for BCG
            Vector_<>& ppRef_; // aliases p for CG, pp for BCG
            double betaPrev_;

            KrylovState_(const Sparse::Square_& a, const XPrecondition_& prec, bool biConjugate, int n)
                : A_(a), precondition_(prec), biConjugate_(biConjugate), r_(n), rr_(biConjugate ? n : 0), z_(n), zz_(biConjugate ? n : 0), p_(n),
                  pp_(biConjugate ? n : 0), zzRef_(biConjugate ? zz_ : z_), ppRef_(biConjugate ? pp_ : p_), betaPrev_(0.0) {}
        };

        double PrepareDirection(KrylovState_& s, int ii) {
            s.precondition_.Left(s.r_, &s.z_);
            if (s.biConjugate_)
                s.precondition_.Right(s.rr_, &s.zz_);
            const double beta = InnerProduct(s.zzRef_, s.r_);
            const double multiply = ii > 0 ? beta / s.betaPrev_ : 0.0;
            s.p_ *= multiply;
            if (s.biConjugate_)
                s.pp_ *= multiply;
            s.p_ += s.z_;
            if (s.biConjugate_)
                s.pp_ += s.zz_;
            s.betaPrev_ = beta;
            return beta;
        }

        void UpdateSolution(KrylovState_& s, double beta, Vector_<>* x) {
            s.A_.MultiplyLeft(s.p_, &s.z_);
            if (s.biConjugate_)
                s.A_.MultiplyRight(s.pp_, &s.zz_);
            const double alphaK = beta / InnerProduct(s.z_, s.ppRef_);
            Transform(x, s.p_, LinearIncrement(alphaK));
            Transform(&s.r_, s.z_, LinearIncrement(-alphaK));
            if (s.biConjugate_)
                Transform(&s.rr_, s.zz_, LinearIncrement(-alphaK));
        }

        void ValidateKrylovParams(int n, const Vector_<>& b, const Vector_<>* x, double tolRel, double tolAbs, int maxIterations) {
            REQUIRE(b.size() == n && x->size() == n, "matrix dimensions are incompatible");
            REQUIRE((IsPositive(tolRel) || IsPositive(tolAbs)) && maxIterations > 0, "parameters are invalid");
        }

        void
        KrylovSolve(const Sparse::Square_& A, const Vector_<>& b, double tolRel, double tolAbs, int maxIterations, bool biConjugate, Vector_<>* x) {
            const int n = A.Size();
            ValidateKrylovParams(n, b, x, tolRel, tolAbs, maxIterations);

            const double tNorm = tolRel * sqrt(InnerProduct(b, b)) + tolAbs;
            XPrecondition_ precondition(A);
            KrylovState_ s(A, precondition, biConjugate, n);

            A.MultiplyLeft(*x, &s.r_);
            Transform(b, s.r_, std::minus<>(), &s.r_); // r = b - Ax
            if (biConjugate)
                s.rr_ = s.r_;

            for (int ii = 0; ii < maxIterations; ++ii) {
                const double beta = PrepareDirection(s, ii);
                UpdateSolution(s, beta, x);
                if (sqrt(InnerProduct(s.r_, s.r_)) <= tNorm)
                    return;
            }
            THROW(biConjugate ? "Exhausted iterations in BCGSolve" : "Exhausted iterations in CGSolve");
        }
    } // namespace

    void Sparse::CGSolve(const Sparse::Square_& A, const Vector_<>& b, double tolRel, double tolAbs, int maxIterations, Vector_<>* x) {
        KrylovSolve(A, b, tolRel, tolAbs, maxIterations, false, x);
    }

    void Sparse::BCGSolve(const Sparse::Square_& A, const Vector_<>& b, double tolRel, double tolAbs, int maxIterations, Vector_<>* x) {
        KrylovSolve(A, b, tolRel, tolAbs, maxIterations, true, x);
    }
} // namespace Dal

//
// Created by wegamekinglc on 22-12-17.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <functional>
#include <dal/math/matrix/bcg.hpp>
#include <dal/math/matrix/sparse.hpp>
#include <dal/utilities/algorithms.hpp>
#include <dal/utilities/numerics.hpp>

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
        // Unified Krylov state for CG (biConjugate = false) and BCG (true). For CG, zzRef/ppRef
        // alias the real vectors so the shared arithmetic is byte-identical to standalone CG.
        struct KrylovState_ {
            const Sparse::Square_& A;
            const XPrecondition_& precondition;
            const bool biConjugate;
            Vector_<> r;
            Vector_<> rr;
            Vector_<> z;
            Vector_<> zz;
            Vector_<> p;
            Vector_<> pp;
            Vector_<>& zzRef; // aliases z for CG, zz for BCG
            Vector_<>& ppRef; // aliases p for CG, pp for BCG
            double betaPrev;

            KrylovState_(const Sparse::Square_& a, const XPrecondition_& prec, bool biConjugate, int n)
                : A(a), precondition(prec), biConjugate(biConjugate), r(n), rr(biConjugate ? n : 0), z(n), zz(biConjugate ? n : 0), p(n),
                  pp(biConjugate ? n : 0), zzRef(biConjugate ? zz : z), ppRef(biConjugate ? pp : p), betaPrev(0.0) {}
        };

        double PrepareDirection_(KrylovState_& s, int ii) {
            s.precondition.Left(s.r, &s.z);
            if (s.biConjugate)
                s.precondition.Right(s.rr, &s.zz);
            const double beta = InnerProduct(s.zzRef, s.r);
            const double multiply = ii > 0 ? beta / s.betaPrev : 0.0;
            // Fused AXPY sweep mirroring the unfused operation order (p *= multiply; p += z)
            // so -ffp-contract=fast forms the same FMAs as the original two-pass code.
            {
                auto pIt = s.p.begin();
                auto zIt = s.z.begin();
                const auto pEnd = s.p.end();
                if (s.biConjugate) {
                    auto ppIt = s.pp.begin();
                    auto zzIt = s.zz.begin();
                    for (; pIt != pEnd; ++pIt, ++zIt, ++ppIt, ++zzIt) {
                        *pIt = *pIt * multiply + *zIt;
                        *ppIt = *ppIt * multiply + *zzIt;
                    }
                } else {
                    for (; pIt != pEnd; ++pIt, ++zIt)
                        *pIt = *pIt * multiply + *zIt;
                }
            }
            s.betaPrev = beta;
            return beta;
        }

        void UpdateSolution_(KrylovState_& s, double beta, Vector_<>* x) {
            s.A.MultiplyLeft(s.p, &s.z);
            if (s.biConjugate)
                s.A.MultiplyRight(s.pp, &s.zz);
            const double alphaK = beta / InnerProduct(s.z, s.ppRef);
            // Fused AXPY sweep mirroring LinearIncrement(scale) = dst + scale * src, so the
            // FMA pattern matches the unfused Transform calls (r + (-alphaK)*z, not r - alphaK*z).
            const double negAlphaK = -alphaK;
            {
                auto xIt = x->begin();
                auto pIt = s.p.begin();
                auto rIt = s.r.begin();
                auto zIt = s.z.begin();
                const auto xEnd = x->end();
                if (s.biConjugate) {
                    auto rrIt = s.rr.begin();
                    auto zzIt = s.zz.begin();
                    for (; xIt != xEnd; ++xIt, ++pIt, ++rIt, ++zIt, ++rrIt, ++zzIt) {
                        *xIt = *xIt + alphaK * *pIt;
                        *rIt = *rIt + negAlphaK * *zIt;
                        *rrIt = *rrIt + negAlphaK * *zzIt;
                    }
                } else {
                    for (; xIt != xEnd; ++xIt, ++pIt, ++rIt, ++zIt) {
                        *xIt = *xIt + alphaK * *pIt;
                        *rIt = *rIt + negAlphaK * *zIt;
                    }
                }
            }
        }

        void ValidateKrylovParams_(int n, const Vector_<>& b, const Vector_<>* x, double tolRel, double tolAbs, int maxIterations) {
            REQUIRE(b.size() == n && x->size() == n, "matrix dimensions are incompatible");
            REQUIRE((IsPositive(tolRel) || IsPositive(tolAbs)) && maxIterations > 0, "parameters are invalid");
        }

        void
        KrylovSolve_(const Sparse::Square_& A, const Vector_<>& b, double tolRel, double tolAbs, int maxIterations, bool biConjugate, Vector_<>* x) {
            const int n = A.Size();
            ValidateKrylovParams_(n, b, x, tolRel, tolAbs, maxIterations);

            const double tNorm = tolRel * sqrt(InnerProduct(b, b)) + tolAbs;
            XPrecondition_ precondition(A);
            KrylovState_ s(A, precondition, biConjugate, n);

            A.MultiplyLeft(*x, &s.r);
            Transform(b, s.r, std::minus<>(), &s.r); // r = b - Ax
            if (biConjugate)
                s.rr = s.r;

            for (int ii = 0; ii < maxIterations; ++ii) {
                const double beta = PrepareDirection_(s, ii);
                UpdateSolution_(s, beta, x);
                if (sqrt(InnerProduct(s.r, s.r)) <= tNorm)
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

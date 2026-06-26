//
// Created by wegam on 2022/12/10.
//

#include <functional>

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/math/optimization/underdetermined.hpp>
#include <dal/math/matrix/bcg.hpp>
#include <dal/math/matrix/cholesky.hpp>
#include <dal/math/matrix/matrixarithmetic.hpp>
#include <dal/math/matrix/matrixs.hpp>
#include <dal/math/matrix/sparse.hpp>
#include <dal/math/matrix/squarematrix.hpp>
#include <dal/utilities/functionals.hpp>
#include <dal/utilities/numerics.hpp>

namespace Dal {

#include <dal/auto/MG_UnderdeterminedControls_object.inc>

    namespace Underdetermined {
        void Function_::Gradient(const Vector_<>& x, const Vector_<>& f, Matrix_<>* j) const {
            Vector_<> fBase;
            Vector_<> fBumped;
            FFast(x, &fBase);
            Vector_<> xBumped(x);
            const double dx = BumpSize();
            auto scale = [&dx](double x) { return 1.0 / dx * x; };
            const auto nx = x.size();
            j->Resize(f.size(), nx);
            for (int ix = 0; ix < nx; ++ix) {
                xBumped[ix] += dx;
                FFast(xBumped, &fBumped);
                fBumped -= fBase;
                auto col = j->Col(ix);
                Transform(fBumped, scale, &col);
                xBumped[ix] = x[ix];
            }
        }
    } // namespace Underdetermined

    namespace {
        struct XJDense_ : Underdetermined::Jacobian_ {
            Matrix_<>& j_;
            explicit XJDense_(Matrix_<>& j) : j_(j) {}

            [[nodiscard]] int Rows() const override { return j_.Rows(); }
            [[nodiscard]] int Columns() const override { return j_.Cols(); }

            void DivideRows(const Vector_<>& tol) override {
                for (int ii = 0; ii < j_.Rows(); ++ii) {
                    auto row = j_.Row(ii);
                    Transform(&row, [&tol, &ii](double x) { return 1.0 / tol[ii] * x; });
                }
            }

            [[nodiscard]] Vector_<> MultiplyRight(const Vector_<>& t) const override {
                Vector_<> retVal;
                Matrix::Multiply(t, j_, &retVal);
                return retVal;
            }
            [[nodiscard]] Vector_<> MultiplyLeft(const Vector_<>& dx) const override {
                Vector_<> retVal;
                Matrix::Multiply(j_, dx, &retVal);
                return retVal;
            }

            void QForm(const Sparse::SymmetricDecomposition_& w, SquareMatrix_<>* form) const override {
                w.QForm(j_, form);
            }

            void SecantUpdate(const Vector_<>& dx, const Vector_<>& df) override {
                const auto nf = df.size();
                const double x2 = InnerProduct(dx, dx);
                for (int ii = 0; ii < nf; ++ii) {
                    auto row = j_.Row(ii);
                    const double excess = df[ii] - InnerProduct(dx, row);
                    Transform(&row, dx, LinearIncrement(excess / x2));
                }
            }
        };

        struct XScaledFunc_ {
            const Vector_<>& tol_;
            const Underdetermined::Function_& func_;
            int nEvals_;
            int nRestarts_;
            Matrix_<> jDense_;

            XScaledFunc_(const Vector_<>& tol,
                         const Underdetermined::Function_& func,
                         const Underdetermined::Controls_& controls)
                : tol_(tol), func_(func), nEvals_(controls.maxEvaluations_), nRestarts_(controls.maxRestarts_) {}

            Vector_<> F(const Vector_<>& x) {
                REQUIRE(nEvals_-- > 0, "Exhausted function evaluations in underdetermined search");
                Vector_<> retVal = func_.F(x);
                Transform(&retVal, tol_, std::divides<>());
                return retVal;
            }

            Underdetermined::Jacobian_* J(const Vector_<>& x, const Vector_<>& f) {
                REQUIRE(nRestarts_-- > 0, "Exhausted gradient evaluations in underdetermined search");
                if (auto sparse = func_.Gradient(x, f)) {
                    sparse->DivideRows(tol_);
                    return sparse;
                }
                func_.Gradient(x, f, &jDense_);
                std::unique_ptr<XJDense_> retVal(new XJDense_(jDense_));
                retVal->DivideRows(tol_);
                return retVal.release();
            }
        };

        void QPStep(const Vector_<>& f,
                    const Underdetermined::Jacobian_& j,
                    const Sparse::SymmetricDecomposition_& w,
                    SquareMatrix_<>* q,
                    Vector_<>* s) {
            j.QForm(w, q);
            Vector_<Vector_<>> qf(1, f);
            qf[0] *= -1.0;
            CholeskySolve(q, &qf);
            Vector_<> ws = j.MultiplyRight(qf[0]);
            w.Solve(ws, s);
        }

        void StoreEffectiveJacobianInverse(const Underdetermined::Jacobian_& j,
                                           const Sparse::SymmetricDecomposition_& w,
                                           Matrix_<>* effJInv) {
            if (!effJInv)
                return;
            SquareMatrix_<> q;
            j.QForm(w, &q);
            Vector_<Vector_<>> qInvRhs(j.Rows(), Vector_<>(j.Rows(), 0.0));
            for (int i = 0; i < j.Rows(); ++i)
                qInvRhs[i][i] = 1.0;
            CholeskySolve(&q, &qInvRhs);

            effJInv->Resize(j.Columns(), j.Rows());
            for (int iCol = 0; iCol < j.Rows(); ++iCol) {
                Vector_<> ws = j.MultiplyRight(qInvRhs[iCol]);
                Vector_<> effCol;
                w.Solve(ws, &effCol);
                for (int iRow = 0; iRow < j.Columns(); ++iRow)
                    (*effJInv)(iRow, iCol) = effCol[iRow];
            }
        }

        void StoreForwardJacobianAtSolution(const Underdetermined::Jacobian_& jac, Matrix_<>* out) {
            out->Resize(jac.Rows(), jac.Columns());
            for (int k = 0; k < jac.Columns(); ++k) {
                Vector_<> ek(jac.Columns(), 0.0);
                ek[k] = 1.0;
                const Vector_<> col = jac.MultiplyLeft(ek);
                for (int i = 0; i < jac.Rows(); ++i)
                    (*out)(i, k) = col[i];
            }
        }

        class XPenaltyWeight_ : public Sparse::Square_ {
            const Sparse::Square_& W_; // must be symmetric
            const Underdetermined::Jacobian_& J_;
            double jWeight_;

            [[nodiscard]] int Size() const override { return W_.Size(); }
            void MultiplyLeft(const Vector_<>& x, Vector_<>* b) const override {
                W_.MultiplyLeft(x, b);
                const Vector_<> Js = J_.MultiplyLeft(x);
                Transform(b, J_.MultiplyRight(Js), LinearIncrement(jWeight_));
            }

            void MultiplyRight(const Vector_<>& x, Vector_<>* b) const override { MultiplyLeft(x, b); }
            [[nodiscard]] bool IsSymmetric() const override {
                REQUIRE(W_.IsSymmetric(), "W_ must be symmetric");
                return true;
            }

            [[nodiscard]] Sparse::SymmetricDecomposition_* Decompose() const override;

            const double& operator()(int iRow, int iCol) const override {
                THROW("Penalty weight element access is not supported");
            }

            void Set(int iRow, int iCol, double val) override {
                THROW("Penalty weight element setting is not possible");
            }

        public:
            XPenaltyWeight_(const Sparse::Square_& W, const Underdetermined::Jacobian_& J, double j_weight)
                : W_(W), J_(J), jWeight_(j_weight) {}
        };

        class XDecompByCG_ : public Sparse::SymmetricDecomposition_ {
            const Sparse::Square_& A_; // must be symmetric
        public:
            explicit XDecompByCG_(const Sparse::Square_& A) : A_(A) {}

            void XMultiply_af(const Vector_<>& x, Vector_<>* b) const override { A_.MultiplyLeft(x, b); }
            Vector_<>::const_iterator MakeCorrelated(Vector_<>::const_iterator, Vector_<>*) const override {
                THROW("Correlation by penalty weight is not supported");
            }

            [[nodiscard]] int Size() const override { return A_.Size(); }

            void XSolve_af(const Vector_<>& b, Vector_<>* x) const override {
                static const double TOL_REL = 0.00001;
                const double tol_abs = sqrt(b.size());
                const int iterations = 200 + AsInt(8.0 * sqrt(b.size()));
                x->Resize(b.size());
                x->Fill(0.0); // no real basis for a guess
                Sparse::CGSolve(A_, b, TOL_REL, tol_abs, iterations, x);
            }
        };

        Sparse::SymmetricDecomposition_* XPenaltyWeight_::Decompose() const { return new XDecompByCG_(*this); }

        Vector_<> ApproxQPStep(const Vector_<>& x0,
                               const Vector_<>& x,
                               const Vector_<>& f,
                               const Underdetermined::Jacobian_& j,
                               double j_weight,
                               const Sparse::Square_& w) {
            XPenaltyWeight_ WJJ(w, j, j_weight);
            scoped_ptr<Sparse::SymmetricDecomposition_> decomp(WJJ.DecomposeSymmetric());
            Vector_<> rhs, retVal;
            w.MultiplyLeft(Apply(std::minus<>(), x0, x), &rhs);
            Transform(&rhs, j.MultiplyRight(f), LinearIncrement(-j_weight));
            decomp->Solve(rhs, &retVal);
            return retVal;
        }
    } // namespace

    Vector_<> Underdetermined::Find(const Function_& funcIn,
                                    const Vector_<>& guess,
                                    const Vector_<>& tol,
                                    const Sparse::SymmetricDecomposition_& w,
                                    const Controls_& controls,
                                    Matrix_<>* effJInv,
                                    Matrix_<>* fwdJacobianAtSolution) {
        XScaledFunc_ func(tol, funcIn, controls);
        Vector_<> xOld(guess);
        Vector_<> fOld = func.F(xOld);
        std::unique_ptr<Jacobian_> j;
        Vector_<> xNew(xOld.size()), s(xOld.size());
        SquareMatrix_<> q;

        bool approxJ = false, restart = true;
        for (;;) {
            if (restart) {
                j.reset(func.J(xOld, fOld));
                approxJ = false;
                restart = false;
            }
            QPStep(fOld, *j, w, &q, &s);
            bool tookStep = false;
            for (int iBacktrack = 0; iBacktrack < controls.maxBacktrackTries_; ++iBacktrack) {
                Transform(xOld, s, std::plus<>(), &xNew);
                Vector_<> fNew = func.F(xNew);
                if (*MaxElement(fNew) < 1.0 && *MinElement(fNew) > -1.0) {
                    StoreEffectiveJacobianInverse(*j, w, effJInv);
                    if (fwdJacobianAtSolution) {
                        fwdJacobianAtSolution->Clear();
                        Vector_<> fUnscaled = fNew;
                        Transform(&fUnscaled, tol, std::multiplies<>());
                        std::unique_ptr<Jacobian_> jSol(funcIn.Gradient(xNew, fUnscaled));
                        if (jSol)
                            StoreForwardJacobianAtSolution(*jSol, fwdJacobianAtSolution);
                    }
                    return xNew;
                }

                const double oldOld = InnerProduct(fOld, fOld);
                const double oldNew = InnerProduct(fOld, fNew);
                const double newNew = InnerProduct(fNew, fNew);
                double kMin = (newNew - 0.5 * oldNew) / (newNew - oldNew + oldOld);
                if (kMin < controls.backtrackTolerance_) {
                    if (!restart) {
                        j->SecantUpdate(s, Apply(std::minus<>(), fNew, fOld));
                        approxJ = true;
                    }
                    xOld = xNew;
                    fOld = fNew;
                    tookStep = true;
                    break;
                }
                if (kMin > controls.restartTolerance_)
                    restart = true;
                double k = min(controls.maxBacktrack_, min(kMin, 2 * (kMin - controls.backtrackTolerance_)));
                assert(k > 0.0);
                s *= 1.0 - k;
            }
            REQUIRE(tookStep || approxJ, "Could not find a descent direction in underdetermined search");
        }
    }

    Vector_<> Underdetermined::Approximate(const Function_& funcIn,
                                           const Vector_<>& guess,
                                           const Vector_<>& funcTol, // accuracy of function evaluation
                                           double fitTol,            // accuracy of approximate fit
                                           const Sparse::Square_& w,
                                           const Controls_& controls) {
        XScaledFunc_ func(funcTol, funcIn, controls);
        Vector_<> xOld(guess);
        Vector_<> fOld = func.F(xOld);
        if (sqrt(InnerProduct(fOld, fOld)) <= fitTol)
            return xOld;
        std::unique_ptr<Jacobian_> j;
        Vector_<> xNew(xOld.size());
        SquareMatrix_<> q;
        const double jWeight = InnerProduct(funcTol, funcTol) / Square(fitTol);

        for (int ie = 3, ticker = 0; ie < controls.maxEvaluations_; ++ie, ticker -= controls.maxRestarts_) {
            if (ticker <= 0) {
                j.reset(func.J(xOld, fOld));
                ticker = controls.maxEvaluations_;
                ++ie; // we used up an evaluation too
            }
            Vector_<> s = ApproxQPStep(guess, xOld, fOld, *j, jWeight, w);
            // POSTPONED -- stop early when step is very small
            Transform(xOld, s, std::plus<>(), &xNew);
            Vector_<> fNew = func.F(xNew);
            if (sqrt(InnerProduct(fNew, fNew)) <= fitTol)
                return xNew;
            if (ticker > controls.maxRestarts_) // we are not about to restart
                j->SecantUpdate(s, Apply(std::minus<>(), fNew, fOld));
            xOld = xNew;
            fOld = fNew;
        }
        return xOld;
    }
} // namespace Dal

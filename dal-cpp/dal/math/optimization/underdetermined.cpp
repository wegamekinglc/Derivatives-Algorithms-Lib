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
                if (x2 == 0.0) // a zero step carries no secant information
                    return;
                for (int ii = 0; ii < nf; ++ii) {
                    auto row = j_.Row(ii);
                    const double excess = df[ii] - Math::Dot(Math::DoubleData(dx), &*row.begin(), row.size());
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
                if (nEvals_-- <= 0)
                    THROW2("Exhausted function evaluations in underdetermined search", Underdetermined::ConvergenceError_);
                Vector_<> retVal = func_.F(x);
                Transform(&retVal, tol_, std::divides<>());
                return retVal;
            }

            std::unique_ptr<Underdetermined::Jacobian_> J(const Vector_<>& x, const Vector_<>& f) {
                if (nRestarts_-- <= 0)
                    THROW2("Exhausted gradient evaluations in underdetermined search", Underdetermined::ConvergenceError_);
                return BuildJacobian(x, f);
            }

            std::unique_ptr<Underdetermined::Jacobian_> JAtSolution(const Vector_<>& x, const Vector_<>& scaledF) {
                Vector_<> unscaledF = scaledF;
                Transform(&unscaledF, tol_, std::multiplies<>());
                return BuildJacobian(x, unscaledF);
            }

            std::unique_ptr<Underdetermined::Jacobian_> BuildJacobian(const Vector_<>& x, const Vector_<>& f) {
                if (auto sparse = func_.Gradient(x, f)) {
                    sparse->DivideRows(tol_);
                    return sparse;
                }
                func_.Gradient(x, f, &jDense_);
                auto retVal = std::make_unique<XJDense_>(jDense_);
                retVal->DivideRows(tol_);
                return retVal;
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

            [[nodiscard]] std::unique_ptr<SquareMatrixDecomposition_> Decompose() const override;

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

        std::unique_ptr<SquareMatrixDecomposition_> XPenaltyWeight_::Decompose() const { return std::make_unique<XDecompByCG_>(*this); }

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

        struct FindState_ {
            Vector_<> xOld_;
            Vector_<> fOld_;
            std::unique_ptr<Underdetermined::Jacobian_> jacobian_;
            Vector_<> xNew_;
            Vector_<> step_;
            bool approximateJacobian_ = false;
            bool restart_ = true;

            FindState_(const Vector_<>& guess, XScaledFunc_* func) : xOld_(guess), fOld_(func->F(xOld_)), xNew_(xOld_.size()), step_(xOld_.size()) {}
        };

        struct FindBacktrackResult_ {
            bool solved_ = false;
            bool tookStep_ = false;
        };

        bool IsFindSolution(const Vector_<>& f) { return *MaxElement(f) < 1.0 && *MinElement(f) > -1.0; }

        // Fail-closed inverse validation: the tolerance-scaled solution Jacobian must map each
        // inverse column back to the identity to 1e-7; a finite-but-wrong inverse fails closed.
        bool ValidSolutionMapping(const Underdetermined::Jacobian_& scaledJacobian, const Matrix_<>& inverse) {
            for (int column = 0; column < inverse.Cols(); ++column) {
                Vector_<> direction(inverse.Rows());
                for (int row = 0; row < inverse.Rows(); ++row)
                    direction[row] = inverse(row, column);
                const auto mapped = scaledJacobian.MultiplyLeft(direction);
                for (int row = 0; row < static_cast<int>(mapped.size()); ++row)
                    if (!std::isfinite(mapped[row]) || std::abs(mapped[row] - (row == column ? 1.0 : 0.0)) > 1.0e-7)
                        return false;
            }
            return !inverse.Empty();
        }

        void StoreFindDiagnostics(XScaledFunc_* func,
                                  const Underdetermined::Function_& funcIn,
                                  const Vector_<>& x,
                                  const Vector_<>& scaledF,
                                  const Vector_<>& tol,
                                  const Sparse::SymmetricDecomposition_& w,
                                  Matrix_<>* effJInv,
                                  Matrix_<>* fwdJacobianAtSolution) {
            if (effJInv) {
                std::unique_ptr<Underdetermined::Jacobian_> jSolution(func->JAtSolution(x, scaledF));
                StoreEffectiveJacobianInverse(*jSolution, w, effJInv);
                if (!ValidSolutionMapping(*jSolution, *effJInv))
                    effJInv->Clear();
            }
            if (fwdJacobianAtSolution) {
                fwdJacobianAtSolution->Clear();
                Vector_<> fUnscaled = scaledF;
                Transform(&fUnscaled, tol, std::multiplies<>());
                std::unique_ptr<Underdetermined::Jacobian_> jSolution(funcIn.Gradient(x, fUnscaled));
                if (jSolution)
                    StoreForwardJacobianAtSolution(*jSolution, fwdJacobianAtSolution);
            }
        }

        double BacktrackMinimum(const Vector_<>& fOld, const Vector_<>& fNew) {
            const double oldOld = InnerProduct(fOld, fOld);
            const double oldNew = InnerProduct(fOld, fNew);
            const double newNew = InnerProduct(fNew, fNew);
            const double denominator = oldOld - 2.0 * oldNew + newNew;
            return denominator > 0.0 ? (newNew - oldNew) / denominator : 1.0;
        }

        void AcceptFindStep(const Vector_<>& fNew, FindState_* state) {
            if (!state->restart_) {
                state->jacobian_->SecantUpdate(state->step_, Apply(std::minus<>(), fNew, state->fOld_));
                state->approximateJacobian_ = true;
            }
            state->xOld_ = state->xNew_;
            state->fOld_ = fNew;
        }

        FindBacktrackResult_ BacktrackFindStep(XScaledFunc_* func,
                                               const Underdetermined::Function_& funcIn,
                                               const Vector_<>& tol,
                                               const Sparse::SymmetricDecomposition_& w,
                                               const Underdetermined::Controls_& controls,
                                               Matrix_<>* effJInv,
                                               Matrix_<>* fwdJacobianAtSolution,
                                               FindState_* state) {
            for (int iBacktrack = 0; iBacktrack < controls.maxBacktrackTries_; ++iBacktrack) {
                Transform(state->xOld_, state->step_, std::plus<>(), &state->xNew_);
                const Vector_<> fNew = func->F(state->xNew_);
                if (IsFindSolution(fNew)) {
                    StoreFindDiagnostics(func, funcIn, state->xNew_, fNew, tol, w, effJInv, fwdJacobianAtSolution);
                    return {true, false};
                }

                const double kMin = BacktrackMinimum(state->fOld_, fNew);
                if (kMin < controls.backtrackTolerance_) {
                    AcceptFindStep(fNew, state);
                    return {false, true};
                }
                if (kMin > controls.restartTolerance_)
                    state->restart_ = true;
                const double k = min(controls.maxBacktrack_, min(kMin, 2 * (kMin - controls.backtrackTolerance_)));
                REQUIRE(k > 0.0, "backtrack step must be positive");
                state->step_ *= 1.0 - k;
            }
            return {};
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
        FindState_ state(guess, &func);
        SquareMatrix_<> q;

        for (;;) {
            if (state.restart_) {
                state.jacobian_ = func.J(state.xOld_, state.fOld_);
                state.approximateJacobian_ = false;
                state.restart_ = false;
            }
            QPStep(state.fOld_, *state.jacobian_, w, &q, &state.step_);
            const FindBacktrackResult_ backtrack = BacktrackFindStep(&func, funcIn, tol, w, controls, effJInv, fwdJacobianAtSolution, &state);
            if (backtrack.solved_)
                return state.xNew_;
            if (!(backtrack.tookStep_ || state.approximateJacobian_))
                THROW2("Could not find a descent direction in underdetermined search", ConvergenceError_);
        }
    }

    namespace {
        double FitNorm(const Vector_<>& values) {
            double norm = 0.;
            for (double value : values)
                norm = std::hypot(norm, value);
            return norm;
        }
        bool FitAccepted(const Vector_<>& scaled, const Underdetermined::FitSettings_& settings) {
            double maximum = 0.;
            for (double value : scaled)
                maximum = std::max(maximum, std::abs(value));
            return settings.exact_ ? maximum <= 1. : FitNorm(scaled) <= *settings.fitNormTolerance_;
        }
        void FiniteFitValues(const Vector_<>& values) {
            for (double value : values)
                REQUIRE(std::isfinite(value), "Calibration fit requires finite values");
        }
        void FiniteFitMatrix(const Matrix_<>& matrix) {
            for (int row = 0; row < matrix.Rows(); ++row)
                FiniteFitValues(Vector_<>(matrix.Row(row)));
        }
        void ValidateFitObjective(const Underdetermined::FitSettings_& settings) {
            if (!settings.exact_) {
                REQUIRE(settings.fitWeight_ && std::isfinite(*settings.fitWeight_) && *settings.fitWeight_ > 0.,
                        "Explicit positive fitWeight required");
                REQUIRE(settings.fitNormTolerance_ && std::isfinite(*settings.fitNormTolerance_) && *settings.fitNormTolerance_ > 0.,
                        "Explicit positive fitNormTolerance required");
            }
        }
        void ValidateFit(const Vector_<>& initial,
                         const Vector_<>& scales,
                         const Sparse::Square_& weights,
                         const Underdetermined::FitSettings_& settings) {
            REQUIRE(!initial.empty() && !scales.empty() && weights.Size() == static_cast<int>(initial.size()) && weights.IsSymmetric(),
                    "Calibration fit requires parameters, residual scales and symmetric weights");
            FiniteFitValues(initial);
            for (double scale : scales)
                REQUIRE(std::isfinite(scale) && scale > 0., "Calibration fit requires finite positive row scales");
            REQUIRE(std::isfinite(settings.differenceStep_) && settings.differenceStep_ > 0., "Calibration fit requires a positive FD step");
            ValidateFitObjective(settings);
        }
        struct FitDriver_ {
            const Underdetermined::Function_& function_;
            const Vector_<>& scales_;
            const Underdetermined::Controls_& controls_;
            const Underdetermined::FitSettings_& settings_;
            Underdetermined::FitResult_ result_;

            // Calibration owns evaluation counters and the last fully validated candidate.
            std::optional<Vector_<>> Evaluate(const Vector_<>& x) {
                if (result_.evaluations_ >= controls_.maxEvaluations_)
                    return {};
                FiniteFitValues(x);
                ++result_.evaluations_;
                auto raw = function_.F(x);
                REQUIRE(raw.size() == scales_.size(), "Calibration residual dimension changed");
                FiniteFitValues(raw);
                return raw;
            }
            Vector_<> Scale(const Vector_<>& raw) const {
                auto scaled = raw;
                for (size_t i = 0; i < scaled.size(); ++i)
                    scaled[i] /= scales_[i];
                FiniteFitValues(scaled);
                return scaled;
            }
            void Accept(const Vector_<>& x, const Vector_<>& raw) {
                const auto scaled = Scale(raw);
                result_.parameters_ = x;
                result_.rawResiduals_ = raw;
                result_.scaledResiduals_ = scaled;
                result_.fitWithinTolerance_ = FitAccepted(scaled, settings_);
            }
            bool DifferenceGradient(Matrix_<>* dense) {
                dense->Resize(static_cast<int>(scales_.size()), static_cast<int>(result_.parameters_.size()));
                for (int j = 0; j < dense->Cols(); ++j) {
                    auto up = result_.parameters_, down = up;
                    const double step = settings_.differenceStep_;
                    up[j] += step;
                    down[j] -= step;
                    REQUIRE(up[j] > down[j], "Calibration FD step is not representable");
                    const auto u = Evaluate(up), d = Evaluate(down);
                    if (!u || !d)
                        return false;
                    for (int i = 0; i < dense->Rows(); ++i)
                        (*dense)(i, j) = ((*u)[i] - (*d)[i]) / (up[j] - down[j]);
                }
                return true;
            }
            std::unique_ptr<Underdetermined::Jacobian_> Gradient(Matrix_<>* dense) {
                if (result_.gradients_ >= controls_.maxRestarts_)
                    return nullptr;
                ++result_.gradients_;
                auto jacobian = function_.Gradient(result_.parameters_, result_.rawResiduals_);
                if (!jacobian) {
                    if (!DifferenceGradient(dense))
                        return nullptr;
                    jacobian = std::make_unique<XJDense_>(*dense);
                }
                REQUIRE(jacobian->Rows() == static_cast<int>(scales_.size()) && jacobian->Columns() == static_cast<int>(result_.parameters_.size()),
                        "Calibration Jacobian dimension changed");
                jacobian->DivideRows(scales_);
                Matrix_<> checked;
                StoreForwardJacobianAtSolution(*jacobian, &checked);
                FiniteFitMatrix(checked);
                return jacobian;
            }
        };
        Vector_<> RegularizedFitStep(const Vector_<>& initial,
                                     const Underdetermined::FitResult_& result,
                                     const Underdetermined::Jacobian_& jacobian,
                                     const Sparse::Square_& weights,
                                     double weight) {
            Matrix_<> j;
            StoreForwardJacobianAtSolution(jacobian, &j);
            const int n = weights.Size();
            SquareMatrix_<> normal(n, 0.);
            Vector_<> rhs;
            weights.MultiplyLeft(Apply(std::minus<>(), initial, result.parameters_), &rhs);
            const auto gradient = jacobian.MultiplyRight(result.scaledResiduals_);
            for (int a = 0; a < n; ++a) {
                rhs[a] -= weight * gradient[a];
                for (int b = 0; b < n; ++b) {
                    double value = weights(a, b);
                    for (int i = 0; i < j.Rows(); ++i)
                        value += weight * j(i, a) * j(i, b);
                    REQUIRE(std::isfinite(value), "Nonfinite calibration normal matrix");
                    normal(a, b) = value;
                }
            }
            FiniteFitValues(rhs);
            const auto decomposition = CholeskyDecomposition(normal);
            Vector_<> step;
            decomposition->Solve(rhs, &step);
            return step;
        }
        bool AdvanceFit(FitDriver_* driver, const Vector_<>& step) {
            FiniteFitValues(step);
            const auto& previous = driver->result_;
            double fraction = 1.;
            for (int attempt = 0; attempt < driver->controls_.maxBacktrackTries_; ++attempt) {
                auto candidate = previous.parameters_;
                for (size_t j = 0; j < candidate.size(); ++j)
                    candidate[j] += fraction * step[j];
                const auto raw = driver->Evaluate(candidate);
                if (!raw)
                    return false;
                const auto scaled = driver->Scale(*raw);
                if (FitAccepted(scaled, driver->settings_) || !driver->settings_.exact_ || FitNorm(scaled) < FitNorm(previous.scaledResiduals_)) {
                    driver->Accept(candidate, *raw);
                    return true;
                }
                fraction *= .5;
            }
            driver->result_.termination_ = "CalibrationFitFailed";
            return false;
        }
        bool TakeFitStep(const Vector_<>& initial, const Sparse::Square_& weights, FitDriver_* driver) {
            Matrix_<> dense;
            const auto jacobian = driver->Gradient(&dense);
            if (!jacobian)
                return false;
            Vector_<> step;
            if (driver->settings_.exact_) {
                const std::unique_ptr<Sparse::SymmetricDecomposition_> decomposition(weights.DecomposeSymmetric());
                SquareMatrix_<> form;
                QPStep(driver->result_.scaledResiduals_, *jacobian, *decomposition, &form, &step);
            } else {
                step = RegularizedFitStep(initial, driver->result_, *jacobian, weights, *driver->settings_.fitWeight_);
            }
            return AdvanceFit(driver, step);
        }
    } // namespace

    Underdetermined::FitResult_ Underdetermined::Fit(const Function_& function,
                                                     const Vector_<>& initial,
                                                     const Vector_<>& scales,
                                                     const Sparse::Square_& weights,
                                                     const Controls_& controls,
                                                     const FitSettings_& settings) {
        ValidateFit(initial, scales, weights, settings);
        FitDriver_ driver{function, scales, controls, settings, {}};
        driver.result_.termination_ = "CalibrationBudgetExceeded";
        try {
            const auto raw = driver.Evaluate(initial);
            if (raw)
                driver.Accept(initial, *raw);
            while (raw && !driver.result_.fitWithinTolerance_) {
                if (!TakeFitStep(initial, weights, &driver))
                    break;
            }
            if (driver.result_.fitWithinTolerance_)
                driver.result_.termination_ = "FitWithinTolerance";
        } catch (...) {
            driver.result_.cause_ = std::current_exception();
            driver.result_.termination_ = "CalibrationNumericalFailure";
        }
        return driver.result_;
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
                j = func.J(xOld, fOld);
                ticker = controls.maxEvaluations_;
                ++ie; // we used up an evaluation too
            }
            Vector_<> s = ApproxQPStep(guess, xOld, fOld, *j, jWeight, w);
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

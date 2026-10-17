//
// Created by dal-implementer on 2026/7/8.
//

#include <algorithm>
#include <array>

#include <dal/math/pde/pdegrid.hpp>
#include <dal/math/pde/pdeoperators.hpp>
#include <dal/math/pde/thetascheme.hpp>
#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/utilities/exceptions.hpp>

namespace Dal::PDE {
    namespace {
        void RequireOneDimensionalGrid(const Vector_<CoordinateVector_>& xPoints) {
            REQUIRE(xPoints.size() == 1, "ThetaScheme_ supports exactly one spatial dimension");
        }

        bool SameGrid(const CoordinateVector_& lhs, const CoordinateVector_& rhs) {
            return lhs.yLow_ == rhs.yLow_ && lhs.yHigh_ == rhs.yHigh_ && lhs.n_ == rhs.n_ && lhs.yToX_.get() == rhs.yToX_.get();
        }

        bool ScalarIndependent(const ScalarCoeff_& coeff) { return !coeff.XDependence().any(); }

        bool VectorIndependent(const VectorCoeff_& coeff) {
            for (const auto& dep : coeff.XDependence())
                if (dep.any())
                    return false;
            return true;
        }

        bool MatrixIndependent(const MatrixCoeff_& coeff) {
            const Matrix_<Coeff_::x_dep_t> dep = coeff.XDependence();
            for (int i = 0; i < dep.Rows(); ++i)
                for (int j = 0; j < dep.Cols(); ++j)
                    if (dep(i, j).any())
                        return false;
            return true;
        }

        struct CoefficientScratch_ {
            Vector_<> where_{0.0};
            Vector_<> muValue_;
        };

        void SampleAt(const ScalarCoeff_& discounting,
                      const VectorCoeff_& advection,
                      const MatrixCoeff_& diffusion,
                      double x,
                      CoefficientScratch_* scratch,
                      double* r,
                      double* mu,
                      double* var) {
            scratch->where_[0] = x;
            scratch->muValue_.clear();
            SquareMatrix_<> varValue;
            discounting.Value(scratch->where_, r);
            advection.Value(scratch->where_, &scratch->muValue_);
            diffusion.Value(scratch->where_, &varValue);
            REQUIRE(scratch->muValue_.size() == 1 && varValue.Rows() == 1 && varValue.Cols() == 1,
                    "advection/diffusion shape must match one spatial dimension");
            *mu = scratch->muValue_[0];
            *var = varValue(0, 0);
        }

        void SampleCoefficients(const Vector_<>& x,
                                const ScalarCoeff_& discounting,
                                const VectorCoeff_& advection,
                                const MatrixCoeff_& diffusion,
                                Vector_<>* r,
                                Vector_<>* mu,
                                Vector_<>* var) {
            const int n = static_cast<int>(x.size());
            r->Resize(n);
            mu->Resize(n);
            var->Resize(n);

            double r0 = 0.0;
            double mu0 = 0.0;
            double var0 = 0.0;
            CoefficientScratch_ scratch;
            if (ScalarIndependent(discounting) && VectorIndependent(advection) && MatrixIndependent(diffusion)) {
                SampleAt(discounting, advection, diffusion, x[0], &scratch, &r0, &mu0, &var0);
                r->Fill(r0);
                mu->Fill(mu0);
                var->Fill(var0);
                return;
            }

            for (int i = 0; i < n; ++i)
                SampleAt(discounting, advection, diffusion, x[i], &scratch, &(*r)[i], &(*mu)[i], &(*var)[i]);
        }

        std::array<double, 9>
        ProbeSamples(const Vector_<>& x, const ScalarCoeff_& discounting, const VectorCoeff_& advection, const MatrixCoeff_& diffusion) {
            const int n = static_cast<int>(x.size());
            const int probes[3] = {0, n / 2, n - 1};
            std::array<double, 9> samples{};
            CoefficientScratch_ scratch;
            for (int i = 0; i < 3; ++i)
                SampleAt(discounting, advection, diffusion, x[probes[i]], &scratch, &samples[3 * i], &samples[3 * i + 1], &samples[3 * i + 2]);
            return samples;
        }

        void RequireShape(const Cube_<>& values, int n) {
            REQUIRE(values.SizeI() == 1 && values.SizeJ() == 1 && values.SizeK() == n, "value layer must have shape (1, 1, n)");
        }

        void EnsureTargetLayer(Vector_<std::shared_ptr<Cube_<>>>* vals, size_t layer, int n) {
            std::shared_ptr<Cube_<>>& target = (*vals)[layer];
            if (!target || target->SizeI() != 1 || target->SizeJ() != 1 || target->SizeK() != n)
                target = std::make_shared<Cube_<>>(1, 1, n);
        }

        void TargetBoundaries(const Vector_<std::shared_ptr<Cube_<>>>& oldVals,
                              const Vector_<std::shared_ptr<Cube_<>>>& newVals,
                              size_t layer,
                              int n,
                              double* left,
                              double* right) {
            const std::shared_ptr<Cube_<>>& target = newVals[layer];
            if (target && target->SizeI() == 1 && target->SizeJ() == 1 && target->SizeK() == n) {
                *left = (*target)(0, 0, 0);
                *right = (*target)(0, 0, n - 1);
                return;
            }
            *left = (*oldVals[layer])(0, 0, 0);
            *right = (*oldVals[layer])(0, 0, n - 1);
        }
    } // namespace

    ThetaScheme_::ThetaScheme_(double theta) : theta_(theta) { REQUIRE(theta >= 0.0 && theta <= 1.0, "theta must be in [0, 1]"); }

    void ThetaScheme_::Prepare(double dt,
                               const Vector_<CoordinateVector_>& xPoints,
                               const ScalarCoeff_& discounting,
                               const VectorCoeff_& advection,
                               const MatrixCoeff_& diffusion) {
        REQUIRE(dt > 0.0, "Prepare requires dt > 0");
        RequireOneDimensionalGrid(xPoints);

        points_ = xPoints[0];
        x_ = GridLocations(points_);
        discounting_ = &discounting;
        advection_ = &advection;
        diffusion_ = &diffusion;
        preparedDt_ = dt;

        Vector_<> r;
        Vector_<> mu;
        Vector_<> var;
        SampleCoefficients(x_, discounting, advection, diffusion, &r, &mu, &var);
        const auto samples = ProbeSamples(x_, discounting, advection, diffusion);
        probeSamples_ = Vector_<>(samples.begin(), samples.end());

        std::unique_ptr<Sparse::TriDiagonal_> dx(NewDx(x_));
        std::unique_ptr<Sparse::TriDiagonal_> dxx(NewDxx(x_));
        const int n = static_cast<int>(x_.size());
        explicitOp_ = std::make_unique<Sparse::TriDiagonal_>(n);
        auto implicitOp = std::make_unique<Sparse::TriDiagonal_>(n);

        for (int i = 0; i < n; ++i) {
            if (i == 0 || i == n - 1) {
                explicitOp_->Set(i, i, 1.0);
                implicitOp->Set(i, i, 1.0);
                continue;
            }

            for (int j = i - 1; j <= i + 1; ++j) {
                double generator = mu[i] * (*dx)(i, j) + 0.5 * var[i] * (*dxx)(i, j);
                if (j == i)
                    generator -= r[i];
                const double identity = j == i ? 1.0 : 0.0;
                explicitOp_->Set(i, j, identity + dt * (1.0 - theta_) * generator);
                implicitOp->Set(i, j, identity - dt * theta_ * generator);
            }
        }

        if (theta_ > 0.0) {
            implicitSolve_ = implicitOp->Decompose();
            ++decompositions_;
        } else {
            implicitSolve_.reset();
        }
    }

    void ThetaScheme_::operator()(double dt,
                                  const Vector_<CoordinateVector_>& xPoints,
                                  const Vector_<std::shared_ptr<Cube_<>>>& oldVals,
                                  const ScalarCoeff_& discounting,
                                  const VectorCoeff_& advection,
                                  const MatrixCoeff_& diffusion,
                                  Vector_<std::shared_ptr<Cube_<>>>* newVals) const {
        REQUIRE(preparedDt_ > 0.0 && explicitOp_, "ThetaScheme_ must be Prepared before rolling");
        REQUIRE(dt == preparedDt_, "dt differs from the prepared dt - call Prepare again");
        RequireOneDimensionalGrid(xPoints);
        REQUIRE(SameGrid(xPoints[0], points_), "grid differs from the prepared grid - call Prepare again");
        REQUIRE(&discounting == discounting_ && &advection == advection_ && &diffusion == diffusion_,
                "coefficients differ from those prepared - call Prepare again");
        const auto samples = ProbeSamples(x_, discounting, advection, diffusion);
        REQUIRE(std::equal(samples.begin(), samples.end(), probeSamples_.begin(), probeSamples_.end()),
                "coefficient values differ from those prepared - call Prepare again");
        REQUIRE(!oldVals.empty(), "old_vals must contain at least one value layer");
        REQUIRE(newVals != nullptr, "new_vals must be non-null");

        const int n = static_cast<int>(x_.size());
        for (const auto& layer : oldVals) {
            REQUIRE(layer != nullptr, "old_vals layers must be non-null");
            RequireShape(*layer, n);
        }

        const bool vectorAlias = newVals == &oldVals;
        if (!vectorAlias && newVals->size() != oldVals.size())
            newVals->Resize(oldVals.size());

        for (size_t layer = 0; layer < oldVals.size(); ++layer) {
            const Cube_<>& srcCube = *oldVals[layer];
            double leftBoundary = 0.0;
            double rightBoundary = 0.0;
            TargetBoundaries(oldVals, *newVals, layer, n, &leftBoundary, &rightBoundary);

            Vector_<> src(srcCube.SliceBegin(0, 0), srcCube.SliceEnd(0, 0));
            Vector_<> rhs;
            explicitOp_->MultiplyLeft(src, &rhs);
            rhs[0] = leftBoundary;
            rhs[n - 1] = rightBoundary;

            Vector_<> dst;
            if (theta_ > 0.0)
                implicitSolve_->SolveLeft(rhs, &dst);
            else
                dst = rhs;

            EnsureTargetLayer(newVals, layer, n);
            Cube_<>& target = *(*newVals)[layer];
            for (int k = 0; k < n; ++k)
                target(0, 0, k) = dst[k];
        }
    }
} // namespace Dal::PDE

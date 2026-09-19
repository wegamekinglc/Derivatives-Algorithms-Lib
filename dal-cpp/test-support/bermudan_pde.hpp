//
// Created by dal-implementer on 2026/9/20.
//

#pragma once

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>
#include <vector>

#include <dal/math/interp/interpcubic.hpp>
#include <dal/math/ndarray.hpp>
#include <dal/math/pde/pde.hpp>
#include <dal/math/pde/pdegrid.hpp>
#include <dal/math/pde/thetascheme.hpp>
#include <dal/math/vectors.hpp>
#include <dal/platform/platform.hpp>

//  Test-only Bermudan/American put pricer used as the ground truth of the
//  LSMC acceptance suite: theta-scheme rollback between exercise dates with an
//  obstacle projection V = max(V, K - s) on every exercise date. A single
//  exercise at maturity reproduces the European limit, which the suite checks
//  against the Black closed form together with grid convergence.
namespace Dal::Script::TestSupport {

    namespace Detail {
        inline std::vector<double> PutGridLocations(const PDE::CoordinateVector_& grid) {
            const Vector_<> loc = PDE::GridLocations(grid);
            return std::vector<double>(loc.begin(), loc.end());
        }
    } // namespace Detail

    inline double BermudanPutPDE(
        double spot, double vol, double rate, double div, double strike, const std::vector<double>& exerciseTimes, int numX, int stepsPerInterval) {
        REQUIRE(!exerciseTimes.empty() && exerciseTimes.back() > 0.0, "BermudanPutPDE: exercise times must be non-empty and positive");
        for (size_t i = 1; i < exerciseTimes.size(); ++i)
            REQUIRE(exerciseTimes[i] > exerciseTimes[i - 1], "BermudanPutPDE: exercise times must be strictly increasing");

        const double xMax = 4.0 * std::max(spot, strike);
        const PDE::CoordinateVector_ x = PDE::MakeUniformGrid(0.0, xMax, numX);
        const Vector_<PDE::CoordinateVector_> grids(1, x);
        const std::vector<double> loc = Detail::PutGridLocations(x);
        const auto payoff = [&](double s) { return std::max(strike - s, 0.0); };

        Vector_<std::shared_ptr<Cube_<>>> vals(1, std::make_shared<Cube_<>>(1, 1, numX));
        Vector_<std::shared_ptr<Cube_<>>> next(1, std::make_shared<Cube_<>>(1, 1, numX));
        for (int k = 0; k < numX; ++k)
            (*vals[0])(0, 0, k) = payoff(loc[k]);

        const Handle_<PDE::ScalarCoeff_> disc(PDE::NewConstCoeff(rate));
        const Handle_<PDE::VectorCoeff_> mu(PDE::NewVectorCoeff([&](double s) { return (rate - div) * s; }));
        const Handle_<PDE::MatrixCoeff_> var(PDE::NewMatrixCoeff([&](double s) { return vol * vol * s * s; }));
        PDE::ThetaScheme_ scheme(0.5);

        double tNext = exerciseTimes.back();
        const auto rollTo = [&](double tEx, double nextExercise, bool project) {
            const double interval = tNext - tEx;
            const int steps = std::max(1, stepsPerInterval);
            const double dt = interval / steps;
            scheme.Prepare(dt, grids, *disc, *mu, *var);
            for (int n = 0; n < steps; ++n) {
                const double t = tNext - (n + 1) * dt;
                (*next[0])(0, 0, 0) = strike * std::exp(-rate * (nextExercise - t));
                (*next[0])(0, 0, numX - 1) = 0.0;
                scheme(dt, grids, vals, *disc, *mu, *var, &next);
                vals.Swap(&next);
            }
            if (project)
                for (int k = 0; k < numX; ++k)
                    (*vals[0])(0, 0, k) = std::max((*vals[0])(0, 0, k), payoff(loc[k]));
            tNext = tEx;
        };

        for (size_t e = exerciseTimes.size() - 1; e-- > 0;)
            rollTo(exerciseTimes[e], exerciseTimes[e], true);
        //  Final segment back to the evaluation date: the lower boundary tracks the
        //  next exercise at t_1, and no projection happens on the evaluation date
        rollTo(0.0, exerciseTimes.front(), false);

        const Cube_<>& value = *vals[0];
        const Vector_<> res(value.SliceBegin(0, 0), value.SliceEnd(0, 0));
        Interp::Boundary_ lhs(2, 0.);
        Interp::Boundary_ rhs(2, 0);
        std::unique_ptr<Interp1_> interp(Interp::NewCubic("cubic", Vector_<>(loc.begin(), loc.end()), res, lhs, rhs));
        return (*interp)(spot);
    }
} // namespace Dal::Script::TestSupport

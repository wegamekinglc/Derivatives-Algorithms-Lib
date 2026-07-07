//
// Created by dal-implementer on 2026-6-28.
//
// PDE time-stepping micro-benchmark.
// Prices a European call via ThetaScheme_ with Crank-Nicolson (theta = 0.5),
// 200 space steps and 200 time steps. Each timed iteration rebuilds the grid,
// prepares the operator, and runs the full rollback loop.

#include <dal/platform/platform.hpp>
#include <dal/benchmarks/bench.hpp>
#include <dal/math/pde/pdegrid.hpp>
#include <dal/math/pde/thetascheme.hpp>
#include <dal/math/vectors.hpp>

using namespace Dal;
using namespace Dal::PDE;

namespace {
    constexpr int kSpaceSteps = 200;
    constexpr int kTimeSteps = 200;
    constexpr double kSpot = 100.0;
    constexpr double kStrike = 120.0;
    constexpr double kRate = 0.05;
    constexpr double kDiv = 0.03;
    constexpr double kVol = 0.15;
    constexpr double kT = 3.0;
    constexpr double kTheta = 0.5;
} // namespace

int main() {
    Dal::RegisterAll_::Init();
    constexpr int kRepeats = 10;
    Bench::PrintHeader();

    {
        double sink = 0.0;
        auto r = Bench::Run(
            "ThetaScheme rollback (200x200 CN)",
            [&]() {
                const double minX = 0.0;
                const double maxX = 500.0;
                const int numX = kSpaceSteps + 1;
                const CoordinateVector_ x = MakeUniformGrid(minX, maxX, numX);
                const Vector_<CoordinateVector_> grids(1, x);
                const Vector_<> loc = GridLocations(x);

                Vector_<std::shared_ptr<Cube_<>>> vals(1, std::make_shared<Cube_<>>(1, 1, numX));
                for (int k = 0; k < numX; ++k)
                    (*vals[0])(0, 0, k) = std::max(loc[k] - kStrike, 0.0);
                Vector_<std::shared_ptr<Cube_<>>> next(1, std::make_shared<Cube_<>>(1, 1, numX));

                const Handle_<ScalarCoeff_> disc(NewConstCoeff(kRate));
                const Handle_<VectorCoeff_> mu(NewVectorCoeff([](double s) { return (kRate - kDiv) * s; }));
                const Handle_<MatrixCoeff_> var(NewMatrixCoeff([](double s) { return kVol * kVol * s * s; }));
                ThetaScheme_ scheme(kTheta);
                const double dt = kT / kTimeSteps;
                scheme.Prepare(dt, grids, *disc, *mu, *var);
                for (int n = 0; n < kTimeSteps; ++n) {
                    (*next[0])(0, 0, 0) = 0.0;
                    (*next[0])(0, 0, numX - 1) = maxX * std::exp(-kDiv * (n + 1) * dt) - std::exp(-kRate * (n + 1) * dt) * kStrike;
                    scheme(dt, grids, vals, *disc, *mu, *var, &next);
                    vals.Swap(&next);
                }
                sink += (*vals[0])(0, 0, numX / 2);
            },
            2, kRepeats);
        Bench::Print(r);
        Bench::DoNotOptimize(&sink);
    }

    return 0;
}

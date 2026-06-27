//
// Created by dal-implementer on 2026-6-28.
//
// PDE time-stepping micro-benchmark.
// Prices a European call via FD1D_ with Crank-Nicolson (theta = 0.5),
// 200 space steps and 200 time steps. Each timed iteration rebuilds the
// mesher, inits the FD operator, and runs the full RollBwd loop.

#include <dal/platform/platform.hpp>
#include <dal/math/pde/fd1d.hpp>
#include <dal/math/pde/meshers/uniform1dmesher.hpp>
#include <dal/math/vectors.hpp>
#include <dal/utilities/algorithms.hpp>
#include <dal/benchmarks/bench.hpp>

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
        auto r = Bench::Run("FD1D RollBwd (200x200 CN)", [&]() {
            const double minX = 0.0;
            const double maxX = 500.0;
            const int numX = kSpaceSteps + 1;
            Uniform1DMesher_ x(minX, maxX, numX);
            Vector_<> v0 = Apply([](double s) { return std::max(s - kStrike, 0.0); }, x.Locations());

            FD1D_ fd(x);
            fd.Init();
            fd.Mu() = (kRate - kDiv) * x.Locations();
            fd.R() = Vector_<>(numX, kRate);
            fd.Var() = kVol * kVol * x.Locations() * x.Locations();
            fd.Res() = v0;

            const double dt = kT / kTimeSteps;
            for (int n = 0; n < kTimeSteps; ++n) {
                fd.RollBwd(dt, kTheta, fd.Res());
                fd.Res()[0] = 0.0;
                fd.Res()[fd.Res().size() - 1] = maxX * std::exp(-kDiv * (n + 1) * dt) - std::exp(-kRate * (n + 1) * dt) * kStrike;
            }
            sink += fd.Res()[numX / 2];
        }, 2, kRepeats);
        Bench::Print(r);
        Bench::DoNotOptimize(&sink);
    }

    return 0;
}

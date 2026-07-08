//
// Created by wegam on 2023/2/26.
//

#include <dal/platform/platform.hpp>

#include <dal/math/distribution/black.hpp>
#include <dal/math/interp/interpcubic.hpp>
#include <dal/math/operators.hpp>
#include <dal/math/pde/pdegrid.hpp>
#include <dal/math/pde/thetascheme.hpp>
#include <dal/utilities/timer.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <memory>
#include <string>

using namespace Dal;

namespace {
    constexpr double kMinX = 0.0;
    constexpr double kMaxX = 500.0;
    constexpr double kT = 3.0;
    constexpr double kRate = 0.05;
    constexpr double kDiv = 0.03;
    constexpr double kVol = 0.15;
    constexpr double kStrike = 120.0;
    constexpr double kSpot = 100.0;
    constexpr int kBaseSteps = 250;
    constexpr int kRounds = 20;
    constexpr int kExplicitTimeSteps = 5000;

    struct SchemeRun_ {
        const char* name;
        double theta;
        int spaceSteps;
        int timeSteps;
    };

    struct PriceResult_ {
        double value;
        int64_t elapsedMs;
    };

    PriceResult_ PriceEuropeanCall(const SchemeRun_& run) {
        Timer_ timer;
        const int numX = run.spaceSteps + 1;
        const int numT = run.timeSteps;

        const PDE::CoordinateVector_ x = PDE::MakeUniformGrid(kMinX, kMaxX, numX);
        const Vector_<PDE::CoordinateVector_> grids(1, x);
        const Vector_<> loc = PDE::GridLocations(x);

        Vector_<std::shared_ptr<Cube_<>>> vals(1, std::make_shared<Cube_<>>(1, 1, numX));
        for (int k = 0; k < numX; ++k)
            (*vals[0])(0, 0, k) = std::max(loc[k] - kStrike, 0.0);
        Vector_<std::shared_ptr<Cube_<>>> next(1, std::make_shared<Cube_<>>(1, 1, numX));

        const Handle_<PDE::ScalarCoeff_> disc(PDE::NewConstCoeff(kRate));
        const Handle_<PDE::VectorCoeff_> mu(PDE::NewVectorCoeff([](double s) { return (kRate - kDiv) * s; }));
        const Handle_<PDE::MatrixCoeff_> var(PDE::NewMatrixCoeff([](double s) { return kVol * kVol * s * s; }));
        PDE::ThetaScheme_ scheme(run.theta);
        const double dt = kT / numT;
        scheme.Prepare(dt, grids, *disc, *mu, *var);
        for (int n = 0; n < numT; ++n) {
            (*next[0])(0, 0, 0) = 0.0;
            (*next[0])(0, 0, numX - 1) = kMaxX * std::exp(-kDiv * (n + 1) * dt) - std::exp(-kRate * (n + 1) * dt) * kStrike;
            scheme(dt, grids, vals, *disc, *mu, *var, &next);
            vals.Swap(&next);
        }

        const Cube_<>& value = *vals[0];
        const Vector_<> res(value.SliceBegin(0, 0), value.SliceEnd(0, 0));
        Interp::Boundary_ lhs(2, 0.);
        Interp::Boundary_ rhs(2, 0);
        std::unique_ptr<Interp1_> interp(Interp::NewCubic("cubic", loc, res, lhs, rhs));
        return {(*interp)(kSpot), timer.Elapsed<milliseconds>()};
    }
} // namespace

int main() {
    Dal::RegisterAll_::Init();

    const SchemeRun_ schemeRuns[] = {
        {"Explicit", 0.0, kBaseSteps, kExplicitTimeSteps},
        {"Crank-Nicolson", 0.5, kBaseSteps, kBaseSteps},
        {"Implicit", 1.0, kBaseSteps, kBaseSteps},
    };

    Vector_<int> widths = {18, 20, 14, 14, 14, 14, 14};
    double discounts = std::exp(-kRate * kT);
    double fwd = std::exp((kRate - kDiv) * kT) * kSpot;
    double volStd = std::sqrt(kT) * kVol;
    const auto benchmark = discounts * Distribution::BlackOpt(fwd, volStd, kStrike, OptionType_::Value_::CALL);

    std::cout << std::setw(widths[0]) << std::right << "scheme" << std::setw(widths[1]) << std::right << "grids (x/t)" << std::setw(widths[2])
              << std::right << "spot" << std::setw(widths[3]) << std::right << "price" << std::setw(widths[4]) << std::right << "benchmark"
              << std::setw(widths[5]) << std::right << "Diff (bps)" << std::setw(widths[6]) << std::right << "Elapsed (ms)" << std::endl;

    const auto printRun = [&](const SchemeRun_& run) {
        const PriceResult_ result = PriceEuropeanCall(run);
        const std::string gridLabel = std::to_string(run.spaceSteps + 1) + "/" + std::to_string(run.timeSteps);
        std::cout << std::setw(widths[0]) << std::right << run.name << std::setw(widths[1]) << std::right << gridLabel << std::fixed
                  << std::setw(widths[2]) << std::right << std::setprecision(2) << kSpot << std::setprecision(6) << std::setw(widths[3]) << std::right
                  << result.value << std::setw(widths[4]) << std::right << benchmark << std::setw(widths[5]) << std::right
                  << (result.value - benchmark) / benchmark * 10000 << std::setw(widths[6]) << std::right << result.elapsedMs << std::endl;
    };

    for (const SchemeRun_& run : schemeRuns) {
        printRun(run);
    }

    for (int i = 2; i <= kRounds; ++i) {
        const SchemeRun_ run{"Crank-Nicolson", 0.5, kBaseSteps * i, kBaseSteps * i};
        printRun(run);
    }

    return 0;
}

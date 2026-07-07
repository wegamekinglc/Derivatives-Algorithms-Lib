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
#include <iomanip>

using namespace Dal;

int main() {
    Dal::RegisterAll_::Init();

    double minX = 0.00;
    double maxX = 500.00;
    int steps = 250;
    int nRound = 20;

    double t = 3.00;
    double rate = 0.05;
    double div = 0.03;
    double vol = 0.15;
    double strike = 120.0;
    double spot = 100.0;
    double theta = 0.5;

    Vector_<int> widths = {20, 14, 14, 14, 14, 14};
    double discounts = std::exp(-rate * t);
    double fwd = std::exp((rate - div) * t) * spot;
    double volStd = std::sqrt(t) * vol;
    const auto benchmark = discounts * Distribution::BlackOpt(fwd, volStd, strike, OptionType_::Value_::CALL);

    std::cout << std::setw(widths[0]) << std::right << "# of grids (x/t)" << std::setw(widths[1]) << std::right << "spot" << std::setw(widths[2])
              << std::right << "price" << std::setw(widths[3]) << std::right << "benchmark" << std::setw(widths[4]) << std::right << "Diff (bps)"
              << std::setw(widths[5]) << std::right << "Elapsed (ms)" << std::endl;

    for (int i = 1; i <= nRound; ++i) {
        int numX = steps * i + 1;
        int numT = steps * i;

        Timer_ timer;
        const PDE::CoordinateVector_ x = PDE::MakeUniformGrid(minX, maxX, numX);
        const Vector_<PDE::CoordinateVector_> grids(1, x);
        const Vector_<> loc = PDE::GridLocations(x);

        Vector_<std::shared_ptr<Cube_<>>> vals(1, std::make_shared<Cube_<>>(1, 1, numX));
        for (int k = 0; k < numX; ++k)
            (*vals[0])(0, 0, k) = std::max(loc[k] - strike, 0.0);
        Vector_<std::shared_ptr<Cube_<>>> next(1, std::make_shared<Cube_<>>(1, 1, numX));

        const Handle_<PDE::ScalarCoeff_> disc(PDE::NewConstCoeff(rate));
        const Handle_<PDE::VectorCoeff_> mu(PDE::NewVectorCoeff([=](double s) { return (rate - div) * s; }));
        const Handle_<PDE::MatrixCoeff_> var(PDE::NewMatrixCoeff([=](double s) { return vol * vol * s * s; }));
        PDE::ThetaScheme_ scheme(theta);
        double dt = t / numT;
        scheme.Prepare(dt, grids, *disc, *mu, *var);
        for (int n = 0; n < numT; ++n) {
            (*next[0])(0, 0, 0) = 0.0;
            (*next[0])(0, 0, numX - 1) = maxX * exp(-div * (n + 1) * dt) - std::exp(-rate * (n + 1) * dt) * strike;
            scheme(dt, grids, vals, *disc, *mu, *var, &next);
            vals.Swap(&next);
        }

        const Cube_<>& value = *vals[0];
        const Vector_<> res(value.SliceBegin(0, 0), value.SliceEnd(0, 0));
        Interp::Boundary_ lhs(2, 0.);
        Interp::Boundary_ rhs(2, 0);
        std::unique_ptr<Interp1_> interp(Interp::NewCubic("cubic", loc, res, lhs, rhs));
        double calculated = (*interp)(spot);
        std::cout << std::setw(widths[0]) << std::right << numT << std::fixed << std::setw(widths[1]) << std::right << spot << std::setprecision(6)
                  << std::setw(widths[2]) << std::right << calculated << std::setw(widths[3]) << std::right << benchmark << std::setw(widths[4])
                  << std::right << (calculated - benchmark) / benchmark * 10000 << std::setw(widths[5]) << std::right
                  << int(timer.Elapsed<milliseconds>()) << std::endl;
    }

    return 0;
}

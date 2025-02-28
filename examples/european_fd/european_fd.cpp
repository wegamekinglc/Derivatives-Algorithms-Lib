//
// Created by wegam on 2023/2/26.
//

#include <iomanip>
#include <dal/platform/platform.hpp>
#include <dal/math/operators.hpp>
#include <dal/math/pde/fd1d.hpp>
#include <dal/math/distribution/black.hpp>
#include <dal/math/interp/interpcubic.hpp>
#include <dal/math/pde/meshers/uniform1dmesher.hpp>
#include <dal/math/pde/meshers/concentrating1dmesher.hpp>
#include <dal/utilities/timer.hpp>

using namespace Dal;

int main() {
    Dal::RegisterAll_::Init();

    double min_x = 0.00;
    double max_x = 500.00;
    int steps = 250;
    int n_round = 20;

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
    double vol_std = std::sqrt(t) * vol;
    const auto benchmark = discounts * Distribution::BlackOpt(fwd, vol_std, strike, OptionType_::Value_::CALL);

    std::cout << std::setw(widths[0]) << std::right << "# of grids (x/t)"
              << std::setw(widths[1]) << std::right << "spot"
              << std::setw(widths[2]) << std::right << "price"
              << std::setw(widths[3]) << std::right << "benchmark"
              << std::setw(widths[4]) << std::right << "Diff (bps)"
              << std::setw(widths[5]) << std::right << "Elapsed (ms)"
              << std::endl;

    for (int i = 1; i <= n_round; ++i) {
        int num_x = steps * i + 1;
        int num_t = steps * i;

        Timer_ timer;
        Uniform1DMesher_ x(min_x, max_x, num_x);
        Vector_<> v0 = Apply([&strike](double x) { return std::max(x - strike, 0.0); }, x.Locations());

        PDE::FD1D_ fd(x);
        fd.Init();

        fd.Mu() = (rate - div) * x.Locations();
        fd.R() = Vector_<>(num_x, rate);
        fd.Var() = vol * vol * x.Locations() * x.Locations();
        fd.Res() = v0;

        double dt = t / num_t;
        for (int n = 0; n < num_t; ++n) {
            fd.RollBwd(dt, theta, fd.Res());
            fd.Res()[0] = 0.0;
            fd.Res()[fd.Res().size() - 1] = max_x * exp(-div * (n + 1) * dt) - std::exp(-rate * (n + 1) * dt) * strike;
        }

        Interp::Boundary_ lhs(2, 0.);
        Interp::Boundary_ rhs(2, 0);
        std::unique_ptr<Interp1_> interp(Interp::NewCubic("cubic", fd.X(), fd.Res(), lhs, rhs));
        double calculated = (*interp)(spot);
        std::cout << std::setw(widths[0]) << std::right << num_t
                  << std::fixed
                  << std::setw(widths[1]) << std::right << spot
                  << std::setprecision(6)
                  << std::setw(widths[2]) << std::right << calculated
                  << std::setw(widths[3]) << std::right << benchmark
                  << std::setw(widths[4]) << std::right << (calculated - benchmark) / benchmark * 10000
                  << std::setw(widths[5]) << std::right << int(timer.Elapsed<milliseconds>())
                  << std::endl;
    }

    return 0;
}

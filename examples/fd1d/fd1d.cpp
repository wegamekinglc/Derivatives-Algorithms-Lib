//
// Created by wegam on 2023/2/26.
//

#include <iomanip>
#include <dal/math/operators.hpp>
#include <dal/math/pde/fdi1d.hpp>
#include <dal/math/distribution/black.hpp>
#include <dal/math/interp/interpcubic.hpp>
#include <dal/utilities/timer.hpp>

using namespace Dal;

int main() {

    double min_x = 10.00;
    double max_x = 510.00;
    int steps = 250;
    int n_round = 200;

    double t = 3.002739726;
    double rate = 0.05;
    double div = 0.03;
    double vol = 0.15;
    double strike = 120.0;
    double spot = 100.0;
    double theta = 0.5;

    Vector_<int> widths = {25, 14, 14, 14, 14, 14};
    double discounts = std::exp(-rate * t);
    double fwd = std::exp((rate - div) * t) * spot;
    double vol_std = std::sqrt(t) * vol;
    const auto benchmark = discounts * Distribution::BlackOpt(fwd, vol_std, strike, OptionType_::Value_::CALL);

    std::cout << std::setw(widths[0]) << std::left << "# of grids (x/t)"
              << std::setw(widths[1]) << std::left << "spot"
              << std::setw(widths[2]) << std::left << "price"
              << std::setw(widths[3]) << std::left << "benchmark"
              << std::setw(widths[4]) << std::left << "Diff (bps)"
              << std::setw(widths[5]) << std::left << "Elapsed (ms)"
              << std::endl;

    for (int i = 1; i <= n_round; ++i) {
        int num_x = steps * i;
        int num_t = steps * i;

        Timer_ timer;

        Vector_<> x = Apply([](double x) { return std::log(x); }, Vector::XRange(min_x, max_x, num_x));
        Vector_<> r(num_x, rate);
        Vector_<> mu(num_x, rate - div - 0.5 * vol * vol);
        Vector_<> sigma(num_x, vol);
        Vector_<> v0 = Apply([&strike](double x) { return std::max(std::exp(x) - strike, 0.0); }, x);

        PDE::FD1D_ fd;
        fd.X() = x;
        fd.Init();

        fd.Mu() = mu;
        fd.R() = r;
        fd.Var() = Apply([](double x) { return x * x; }, sigma);
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
        double calculated = (*interp)(std::log(spot));
        std::cout << std::setw(widths[0]) << std::left << num_t
                  << std::fixed
                  << std::setw(widths[1]) << std::left << spot
                  << std::setprecision(8)
                  << std::setw(widths[2]) << std::left << calculated
                  << std::setw(widths[3]) << std::left << benchmark
                  << std::setw(widths[4]) << std::left << (calculated - benchmark) / benchmark * 10000
                  << std::setw(widths[5]) << std::left << int(timer.Elapsed<milliseconds>())
                  << std::endl;
    }

    return 0;
}

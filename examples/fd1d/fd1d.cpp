//
// Created by wegam on 2023/2/26.
//

#include <iomanip>
#include <dal/math/operators.hpp>
#include <dal/math/pde/fdi1d.hpp>
#include <dal/math/distribution/black.hpp>
#include <dal/utilities/timer.hpp>

using namespace Dal;

int main() {

    double t = 3.002739726;
    double rate = 0.0;
    double vol = 0.15;
    double strike = 120.0;

    double theta = 0.5;

    for (int i = 1; i <= 400; ++i) {
        int num_x = 50 * i + 1;
        int num_t = 50 * i;

        Timer_ timer;

        Vector_<> x = Apply([](double x) { return std::log(x); }, Vector::XRange(10.0, 260.0, num_x));
        Vector_<> r(num_x, rate);
        Vector_<> mu(num_x, rate - 0.5 * vol * vol);
        Vector_<> sigma(num_x, vol);
        Vector_<> v0 = Apply([&strike](double x) { return std::max(std::exp(x) - strike, 0.0); }, x);

        PDE::FD1D_ fd;
        fd.X() = x;
        fd.Init();

        fd.Mu() = mu;
        fd.Var() = Apply([](double x) { return x * x; }, sigma);
        fd.Res() = v0;

        double dt = t / num_t;
        for (int n = 0; n < num_t; ++n)
            fd.RollBwd(dt, theta, fd.Res());

        double discounts = std::exp(-rate * t);
        double vol_std = std::sqrt(t) * vol;

        double x_n = fd.X()[i * 22];
        double res_n = fd.Res()[i * 22];
        const double spot = std::exp(x_n);
        const double fwd = std::exp(rate * t) * spot;
        const double benchmark_price = discounts * Distribution::BlackOpt(fwd, vol_std, strike, OptionType_::Value_::CALL);
        std::cout << std::setprecision(12) << num_x << ","  << num_t << "," << spot << "," << res_n
                  << "," << benchmark_price
                  << "," << (res_n - benchmark_price) / benchmark_price * 10000 << "," << timer.Elapsed<milliseconds>() <<std::endl;
    }

    return 0;
}

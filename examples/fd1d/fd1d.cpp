//
// Created by wegam on 2023/2/26.
//

#include <iomanip>
#include <dal/math/pde/fdi1d.hpp>

using namespace Dal;

int main() {

    double t = 0.25;
    double strike = -0.005;
    int num_x = 51;

    double theta = 0.0;
    int num_t = 100;

    Vector_<> x = Vector::XRange(-0.25, 0.25, num_x);
    Vector_<> r(num_x, 0.0);
    Vector_<> mu(num_x, 0.0);
    Vector_<> sigma(num_x, 0.1);
    Vector_<> v0 = Apply([&strike](double x) { return std::max(x - strike, 0.0); }, x);

    PDE::FD1D_ fd(num_t);

    fd.X() = x;
    fd.Init(1);

    fd.Mu() = mu;
    fd.Var() = Apply([](double x) { return x * x; }, sigma);
    fd.Res()[0] = v0;


    double dt = t / num_t;
    for (int n = 0; n < num_t; ++n)
        fd.RollBwd(dt, theta, fd.Res());

    for (int n = 0; n < num_x; ++n)
        std::cout << std::setprecision(4) << fd.X()[n] << ": " << std::setprecision(8) << fd.Res()[0][n] << std::endl;

    return 0;
}

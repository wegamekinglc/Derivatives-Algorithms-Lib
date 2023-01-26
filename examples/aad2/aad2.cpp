//
// Created by wegam on 2023/1/26.
//

#define ADEPT_FAST
#define __FAST_MATH__
#define ADEPT_FAST_EXPONENTIAL 1

#include <iomanip>
#include <adept.h>
#include <adept_source.h>
#include <dal/utilities/timer.hpp>

template <class T_>
T_ BlackTest(T_ fwd, T_ vol, T_ numeraire, T_ strike, T_ expiry, bool is_call, int n_repetition) {
    static const double M_SQRT_2 = 1.4142135623730951;
    const double omega = is_call ? 1.0 : -1.0;
    T_ y = 0.0;
    for(int i = 0; i < n_repetition; ++i) {
        T_ sqrt_var = vol * sqrt(expiry);
        T_ d_minus = log(fwd / strike) / sqrt_var - 0.5 * sqrt_var;
        T_ d_plus = d_minus + sqrt_var;
        y += numeraire * omega * (0.5 * fwd * erfc(-d_plus / M_SQRT_2) - strike * 0.5 * erfc(-d_minus / M_SQRT_2));
    }
    return y;
}

using namespace Dal;
using adept::adouble;

int main() {
    int n_rounds = 1000000;
    int n_repetition = 1;
    double fwd = 1.00;
    double vol = 0.20;
    double numeraire = 1.0;
    double strike = 1.20;
    double expiry = 3.0;
    bool is_call = true;
    Timer_ timer;

    timer.Reset();
    double price;
    for (int i = 0; i < n_rounds; ++i)
        price = BlackTest(fwd, vol, numeraire, strike, expiry, is_call, n_repetition);
    std::cout << "Normal Mode: " << std::setprecision(8) << price << " with " << timer.Elapsed<milliseconds>() << " ms" << std::endl;

    timer.Reset();
    double y_value;
    adept::Stack stack;
    adouble x[5] = {fwd, vol, numeraire, strike, expiry};
    for (int i = 0; i < n_rounds; ++i) {
        stack.new_recording();
        adouble y = BlackTest(x[0], x[1], x[2], x[3], x[4], is_call, n_repetition);
        y.set_gradient(1.0); // Defines y as the cost function
        stack.compute_adjoint();
        y_value = y.value();
    }
    std::cout << "   AAD Mode: " << std::setprecision(8) << y_value << " with " << timer.Elapsed<milliseconds>() << " ms" << std::endl;
    std::cout << "    dP/dFwd: " << std::setprecision(8) << x[0].get_gradient() << std::endl;
    std::cout << "    dP/dVol: " << std::setprecision(8) << x[1].get_gradient() << std::endl;
    std::cout << "    dP/dNum: " << std::setprecision(8) << x[2].get_gradient() << std::endl;
    std::cout << "    dP/dK  : " << std::setprecision(8) << x[3].get_gradient() << std::endl;
    std::cout << "    dP/dT  : " << std::setprecision(8) << x[4].get_gradient() << std::endl;

    return 0;
}
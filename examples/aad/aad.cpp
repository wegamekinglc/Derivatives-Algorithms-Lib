//
// Created by wegam on 2020/12/21.
//

#define ADEPT_FAST
#define ADEPT_FAST_EXPONENTIAL 1

#include <dal/math/operators.hpp>
#include <dal/math/aad/aad.hpp>
#include <dal/math/vectors.hpp>
#include <dal/utilities/timer.hpp>
#include <iomanip>
#include <iostream>
#include <adept.h>
#include <adept_source.h>

using namespace std;
using namespace Dal;
using Dal::AAD::Number_;
using Dal::AAD::Tape_;
using adept::adouble;


template <class T_>
T_ BlackTest(T_ fwd, T_ vol, T_ numeraire, T_ strike, T_ expiry, bool is_call, int n_repetition) {
    static const double M_SQRT_2 = 1.4142135623730951;
    const double omega = is_call ? 1.0 : -1.0;
    T_ y(0.0);
    for(int i = 0; i < n_repetition; ++i) {
        T_ sqrt_var = vol * sqrt(expiry);
        T_ d_minus = log(fwd / strike) / sqrt_var - 0.5 * sqrt_var;
        T_ d_plus = d_minus + sqrt_var;
        y += numeraire * omega * (0.5 * fwd * erfc(-d_plus / M_SQRT_2) - strike * 0.5 * erfc(-d_minus / M_SQRT_2));
    }
    return y;
}



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

    Number_ fwd_aad(fwd);
    Number_ vol_aad(vol);
    Number_ numeraire_aad(numeraire);
    Number_ strike_aad(strike);
    Number_ expiry_aad(expiry);

    Tape_& tape = *Number_::tape_;
    tape.Clear();
    auto aad_resetter = AAD::SetNumResultsForAAD();

    fwd_aad.PutOnTape();
    vol_aad.PutOnTape();
    numeraire_aad.PutOnTape();
    strike_aad.PutOnTape();
    expiry_aad.PutOnTape();

    tape.Mark();
    double d_fwd_aad ;
    double d_vol_aad;
    double d_numeraire_aad;
    double d_strike_aad;
    double d_expiry_aad;

    timer.Reset();
    Number_ price_aad;

    for (int i = 0; i < n_rounds; ++i) {
        tape.RewindToMark();
        price_aad = BlackTest(fwd_aad, vol_aad, numeraire_aad, strike_aad, expiry_aad, is_call, n_repetition);
        price_aad.PropagateToMark();

        price = price_aad.Value() / n_repetition / n_rounds;
        d_fwd_aad = fwd_aad.Adjoint() / n_repetition / n_rounds;
        d_vol_aad = vol_aad.Adjoint() / n_repetition / n_rounds;
        d_numeraire_aad = numeraire_aad.Adjoint() / n_repetition / n_rounds;
        d_strike_aad = strike_aad.Adjoint() / n_repetition / n_rounds;
        d_expiry_aad = expiry_aad.Adjoint() / n_repetition / n_rounds;
    }

    std::cout << " DAL  AAD Mode: " << std::setprecision(8) << price << " with " << timer.Elapsed<milliseconds>() << " ms" << std::endl;
    std::cout << "      dP/dFwd : " << std::setprecision(8) << d_fwd_aad << std::endl;
    std::cout << "      dP/dVol : " << std::setprecision(8) << d_vol_aad << std::endl;
    std::cout << "      dP/dNum : " << std::setprecision(8) << d_numeraire_aad << std::endl;
    std::cout << "      dP/dK   : " << std::setprecision(8) << d_strike_aad << std::endl;
    std::cout << "      dP/dT   : " << std::setprecision(8) << d_expiry_aad << std::endl;
    tape.Clear();

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
    std::cout << "ADEPT AAD Mode: " << std::setprecision(8) << y_value << " with " << timer.Elapsed<milliseconds>() << " ms" << std::endl;
    std::cout << "      dP/dFwd : " << std::setprecision(8) << x[0].get_gradient() << std::endl;
    std::cout << "      dP/dVol : " << std::setprecision(8) << x[1].get_gradient() << std::endl;
    std::cout << "      dP/dNum : " << std::setprecision(8) << x[2].get_gradient() << std::endl;
    std::cout << "      dP/dK   : " << std::setprecision(8) << x[3].get_gradient() << std::endl;
    std::cout << "      dP/dT   : " << std::setprecision(8) << x[4].get_gradient() << std::endl;

    return 0;
}
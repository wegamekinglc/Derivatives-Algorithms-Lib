//
// Created by wegam on 2020/12/21.
//

#include <dal/platform/platform.hpp>
#include <dal/math/operators.hpp>
#include <dal/math/aad/aad.hpp>
#include <dal/math/vectors.hpp>
#include <dal/utilities/timer.hpp>
#include <iomanip>
#include <iostream>

using namespace std;
using namespace Dal;
using Dal::AAD::Number_;
using Dal::AAD::Tape_;


template <class T_>
T_ BlackTest(const T_& fwd, const T_& vol, const T_& numeraire, const T_& strike, const T_& expiry, bool is_call) {
    static const double M_SQRT_2 = 1.4142135623730951;
    const double omega = is_call ? 1.0 : -1.0;
    T_ y(0.0);
    T_ sqrt_var = vol * sqrt(expiry);
    T_ d_minus = log(fwd / strike) / sqrt_var - 0.5 * sqrt_var;
    T_ d_plus = d_minus + sqrt_var;
    y = numeraire * omega * (0.5 * fwd * erfc(-d_plus / M_SQRT_2) - strike * 0.5 * erfc(-d_minus / M_SQRT_2));
    return y;
}


int main() {
    Dal::RegisterAll_::Init();

    int n_rounds = 1000;
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
        price = BlackTest(fwd, vol, numeraire, strike, expiry, is_call);
    std::cout << "Normal Mode: " << std::setprecision(8) << price << " with " << timer.Elapsed<milliseconds>() << " ms" << std::endl;

    Number_ fwd_aad(fwd);
    Number_ vol_aad(vol);
    Number_ numeraire_aad(numeraire);
    Number_ strike_aad(strike);
    Number_ expiry_aad(expiry);

#ifndef USE_XAD
    auto& tape = AAD::GetTape();
#else
    auto tape = AAD::GetTape();
#endif

    AAD::Reset(&tape);
    AAD::SetActive(&tape);

    tape.registerInput(fwd_aad);
    tape.registerInput(vol_aad);
    tape.registerInput(numeraire_aad);
    tape.registerInput(strike_aad);
    tape.registerInput(expiry_aad);
    AAD::NewRecording(&tape);

    timer.Reset();

    Number_ numeraire_aad_2 = numeraire_aad * 2.0;
    auto begin = AAD::GetPosition(tape);
    Number_ price_aad = BlackTest(fwd_aad, vol_aad, numeraire_aad_2, strike_aad, expiry_aad, is_call);
    AAD::SetGradient(price_aad, 1.0);
    AAD::Evaluate(&tape);
    AAD::ResetToPos(&tape, begin);

    price_aad = BlackTest(fwd_aad, vol_aad, numeraire_aad_2, strike_aad, expiry_aad, is_call);
    AAD::SetGradient(price_aad, 1.0);
    AAD::Evaluate(&tape);
    AAD::ResetToPos(&tape, begin);

    AAD::Evaluate(&tape);

    std::cout << " DAL  AAD Mode: " << std::setprecision(8) << price_aad.value() << " with " << timer.Elapsed<milliseconds>() << " ms" << std::endl;
    std::cout << "      dP/dFwd : " << std::setprecision(8) << AAD::GetGradient(fwd_aad) << std::endl;
    std::cout << "      dP/dVol : " << std::setprecision(8) << AAD::GetGradient(vol_aad) << std::endl;
    std::cout << "      dP/dNum : " << std::setprecision(8) << AAD::GetGradient(numeraire_aad) << std::endl;
    std::cout << "      dP/dK   : " << std::setprecision(8) << AAD::GetGradient(strike_aad) << std::endl;
    std::cout << "      dP/dT   : " << std::setprecision(8) << AAD::GetGradient(expiry_aad) << std::endl;

    return 0;
}
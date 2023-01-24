//
// Created by wegam on 2020/12/21.
//

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
T_ BlackTest(T_ fwd, T_ vol, T_ numeraire, T_ strike, T_ expiry, bool is_call) {
    const double omega = is_call ? 1.0 : -1.0;
    const auto sqrt_var = vol * Sqrt(expiry);
    const auto dMinus = Log(fwd / strike) / sqrt_var - 0.5 * sqrt_var;
    const auto dPlus = dMinus + sqrt_var;
    return numeraire * omega * (fwd * NCDF(dPlus) - strike * NCDF(dMinus));
}


int main() {
    int n_rounds = 1000000;
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

    Tape_& tape = *Number_::tape_;
    tape.Clear();
    auto aad_resetter = AAD::SetNumResultsForAAD();

    fwd_aad.PutOnTape();
    vol_aad.PutOnTape();
    numeraire_aad.PutOnTape();
    strike_aad.PutOnTape();
    expiry_aad.PutOnTape();

    tape.Mark();
    timer.Reset();
    Number_ price_aad;

    for (int i = 0; i < n_rounds; ++i) {
        tape.RewindToMark();
        price_aad = BlackTest(fwd_aad, vol_aad, numeraire_aad, strike_aad, expiry_aad, is_call);
        price_aad.PropagateToMark();
    }
    price_aad.PropagateToMark();
    double d_fwd_aad = fwd_aad.Adjoint() / n_rounds;
    double d_vol_aad = vol_aad.Adjoint() / n_rounds;
    double d_numeraire_aad = numeraire_aad.Adjoint() / n_rounds;
    double d_strike_aad = strike_aad.Adjoint() / n_rounds;
    double d_expiry_aad = expiry_aad.Adjoint() / n_rounds;
    std::cout << "   AAD Mode: " << std::setprecision(8) << price_aad.Value() << " with " << timer.Elapsed<milliseconds>() << " ms" << std::endl;
    std::cout << "    dP/dFwd: " << std::setprecision(8) << d_fwd_aad << std::endl;
    std::cout << "    dP/dVol: " << std::setprecision(8) << d_vol_aad << std::endl;
    std::cout << "    dP/dNum: " << std::setprecision(8) << d_numeraire_aad << std::endl;
    std::cout << "    dP/dK  : " << std::setprecision(8) << d_strike_aad << std::endl;
    std::cout << "    dP/dT  : " << std::setprecision(8) << d_expiry_aad << std::endl;
    return 0;
}
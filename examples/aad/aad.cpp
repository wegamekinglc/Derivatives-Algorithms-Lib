//
// Created by wegam on 2020/12/21.
//

#include <adept.h>
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

    int n_rounds = 10000000;
    double fwd = 1.00;
    double vol = 0.20;
    double numeraire = 1.0;
    double strike = 1.20;
    double expiry = 3.0;
    bool is_call = true;
    Timer_ timer;

    Vector_<int> widths = {14, 14, 14, 14, 14, 14, 14, 14};

    std::cout << std::setw(widths[0]) << std::left << "Method"
              << std::setw(widths[1]) << std::right << "PV"
              << std::setw(widths[2]) << std::right << "dP/dFwd"
              << std::setw(widths[3]) << std::right << "dP/dVol"
              << std::setw(widths[4]) << std::right << "dP/dNum"
              << std::setw(widths[5]) << std::right << "dP/dK"
              << std::setw(widths[6]) << std::right << "dP/dT"
              << std::setw(widths[7]) << std::right << "Elapsed (ms)"
              << std::endl;

    {
        timer.Reset();
        double total_price = 0.0;
        for (int i = 0; i < n_rounds; ++i)
            total_price += BlackTest(fwd, vol, numeraire, strike, expiry, is_call);
        const auto duration = static_cast<int>(timer.Elapsed<milliseconds>());

        std::cout << std::setw(widths[0]) << std::left << "Non-AAD"
                  << std::fixed
                  << std::setprecision(6)
                  << std::setw(widths[1]) << std::right << total_price / n_rounds
                  << std::setw(widths[2]) << std::right << "#NA"
                  << std::setw(widths[3]) << std::right << "#NA"
                  << std::setw(widths[4]) << std::right << "#NA"
                  << std::setw(widths[5]) << std::right << "#NA"
                  << std::setw(widths[6]) << std::right << "#NA"
                  << std::setw(widths[7]) << std::right << duration
                  << std::endl;
    }

    {
        // aadet
        Number_::Tape()->Clear();

        timer.Reset();
        Number_ fwd_aad(fwd);
        Number_ vol_aad(vol);
        Number_ numeraire_aad(numeraire);
        Number_ strike_aad(strike);
        Number_ expiry_aad(expiry);

        fwd_aad.PutOnTape();
        vol_aad.PutOnTape();
        numeraire_aad.PutOnTape();
        strike_aad.PutOnTape();
        expiry_aad.PutOnTape();

        Number_  price_aad{0.0};
        for (int i = 0; i < n_rounds; ++i) {
            Number_::Tape()->Rewind();
            price_aad = BlackTest(fwd_aad, vol_aad, numeraire_aad, strike_aad, expiry_aad, is_call);
            price_aad.PropagateToStart();
        }

        const auto duration = static_cast<int>(timer.Elapsed<milliseconds>());
        std::cout << std::setw(widths[0]) << std::left << "AADET"
                  << std::fixed
                  << std::setprecision(6)
                  << std::setw(widths[1]) << std::right << price_aad.value()
                  << std::setw(widths[2]) << std::right << fwd_aad.Adjoint() / n_rounds
                  << std::setw(widths[3]) << std::right << vol_aad.Adjoint() / n_rounds
                  << std::setw(widths[4]) << std::right << numeraire_aad.Adjoint() / n_rounds
                  << std::setw(widths[5]) << std::right << strike_aad.Adjoint() / n_rounds
                  << std::setw(widths[6]) << std::right << expiry_aad.Adjoint() / n_rounds
                  << std::setw(widths[7]) << std::right << duration
                  << std::endl;
    }

    {
        adept::Stack stack;
        using adept::adouble;

        adouble fwd_aad(fwd);
        adouble vol_aad(vol);
        adouble numeraire_aad(numeraire);
        adouble strike_aad(strike);
        adouble expiry_aad(expiry);

        timer.Reset();
        adouble price_aad{0.0};
        for (int i = 0; i < n_rounds; ++i) {
            stack.new_recording();
            price_aad = BlackTest(fwd_aad, vol_aad, numeraire_aad, strike_aad, expiry_aad, is_call);
            price_aad.set_gradient(1.0);
            stack.compute_adjoint();
        }

        const auto duration = static_cast<int>(timer.Elapsed<milliseconds>());
        std::cout << std::setw(widths[0]) << std::left << "ADEPT"
                  << std::fixed
                  << std::setprecision(6)
                  << std::setw(widths[1]) << std::right << price_aad.value()
                  << std::setw(widths[2]) << std::right << fwd_aad.get_gradient()
                  << std::setw(widths[3]) << std::right << vol_aad.get_gradient()
                  << std::setw(widths[4]) << std::right << numeraire_aad.get_gradient()
                  << std::setw(widths[5]) << std::right << strike_aad.get_gradient()
                  << std::setw(widths[6]) << std::right << expiry_aad.get_gradient()
                  << std::setw(widths[7]) << std::right << duration
                  << std::endl;
    }

    return 0;
}
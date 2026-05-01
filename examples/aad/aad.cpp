//
// Created by wegam on 2020/12/21.
//

#include <XAD/XAD.hpp>
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

    int n_rounds = 1000000;
    double expiry = 3.0;
    double fwd = 100.00 * std::exp(0.02 * expiry);
    double vol = 0.15;
    double numeraire = std::exp(-0.05 * expiry);
    double strike = 120;
    bool is_call = true;
    Timer_ timer;

    Vector_<int> widths = {25, 14, 14, 14, 14, 14, 14, 14};

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
        // no aad
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
        // builtin framework
        AAD::Clear(*AAD::Tape());

        timer.Reset();
        Number_ fwd_aad(fwd);
        Number_ vol_aad(vol);
        Number_ numeraire_aad(numeraire);
        Number_ strike_aad(strike);
        Number_ expiry_aad(expiry);

        PutOnTape(fwd_aad);
        PutOnTape(vol_aad);
        PutOnTape(numeraire_aad);
        PutOnTape(strike_aad);
        PutOnTape(expiry_aad);
        AAD::NewRecording(*AAD::Tape());

        Number_ price_aad{0.0};
        for (int i = 0; i < n_rounds; ++i) {
            AAD::Rewind(*AAD::Tape());
            price_aad = BlackTest(fwd_aad, vol_aad, numeraire_aad, strike_aad, expiry_aad, is_call);
            Adjoint(price_aad) = 1.0;
            AAD::PropagateToStart(*AAD::Tape());
        }

        const auto duration = static_cast<int>(timer.Elapsed<milliseconds>());
#ifdef DAL_USE_XAD_AAD
        std::cout << std::setw(widths[0]) << std::left << "Builtin (XAD)"
#elif defined(DAL_USE_CODIPACK_AAD)
        std::cout << std::setw(widths[0]) << std::left << "Builtin (CoDiPack)"
#elif defined(DAL_USE_ADEPT_AAD)
        std::cout << std::setw(widths[0]) << std::left << "Builtin (Adept)"
#else
        std::cout << std::setw(widths[0]) << std::left << "Builtin (AADET)"
#endif
                  << std::fixed
                  << std::setprecision(6)
                  << std::setw(widths[1]) << std::right << Value(price_aad)
                  << std::setw(widths[2]) << std::right << Adjoint(fwd_aad) / n_rounds
                  << std::setw(widths[3]) << std::right << Adjoint(vol_aad) / n_rounds
                  << std::setw(widths[4]) << std::right << Adjoint(numeraire_aad) / n_rounds
                  << std::setw(widths[5]) << std::right << Adjoint(strike_aad) / n_rounds
                  << std::setw(widths[6]) << std::right << Adjoint(expiry_aad) / n_rounds
                  << std::setw(widths[7]) << std::right << duration
                  << std::endl;
    }

    {
        // xad
        using mode = xad::adj<double>;
        using ADouble = mode::active_type;
        using Tape = mode::tape_type;

#ifndef DAL_USE_XAD_AAD
        Tape tape;
#else
        auto& tape = AAD::Tape()->tape_;
#endif

        timer.Reset();

        ADouble fwd_aad(fwd);
        ADouble vol_aad(vol);
        ADouble numeraire_aad(numeraire);
        ADouble strike_aad(strike);
        ADouble expiry_aad(expiry);

        tape.registerInput(fwd_aad);
        tape.registerInput(vol_aad);
        tape.registerInput(numeraire_aad);
        tape.registerInput(strike_aad);
        tape.registerInput(expiry_aad);

        ADouble price_aad(0.0);
        tape.registerOutput(price_aad);

        tape.newRecording();
        auto current_pos = tape.getPosition();

        for (int i = 0; i < n_rounds; ++i) {
            tape.resetTo(current_pos);
            price_aad = BlackTest(fwd_aad, vol_aad, numeraire_aad, strike_aad, expiry_aad, is_call);
            xad::derivative(price_aad) = 1.0;
            tape.computeAdjoints();
        }

        const auto duration = static_cast<int>(timer.Elapsed<milliseconds>());
        std::cout << std::setw(widths[0]) << std::left << "XAD"
                  << std::fixed
                  << std::setprecision(6)
                  << std::setw(widths[1]) << std::right << xad::value(price_aad)
                  << std::setw(widths[2]) << std::right << xad::derivative(fwd_aad) / n_rounds
                  << std::setw(widths[3]) << std::right << xad::derivative(vol_aad) / n_rounds
                  << std::setw(widths[4]) << std::right << xad::derivative(numeraire_aad) / n_rounds
                  << std::setw(widths[5]) << std::right << xad::derivative(strike_aad) / n_rounds
                  << std::setw(widths[6]) << std::right << xad::derivative(expiry_aad) / n_rounds
                  << std::setw(widths[7]) << std::right << duration
                  << std::endl;
    }

    // {
    //     // xad with jit
    //     using mode = xad::adj<double>;
    //     using ADouble = mode::active_type;

    //     // Create JIT compiler and register inputs
    //     xad::JITCompiler<double, 1> jit;

    //     timer.Reset();

    //     ADouble fwd_aad(fwd);
    //     ADouble vol_aad(vol);
    //     ADouble numeraire_aad(numeraire);
    //     ADouble strike_aad(strike);
    //     ADouble expiry_aad(expiry);

    //     jit.registerInput(fwd_aad);
    //     jit.registerInput(vol_aad);
    //     jit.registerInput(numeraire_aad);
    //     jit.registerInput(strike_aad);
    //     jit.registerInput(expiry_aad);

    //     ADouble price_aad = BlackTest(fwd_aad, vol_aad, numeraire_aad, strike_aad, expiry_aad, is_call);
    //     jit.registerOutput(price_aad);
    //     jit.compile();

    //     for (int i = 0; i < n_rounds; ++i) {
    //         jit.clearDerivatives();
    //         xad::derivative(price_aad) = 1.0;
    //         jit.computeAdjoints();
    //     }

    //     const auto duration = static_cast<int>(timer.Elapsed<milliseconds>());
    //     std::cout << std::setw(widths[0]) << std::left << "XAD w/ jit"
    //               << std::fixed
    //               << std::setprecision(6)
    //               << std::setw(widths[1]) << std::right << xad::value(price_aad)
    //               << std::setw(widths[2]) << std::right << xad::derivative(fwd_aad)
    //               << std::setw(widths[3]) << std::right << xad::derivative(vol_aad)
    //               << std::setw(widths[4]) << std::right << xad::derivative(numeraire_aad)
    //               << std::setw(widths[5]) << std::right << xad::derivative(strike_aad)
    //               << std::setw(widths[6]) << std::right << xad::derivative(expiry_aad)
    //               << std::setw(widths[7]) << std::right << duration
    //               << std::endl;
    // }

    return 0;
}

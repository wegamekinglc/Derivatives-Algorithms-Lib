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
T_ BlackTest(const T_& fwd, const T_& vol, const T_& numeraire, const T_& strike, const T_& expiry, bool isCall) {
    static const double M_SQRT_2 = 1.4142135623730951;
    const double omega = isCall ? 1.0 : -1.0;
    T_ y(0.0);
    T_ sqrtVar = vol * sqrt(expiry);
    T_ d_minus = log(fwd / strike) / sqrtVar - 0.5 * sqrtVar;
    T_ d_plus = d_minus + sqrtVar;
    y = numeraire * omega * (0.5 * fwd * erfc(-d_plus / M_SQRT_2) - strike * 0.5 * erfc(-d_minus / M_SQRT_2));
    return y;
}


int main() {
    Dal::RegisterAll_::Init();

    int nRounds = 1000000;
    double expiry = 3.0;
    double fwd = 100.00 * std::exp(0.02 * expiry);
    double vol = 0.15;
    double numeraire = std::exp(-0.05 * expiry);
    double strike = 120;
    bool isCall = true;
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
        for (int i = 0; i < nRounds; ++i)
            total_price += BlackTest(fwd, vol, numeraire, strike, expiry, isCall);
        const auto duration = static_cast<int>(timer.Elapsed<milliseconds>());

        std::cout << std::setw(widths[0]) << std::left << "Non-AAD"
                  << std::fixed
                  << std::setprecision(6)
                  << std::setw(widths[1]) << std::right << total_price / nRounds
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
        Number_ fwdAad(fwd);
        Number_ volAad(vol);
        Number_ numeraireAad(numeraire);
        Number_ strikeAad(strike);
        Number_ expiryAad(expiry);

        PutOnTape(fwdAad);
        PutOnTape(volAad);
        PutOnTape(numeraireAad);
        PutOnTape(strikeAad);
        PutOnTape(expiryAad);
        AAD::NewRecording(*AAD::Tape());

        Number_ priceAad{0.0};
        for (int i = 0; i < nRounds; ++i) {
            AAD::Rewind(*AAD::Tape());
            priceAad = BlackTest(fwdAad, volAad, numeraireAad, strikeAad, expiryAad, isCall);
            Adjoint(priceAad) = 1.0;
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
                  << std::setw(widths[1]) << std::right << Value(priceAad)
                  << std::setw(widths[2]) << std::right << Adjoint(fwdAad) / nRounds
                  << std::setw(widths[3]) << std::right << Adjoint(volAad) / nRounds
                  << std::setw(widths[4]) << std::right << Adjoint(numeraireAad) / nRounds
                  << std::setw(widths[5]) << std::right << Adjoint(strikeAad) / nRounds
                  << std::setw(widths[6]) << std::right << Adjoint(expiryAad) / nRounds
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

        ADouble fwdAad(fwd);
        ADouble volAad(vol);
        ADouble numeraireAad(numeraire);
        ADouble strikeAad(strike);
        ADouble expiryAad(expiry);

        tape.registerInput(fwdAad);
        tape.registerInput(volAad);
        tape.registerInput(numeraireAad);
        tape.registerInput(strikeAad);
        tape.registerInput(expiryAad);

        ADouble priceAad(0.0);
        tape.registerOutput(priceAad);

        tape.newRecording();
        auto currentPos = tape.getPosition();

        for (int i = 0; i < nRounds; ++i) {
            tape.resetTo(currentPos);
            priceAad = BlackTest(fwdAad, volAad, numeraireAad, strikeAad, expiryAad, isCall);
            xad::derivative(priceAad) = 1.0;
            tape.computeAdjoints();
        }

        const auto duration = static_cast<int>(timer.Elapsed<milliseconds>());
        std::cout << std::setw(widths[0]) << std::left << "XAD"
                  << std::fixed
                  << std::setprecision(6)
                  << std::setw(widths[1]) << std::right << xad::value(priceAad)
                  << std::setw(widths[2]) << std::right << xad::derivative(fwdAad) / nRounds
                  << std::setw(widths[3]) << std::right << xad::derivative(volAad) / nRounds
                  << std::setw(widths[4]) << std::right << xad::derivative(numeraireAad) / nRounds
                  << std::setw(widths[5]) << std::right << xad::derivative(strikeAad) / nRounds
                  << std::setw(widths[6]) << std::right << xad::derivative(expiryAad) / nRounds
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

    //     ADouble fwdAad(fwd);
    //     ADouble volAad(vol);
    //     ADouble numeraireAad(numeraire);
    //     ADouble strikeAad(strike);
    //     ADouble expiryAad(expiry);

    //     jit.registerInput(fwdAad);
    //     jit.registerInput(volAad);
    //     jit.registerInput(numeraireAad);
    //     jit.registerInput(strikeAad);
    //     jit.registerInput(expiryAad);

    //     ADouble priceAad = BlackTest(fwdAad, volAad, numeraireAad, strikeAad, expiryAad, isCall);
    //     jit.registerOutput(priceAad);
    //     jit.compile();

    //     for (int i = 0; i < nRounds; ++i) {
    //         jit.clearDerivatives();
    //         xad::derivative(priceAad) = 1.0;
    //         jit.computeAdjoints();
    //     }

    //     const auto duration = static_cast<int>(timer.Elapsed<milliseconds>());
    //     std::cout << std::setw(widths[0]) << std::left << "XAD w/ jit"
    //               << std::fixed
    //               << std::setprecision(6)
    //               << std::setw(widths[1]) << std::right << xad::value(priceAad)
    //               << std::setw(widths[2]) << std::right << xad::derivative(fwdAad)
    //               << std::setw(widths[3]) << std::right << xad::derivative(volAad)
    //               << std::setw(widths[4]) << std::right << xad::derivative(numeraireAad)
    //               << std::setw(widths[5]) << std::right << xad::derivative(strikeAad)
    //               << std::setw(widths[6]) << std::right << xad::derivative(expiryAad)
    //               << std::setw(widths[7]) << std::right << duration
    //               << std::endl;
    // }

    return 0;
}

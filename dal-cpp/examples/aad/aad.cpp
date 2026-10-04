//
// Created by wegam on 2020/12/21.
//

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <string>
#include <dal/platform/platform.hpp>
#include <dal/math/aad/native.hpp>
#include <dal/math/aad/recording.hpp>
#include <dal/utilities/timer.hpp>

namespace {
    template <class T_> T_ BlackTest(const T_& fwd, const T_& vol, const T_& numeraire, const T_& strike, const T_& expiry, bool isCall) {
        using std::erfc;
        using std::log;
        using std::sqrt;
        constexpr double sqrtTwo = 1.4142135623730951;
        const double omega = isCall ? 1.0 : -1.0;
        const T_ sqrtVar = vol * sqrt(expiry);
        const T_ dMinus = log(fwd / strike) / sqrtVar - 0.5 * sqrtVar;
        const T_ dPlus = dMinus + sqrtVar;
        return numeraire * omega * (0.5 * fwd * erfc(-omega * dPlus / sqrtTwo) - 0.5 * strike * erfc(-omega * dMinus / sqrtTwo));
    }

    std::array<double, 6> AnalyticBlack(double fwd, double vol, double numeraire, double strike, double expiry, bool isCall) {
        constexpr double sqrtTwo = 1.4142135623730951;
        constexpr double sqrtTwoPi = 2.5066282746310005;
        const double omega = isCall ? 1.0 : -1.0;
        const double sqrtTime = std::sqrt(expiry);
        const double sqrtVar = vol * sqrtTime;
        const double dPlus = std::log(fwd / strike) / sqrtVar + 0.5 * sqrtVar;
        const double dMinus = dPlus - sqrtVar;
        const double cdfPlus = 0.5 * std::erfc(-omega * dPlus / sqrtTwo);
        const double cdfMinus = 0.5 * std::erfc(-omega * dMinus / sqrtTwo);
        const double pdfPlus = std::exp(-0.5 * dPlus * dPlus) / sqrtTwoPi;
        const double undiscounted = omega * (fwd * cdfPlus - strike * cdfMinus);
        return {numeraire * undiscounted,
                numeraire * omega * cdfPlus,
                numeraire * fwd * pdfPlus * sqrtTime,
                undiscounted,
                -numeraire * omega * cdfMinus,
                numeraire * fwd * pdfPlus * vol / (2.0 * sqrtTime)};
    }

    void PrintResult(const char* method, const std::array<double, 6>& values, int elapsed, bool hasRisk = true) {
        std::cout << std::setw(25) << std::left << method << std::fixed << std::setprecision(6);
        for (size_t i = 0; i < values.size(); ++i) {
            if (hasRisk || i == 0)
                std::cout << std::setw(14) << std::right << values[i];
            else
                std::cout << std::setw(14) << std::right << "#NA";
        }
        std::cout << std::setw(14) << std::right << elapsed << '\n';
    }
} // namespace

int main() {
    Dal::RegisterAll_::Init();
    constexpr int rounds = 1000000;
    const double expiry = 3.0;
    const double fwd = 100.0 * std::exp(0.02 * expiry);
    const double vol = 0.15;
    const double numeraire = std::exp(-0.05 * expiry);
    const double strike = 120.0;
    constexpr bool isCall = true;
    Dal::Timer_ timer;

    std::cout << '\n'
              << std::string(70, '=') << "\n  Native AAD Black pricing and analytic risk\n"
              << std::string(70, '=') << "\n\n"
              << std::setw(25) << std::left << "Method";
    for (const auto* label : {"PV", "dP/dFwd", "dP/dVol", "dP/dNum", "dP/dK", "dP/dT", "Elapsed (ms)"})
        std::cout << std::setw(14) << std::right << label;
    std::cout << '\n' << std::string(123, '-') << '\n';

    timer.Reset();
    double totalPrice = 0.0;
    for (int i = 0; i < rounds; ++i)
        totalPrice += BlackTest(fwd, vol, numeraire, strike, expiry, isCall);
    PrintResult("Passive", {totalPrice / rounds, 0.0, 0.0, 0.0, 0.0, 0.0}, static_cast<int>(timer.Elapsed<std::chrono::milliseconds>()), false);

    Dal::AAD::Clear(*Dal::AAD::Tape());
    timer.Reset();
    std::array<double, 6> values{};
    {
        Dal::AAD::RecordingScope_ scope;
        Dal::AAD::Number_ fwdAad, volAad, numeraireAad, strikeAad, expiryAad;
        scope.RegisterInput(fwdAad, fwd);
        scope.RegisterInput(volAad, vol);
        scope.RegisterInput(numeraireAad, numeraire);
        scope.RegisterInput(strikeAad, strike);
        scope.RegisterInput(expiryAad, expiry);
        scope.StartRecording();
        const auto checkpoint = scope.MakeCheckpoint();
        for (int i = 0; i < rounds; ++i) {
            scope.Restore(checkpoint);
            Dal::AAD::Number_ price = BlackTest(fwdAad, volAad, numeraireAad, strikeAad, expiryAad, isCall);
            values[0] = Dal::AAD::Value(price);
            scope.FinishRecording();
            Dal::AAD::NativeOperations_::SetSeed(price, 1.0);
            scope.ReverseSuffix(checkpoint);
        }
        scope.ReversePrefix(checkpoint);
        const std::array<const Dal::AAD::Number_*, 5> inputs{&fwdAad, &volAad, &numeraireAad, &strikeAad, &expiryAad};
        for (size_t i = 0; i < inputs.size(); ++i)
            values[i + 1] = Dal::AAD::NativeOperations_::ReadAdjoint(*inputs[i]) / rounds;
        scope.Close();
    }
    PrintResult("Native AAD", values, static_cast<int>(timer.Elapsed<std::chrono::milliseconds>()));
    const auto reference = AnalyticBlack(fwd, vol, numeraire, strike, expiry, isCall);
    PrintResult("Analytic", reference, 0);
    for (size_t i = 0; i < values.size(); ++i)
        REQUIRE(std::isfinite(values[i]) && std::abs(values[i] - reference[i]) <= 1e-7 * std::max(1.0, std::abs(reference[i])),
                "Native AAD Black result differs from its analytic reference");
    Dal::AAD::Clear(*Dal::AAD::Tape());
    std::cout << std::string(123, '-') << "\n\n";
    return 0;
}

//
// Created by wegam on 2026/4/19.
//

#include <dal/platform/platform.hpp>
#include <dal/curve/yccalibration.hpp>
#include <iostream>
#include <iomanip>
#include <memory>

using namespace Dal;

int main() {
    const Date_ today(2024, 1, 15);
    const DayBasis_ basis("ACT_365F");

    Vector_<DepositInstrument_> deposits = {
        {Date::AddMonths(today, 1), 0.0450},
        {Date::AddMonths(today, 3), 0.0460},
        {Date::AddMonths(today, 6), 0.0475},
    };

    Vector_<SwapInstrument_> swaps = {
        {Date::AddMonths(today, 12), 0.0490, 6},
        {Date::AddMonths(today, 24), 0.0500, 6},
        {Date::AddMonths(today, 36), 0.0505, 6},
        {Date::AddMonths(today, 60), 0.0510, 6},
    };

    Vector_<Date_> knotDates = {
        Date::AddMonths(today, 1),
        Date::AddMonths(today, 3),
        Date::AddMonths(today, 6),
        Date::AddMonths(today, 12),
        Date::AddMonths(today, 18),
        Date::AddMonths(today, 24),
        Date::AddMonths(today, 36),
        Date::AddMonths(today, 48),
        Date::AddMonths(today, 60),
    };

    const int nParams = 2 * static_cast<int>(knotDates.size());
    const int nInstruments = static_cast<int>(deposits.size() + swaps.size());

    std::cout << "Yield Curve Calibration via Underdetermined Search\n";
    std::cout << "===================================================\n";
    std::cout << "Knot dates: " << knotDates.size() << "\n";
    std::cout << "Parameters: " << nParams << " (2 per knot: left + right)\n";
    std::cout << "Instruments: " << nInstruments << "\n";
    std::cout << "Degrees of freedom: " << nParams - nInstruments << "\n\n";

    std::cout << "Calibrating...\n";
    std::unique_ptr<DiscountCurve_> dc(CalibrateYieldCurve(today, deposits, swaps, knotDates, basis));
    std::cout << "Calibration complete.\n\n";

    std::cout << "Repricing Check:\n";
    std::cout << std::fixed << std::setprecision(6);
    std::cout << std::setw(12) << "Instrument" << std::setw(10) << "Market" << std::setw(12) << "Model" << std::setw(12) << "Error(bp)\n";
    for (int i = 0; i < static_cast<int>(deposits.size()); ++i) {
        double modelRate = DepositRate(*dc, today, deposits[i].maturity_, basis);
        std::cout << std::setw(12) << "Dep " + std::to_string(i + 1)
                  << std::setw(10) << deposits[i].marketRate_ * 100.0
                  << std::setw(12) << modelRate * 100.0
                  << std::setw(12) << (modelRate - deposits[i].marketRate_) * 10000.0 << "\n";
    }
    for (int i = 0; i < static_cast<int>(swaps.size()); ++i) {
        double modelRate = SwapRate(*dc, today, swaps[i].maturity_, swaps[i].freqMonths_, basis);
        std::cout << std::setw(12) << "Swap " + std::to_string(i + 1)
                  << std::setw(10) << swaps[i].marketRate_ * 100.0
                  << std::setw(12) << modelRate * 100.0
                  << std::setw(12) << (modelRate - swaps[i].marketRate_) * 10000.0 << "\n";
    }

    return 0;
}

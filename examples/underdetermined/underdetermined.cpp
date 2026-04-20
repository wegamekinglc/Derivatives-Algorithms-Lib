//
// Created by wegam on 2026/4/19.
//

#include <iomanip>
#include <iostream>
#include <memory>
#include <dal/platform/platform.hpp>
#include <dal/curve/yccalibration.hpp>

using namespace Dal;

int main() {
    const Date_ today(2024, 1, 15);
    const String_& ccy = "USD";
    const DayBasis_ basis("ACT_365F");

    Vector_<Handle_<YCInstrument_>> instruments;
    instruments.push_back(Handle_<YCInstrument_>(new Deposit_(today, Date::AddMonths(today, 1), 0.0450, basis)));
    instruments.push_back(Handle_<YCInstrument_>(new Deposit_(today, Date::AddMonths(today, 3), 0.0460, basis)));
    instruments.push_back(Handle_<YCInstrument_>(new Deposit_(today, Date::AddMonths(today, 6), 0.0475, basis)));
    instruments.push_back(Handle_<YCInstrument_>(new Swap_(today, Date::AddMonths(today, 12), 0.0490, 6, basis)));
    instruments.push_back(Handle_<YCInstrument_>(new Swap_(today, Date::AddMonths(today, 24), 0.0500, 6, basis)));
    instruments.push_back(Handle_<YCInstrument_>(new Swap_(today, Date::AddMonths(today, 36), 0.0505, 6, basis)));
    instruments.push_back(Handle_<YCInstrument_>(new Swap_(today, Date::AddMonths(today, 60), 0.0510, 6, basis)));

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
    const int nInstruments = static_cast<int>(instruments.size());

    std::cout << "Yield Curve Calibration via Underdetermined Search\n";
    std::cout << "===================================================\n";
    std::cout << "Knot dates: " << knotDates.size() << "\n";
    std::cout << "Parameters: " << nParams << " (2 per knot: left + right)\n";
    std::cout << "Instruments: " << nInstruments << "\n";
    std::cout << "Degrees of freedom: " << nParams - nInstruments << "\n\n";

    std::cout << "Calibrating...\n";
    std::unique_ptr<DiscountCurve_> dc(CalibrateYieldCurve(today, ccy, instruments, knotDates));
    std::cout << "Calibration complete.\n\n";

    std::cout << "Repricing Check:\n";
    std::cout << std::fixed << std::setprecision(6);
    std::cout << std::setw(12) << "Instrument" << std::setw(10) << "Market" << std::setw(12) << "Model" << std::setw(12) << "Error(bp)\n";

    CalibratedYieldCurve_ calibYC(*dc);
    for (int i = 0; i < nInstruments; ++i) {
        Handle_<YCInstrument_::Rate_> rate = instruments[i]->Precompute(instruments[i], Handle_<YieldCurve_>());
        double modelRate = (*rate)(calibYC);
        double mktRate = instruments[i]->MarketRate();
        std::cout << std::setw(12) << instruments[i]->Name()
                  << std::setw(10) << mktRate * 100.0
                  << std::setw(12) << modelRate * 100.0
                  << std::setw(12) << (modelRate - mktRate) * 10000.0 << "\n";
    }

    return 0;
}


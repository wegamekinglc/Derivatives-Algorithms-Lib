//
// Created by wegam on 2020/12/21.
//

#include <iostream>
#include <iomanip>
#include <dal/platform/platform.hpp>
#include <dal/time/schedules.hpp>
#include <dal/time/dateincrement.hpp>
#include <dal/script/event.hpp>
#include <dal/model/blackscholes.hpp>
#include <dal/model/dupire.hpp>
#include <dal/storage/globals.hpp>
#include <dal/utilities/timer.hpp>
#include <dal/script/simulation.hpp>

using namespace std;
using namespace Dal;
using namespace Dal::Script;
using Dal::AAD::Model_;
using Dal::AAD::Dupire_;


template <class T_>
T_ DigitalTest(const T_& spot, const T_& vol, const T_& rate, const T_& div, const T_& strike, const T_& expiry) {
    static const double M_SQRT_2 = 1.4142135623730951;
    T_ y(0.0);
    T_ sqrtVar = vol * sqrt(expiry);
    T_ d_minus = (log(spot / strike) + (rate - div) * expiry)/ sqrtVar - 0.5 * sqrtVar;
    y = exp(-rate * expiry) * 0.5 * erfc(-d_minus / M_SQRT_2);
    return y;
}


int main() {
    Dal::RegisterAll_::Init();

    const Date_ start = Date_(2022, 9, 25);
    const Date_ maturity = Date_(2025, 9, 25);
    auto days = (maturity - start);

    Global::Dates_::SetEvaluationDate(start);
    Timer_ timer;

    using Real_ = Number_;

    const double spot = 100.0;
    const double vol = 0.15;
    const double rate = 0.0;
    const double div = 0.0;
    const double strike = 120.0;
    const String_ fuzzy = "0.2";
    const int numPath = static_cast<int>(std::pow(2, 20));

    timer.Reset();

    Vector_<Cell_> eventDates;
    Vector_<String_> events;
    eventDates.push_back(Cell_("STRIKE"));
    events.push_back(ToString(strike));
    eventDates.push_back(Cell_(maturity));
    events.push_back(String_("IF spot() >= STRIKE:" + fuzzy + " THEN call pays 1.0 ELSE call pays 0 END"));

    constexpr int numObs = 1;

    Vector_<int> widths = {14, 14, 14, 14, 14, 14, 14, 14, 14, 14};
    std::cout << std::setw(widths[0]) << std::left << "Method"
              << std::setw(widths[1]) << std::right << "# of paths"
              << std::setw(widths[2]) << std::right << "# of obs"
              << std::setw(widths[3]) << std::right << "PV"
              << std::setw(widths[4]) << std::right << "delta"
              << std::setw(widths[5]) << std::right << "dP/dR"
              << std::setw(widths[6]) << std::right << "dP/dDiv"
              << std::setw(widths[7]) << std::right << "vega"
              << std::setw(widths[8]) << std::right << "dP/dK"
              << std::setw(widths[9]) << std::right << "Elapsed (ms)"
              << std::endl;
    {
        AAD::Clear(*AAD::Tape());
        timer.Reset();
        Number_ spotAad(spot);
        Number_ volAad(vol);
        Number_ rateAad(rate);
        Number_ divAad(div);
        Number_ strikeAad(strike);
        Number_ expiryAad((maturity - start) / 365.0);

        PutOnTape(spotAad);
        PutOnTape(volAad);
        PutOnTape(rateAad);
        PutOnTape(divAad);
        PutOnTape(strikeAad);
        PutOnTape(expiryAad);
        AAD::NewRecording(*AAD::Tape());

        auto priceAad = DigitalTest<Number_>(spotAad, volAad, rateAad, divAad, strikeAad, expiryAad);
        Adjoint(priceAad) = 1.0;
        AAD::PropagateToStart(*AAD::Tape());

        std::cout << std::setw(widths[0]) << std::left << "Analytic"
                  << std::setw(widths[1]) << std::right << ""
                  << std::setw(widths[2]) << std::right << numObs
                  << std::fixed << std::setprecision(6)
                  << std::setw(widths[3]) << std::right << Value(priceAad)
                  << std::setw(widths[4]) << std::right << Adjoint(spotAad)
                  << std::setw(widths[5]) << std::right << Adjoint(rateAad)
                  << std::setw(widths[6]) << std::right << Adjoint(divAad)
                  << std::setw(widths[7]) << std::right << Adjoint(volAad)
                  << std::setw(widths[8]) << std::right << Adjoint(strikeAad)
                  << std::setw(widths[9]) << std::right << int(timer.Elapsed<milliseconds>()) << std::endl;
    }

    {
        Handle_<ModelData_> modelData(new BSModelData_("bsmodel", spot, vol, rate, div));
        timer.Reset();

        ScriptProduct_ product(eventDates, events);
        product.PreProcess(false, false);
        SimResults_ results = MCSimulation<double>(product, modelData, numPath, String_("sobol"), false, false);

        auto calculated = results.aggregated_ / static_cast<double>(numPath);

        std::cout << std::setw(widths[0]) << std::left << "Non-AAD"
                  << std::setw(widths[1]) << std::right << numPath
                  << std::setw(widths[2]) << std::right << numObs
                  << std::fixed << std::setprecision(6)
                  << std::setw(widths[3]) << std::right << calculated
                  << std::setw(widths[4]) << std::right << "#NA"
                  << std::setw(widths[5]) << std::right << "#NA"
                  << std::setw(widths[6]) << std::right << "#NA"
                  << std::setw(widths[7]) << std::right << "#NA"
                  << std::setw(widths[8]) << std::right << "#NA"
                  << std::setw(widths[9]) << std::right << int(timer.Elapsed<milliseconds>()) << std::endl;
    }

    {
        Handle_<ModelData_> modelData(new BSModelData_("bsmodel", spot, vol, rate, div));
        timer.Reset();

        ScriptProduct_ product(eventDates, events);
        product.PreProcess(false, false);
        SimResults_ results = MCSimulation<double>(product, modelData, numPath, String_("sobol"), false, true);

        auto calculated = results.aggregated_ / static_cast<double>(numPath);

        std::cout << std::setw(widths[0]) << std::left << "Non-AAD Comp"
                  << std::setw(widths[1]) << std::right << numPath
                  << std::setw(widths[2]) << std::right << numObs
                  << std::fixed << std::setprecision(6)
                  << std::setw(widths[3]) << std::right << calculated
                  << std::setw(widths[4]) << std::right << "#NA"
                  << std::setw(widths[5]) << std::right << "#NA"
                  << std::setw(widths[6]) << std::right << "#NA"
                  << std::setw(widths[7]) << std::right << "#NA"
                  << std::setw(widths[8]) << std::right << "#NA"
                  << std::setw(widths[9]) << std::right << int(timer.Elapsed<milliseconds>()) << std::endl;
    }

    {
        Handle_<ModelData_> modelData(new BSModelData_("bsmodel", spot, vol, rate, div));
        timer.Reset();

        ScriptProduct_ product(eventDates, events);
        product.PreProcess(false, false);
        SimResults_ results = MCSimulation<double>(product, modelData, numPath, String_("sobol"), false, false);
        auto calculated = results.aggregated_ / static_cast<double>(numPath);

        double eps = 0.0001;
        Handle_<ModelData_> modelDataDown(new BSModelData_("bsmodel", spot * (1 - eps), vol, rate, div));
        product.PreProcess(false, false);
        SimResults_ results_down = MCSimulation<double>(product, modelDataDown, numPath, String_("sobol"), false);
        auto calculatedDown = results_down.aggregated_ / static_cast<double>(numPath);

        Handle_<ModelData_> modelDataUp(new BSModelData_("bsmodel", spot * (1 + eps), vol, rate, div));
        product.PreProcess(false, false);
        SimResults_ results_up = MCSimulation<double>(product, modelDataUp, numPath, String_("sobol"), false);
        auto calculatedUp = results_up.aggregated_ / static_cast<double>(numPath);
        auto dSpot = (calculatedUp - calculatedDown) / (2 * spot * eps);

        double epsVol = eps;
        modelDataDown.reset(new BSModelData_("bsmodel", spot, vol - epsVol, rate, div));
        product.PreProcess(false, false);
        results_down = MCSimulation<double>(product, modelDataDown, numPath, String_("sobol"), false);
        calculatedDown = results_down.aggregated_ / static_cast<double>(numPath);

        modelDataUp.reset(new BSModelData_("bsmodel", spot, vol + epsVol, rate, div));
        product.PreProcess(false, false);
        results_up = MCSimulation<double>(product, modelDataUp, numPath, String_("sobol"), false);
        calculatedUp = results_up.aggregated_ / static_cast<double>(numPath);
        auto dVol = (calculatedUp - calculatedDown) / (2 * epsVol);

        double epsRate = std::abs(rate) > 0 ? abs(rate) * eps : eps;
        modelDataDown.reset(new BSModelData_("bsmodel", spot, vol, rate - epsRate, div));
        product.PreProcess(false, false);
        results_down = MCSimulation<double>(product, modelDataDown, numPath, String_("sobol"), false);
        calculatedDown = results_down.aggregated_ / static_cast<double>(numPath);

        modelDataUp.reset(new BSModelData_("bsmodel", spot, vol, rate + epsRate, div));
        product.PreProcess(false, false);
        results_up = MCSimulation<double>(product, modelDataUp, numPath, String_("sobol"), false);
        calculatedUp = results_up.aggregated_ / static_cast<double>(numPath);
        auto dRate = (calculatedUp - calculatedDown) / (2 * epsRate);

        double epsDiv = std::abs(div) > 0 ? abs(div) * eps : eps;
        modelDataDown.reset(new BSModelData_("bsmodel", spot, vol, rate, div - epsDiv));
        product.PreProcess(false, false);
        results_down = MCSimulation<double>(product, modelDataDown, numPath, String_("sobol"), false);
        calculatedDown = results_down.aggregated_ / static_cast<double>(numPath);

        modelDataUp.reset(new BSModelData_("bsmodel", spot, vol, rate, div + epsDiv));
        product.PreProcess(false, false);
        results_up = MCSimulation<double>(product, modelDataUp, numPath, String_("sobol"), false);
        calculatedUp = results_up.aggregated_ / static_cast<double>(numPath);
        auto dDiv = (calculatedUp - calculatedDown) / (2 * epsDiv);

        auto events_down = events;
        events_down[0] = ToString(strike * (1.0 - eps));
        ScriptProduct_ product_down(eventDates, events_down);
        product_down.PreProcess(false, false);
        results_down = MCSimulation<double>(product_down, modelData, numPath, String_("sobol"), false);
        calculatedDown = results_down.aggregated_ / static_cast<double>(numPath);

        auto events_up = events;
        events_up[0] = ToString(strike * (1.0 + eps));
        ScriptProduct_ product_up(eventDates, events_up);
        product_up.PreProcess(false, false);
        results_up = MCSimulation<double>(product_up, modelData, numPath, String_("sobol"), false);
        calculatedUp = results_up.aggregated_ / static_cast<double>(numPath);
        auto dStrike = (calculatedUp - calculatedDown) / (2 * strike * eps);

        std::cout << std::setw(widths[0]) << std::left << "FDM"
                  << std::setw(widths[1]) << std::right << numPath
                  << std::setw(widths[2]) << std::right << numObs
                  << std::fixed << std::setprecision(6)
                  << std::setw(widths[3]) << std::right << calculated
                  << std::setw(widths[4]) << std::right << dSpot
                  << std::setw(widths[5]) << std::right << dRate
                  << std::setw(widths[6]) << std::right << dDiv
                  << std::setw(widths[7]) << std::right << dVol
                  << std::setw(widths[8]) << std::right << dStrike
                  << std::setw(widths[9]) << std::right << int(timer.Elapsed<milliseconds>()) << std::endl;
    }

    {
        Handle_<ModelData_> modelData(new BSModelData_("bsmodel", spot, vol, rate, div));
        timer.Reset();

        ScriptProduct_ product(eventDates, events);
        int maxNestedIfs = product.PreProcess(true, true);
        SimResults_ results = MCSimulation<Number_>(product, modelData, numPath, String_("sobol"), false, false, maxNestedIfs);

        auto calculated = results.aggregated_ / static_cast<double>(numPath);

        std::cout << std::setw(widths[0]) << std::left << "AAD"
                  << std::setw(widths[1]) << std::right << numPath
                  << std::setw(widths[2]) << std::right << numObs
                  << std::fixed << std::setprecision(6)
                  << std::setw(widths[3]) << std::right << calculated
                  << std::setw(widths[4]) << std::right << results.risks_[0]
                  << std::setw(widths[5]) << std::right << results.risks_[2]
                  << std::setw(widths[6]) << std::right << results.risks_[3]
                  << std::setw(widths[7]) << std::right << results.risks_[1]
                  << std::setw(widths[8]) << std::right << results.risks_[4]
                  << std::setw(widths[9]) << std::right << int(timer.Elapsed<milliseconds>()) << std::endl;
    }

    {
        Handle_<ModelData_> modelData(new BSModelData_("bsmodel", spot, vol, rate, div));
        timer.Reset();

        ScriptProduct_ product(eventDates, events);
        int maxNestedIfs = product.PreProcess(true, true);
        SimResults_ results = MCSimulation<Number_>(product, modelData, numPath, String_("sobol"), false, true, maxNestedIfs);

        auto calculated = results.aggregated_ / static_cast<double>(numPath);

        std::cout << std::setw(widths[0]) << std::left << "AAD Comp"
                  << std::setw(widths[1]) << std::right << numPath
                  << std::setw(widths[2]) << std::right << numObs
                  << std::fixed << std::setprecision(6)
                  << std::setw(widths[3]) << std::right << calculated
                  << std::setw(widths[4]) << std::right << results.risks_[0]
                  << std::setw(widths[5]) << std::right << results.risks_[2]
                  << std::setw(widths[6]) << std::right << results.risks_[3]
                  << std::setw(widths[7]) << std::right << results.risks_[1]
                  << std::setw(widths[8]) << std::right << results.risks_[4]
                  << std::setw(widths[9]) << std::right << int(timer.Elapsed<milliseconds>()) << std::endl;
    }
    return 0;
}

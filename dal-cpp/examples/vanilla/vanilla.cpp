//
// Created by wegam on 2020/12/21.
//

#include <iostream>
#include <dal/time/dateincrement.hpp>
#include <dal/script/event.hpp>
#include <dal/model/blackscholes.hpp>
#include <dal/storage/globals.hpp>
#include <dal/utilities/timer.hpp>
#include <dal/script/simulation.hpp>
#include <dal/math/distribution/black.hpp>
#include <iomanip>


using namespace std;
using namespace Dal;
using namespace Dal::Script;
using Dal::AAD::Model_;
using Dal::AAD::BlackScholes_;


template <class T_>
T_ BlackTest(const T_& spot, const T_& vol, const T_& rate, const T_& div, const T_& strike, const T_& expiry, bool isCall) {
    static const double M_SQRT_2 = 1.4142135623730951;
    const double omega = isCall ? 1.0 : -1.0;
    T_ y(0.0);
    T_ numeraire = exp(-rate * expiry);
    T_ sqrtVar = vol * sqrt(expiry);
    T_ d_minus = (log(spot / strike) + (rate - div - 0.5 * vol * vol) * expiry) / sqrtVar;
    T_ d_plus = d_minus + sqrtVar;
    y = numeraire * omega * (0.5 * spot * exp((rate - div) * expiry) * erfc(-d_plus / M_SQRT_2) - strike * 0.5 * erfc(-d_minus / M_SQRT_2));
    return y;
}


int main() {
    RegisterAll_::Init();

    Global::Dates_::SetEvaluationDate(Date_(2022, 9, 25));
    Timer_ timer;

    const double spot = 100.0;
    const double vol = 0.15;
    const double rate = 0.05;
    const double div = 0.03;
    const double strike = 120.0;
    const String_ rsg = "sobol";
    const Date_ maturity(2025, 9, 24);
    const int numPath = std::pow(2, 20);
    const double expiry = 3.0;

    timer.Reset();

    Vector_<Cell_> eventDates(1, Cell_("STRIKE"));
    Vector_<String_> events(1, ToString(strike));
    eventDates.push_back(Cell_(maturity));
    events.push_back("call pays MAX(spot() - STRIKE, 0.0)");

    Vector_<int> widths = {14, 14, 14, 14, 14, 14, 14, 14, 14, 14};
    std::cout << std::setw(widths[0]) << std::left << "Method"
              << std::setw(widths[1]) << std::right << "# of paths"
              << std::setw(widths[2]) << std::right << "# of obs"
              << std::setw(widths[3]) << std::right << "PV"
              << std::setw(widths[4]) << std::right << "dP/dS"
              << std::setw(widths[5]) << std::right << "dP/dR"
              << std::setw(widths[6]) << std::right << "dP/dDiv"
              << std::setw(widths[7]) << std::right << "dP/dV"
              << std::setw(widths[8]) << std::right << "dP/dK"
              << std::setw(widths[9]) << std::right << "Elapsed (ms)"
              << std::endl;

    {
        // aadet
        AAD::Clear(*AAD::Tape());

        timer.Reset();
        Number_ spotAad(spot);
        Number_ volAad(vol);
        Number_ rateAad(rate);
        Number_ divAad(div);
        Number_ strikeAad(strike);
        Number_ expiryAad(expiry);

        PutOnTape(spotAad);
        PutOnTape(volAad);
        PutOnTape(rateAad);
        PutOnTape(divAad);
        PutOnTape(strikeAad);
        PutOnTape(expiryAad);
        AAD::NewRecording(*AAD::Tape());

        AAD::Rewind(*AAD::Tape());
        Number_ priceAad = BlackTest(spotAad, volAad, rateAad, divAad, strikeAad, expiryAad, true);
        Adjoint(priceAad) = 1.0;
        AAD::PropagateToStart(*AAD::Tape());

        std::cout << std::setw(widths[0]) << std::left << "Analytical"
                  << std::setw(widths[1]) << std::right << "-"
                  << std::setw(widths[2]) << std::right << 1
                  << std::fixed
                  << std::setprecision(6)
                  << std::setw(widths[3]) << std::right << Value(priceAad)
                  << std::setw(widths[4]) << std::right << Adjoint(spotAad)
                  << std::setw(widths[5]) << std::right << Adjoint(volAad)
                  << std::setw(widths[6]) << std::right << Adjoint(rateAad)
                  << std::setw(widths[7]) << std::right << Adjoint(divAad)
                  << std::setw(widths[8]) << std::right << Adjoint(strikeAad)
                  << std::setw(widths[9]) << std::right << int(timer.Elapsed<milliseconds>()) << std::endl;
    }

    {
        timer.Reset();
        Handle_<ModelData_> modelData(new BSModelData_("bsmodel", spot, vol, rate, div));
        ScriptProduct_ product(eventDates, events, "call");
        product.PreProcess(false, false);
        SimResults_ results = MCSimulation<double>(product, modelData, numPath, String_("sobol"), false, false);

        auto calculated = results.aggregated_ / static_cast<double>(numPath);

        std::cout << std::setw(widths[0]) << std::left << "Non-AAD"
                  << std::setw(widths[1]) << std::right << numPath
                  << std::setw(widths[2]) << std::right << 1
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
        timer.Reset();
        Handle_<ModelData_> modelData(new BSModelData_("bsmodel", spot, vol, rate, div));
        ScriptProduct_ product(eventDates, events, "call");
        product.PreProcess(false, false);
        SimResults_ results = MCSimulation<double>(product, modelData, numPath, String_("sobol"), false, true);

        auto calculated = results.aggregated_ / static_cast<double>(numPath);

        std::cout << std::setw(widths[0]) << std::left << "Non-AAD Comp"
                  << std::setw(widths[1]) << std::right << numPath
                  << std::setw(widths[2]) << std::right << 1
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
        timer.Reset();
        Handle_<ModelData_> modelData(new BSModelData_("bsmodel", spot, vol, rate, div));

        timer.Reset();
        ScriptProduct_ product(eventDates, events, "call");
        int maxNestedIfs = product.PreProcess(false, false);
        SimResults_ results = MCSimulation<double>(product, modelData, numPath, String_("sobol"), false);
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

        modelDataDown.reset(new BSModelData_("bsmodel", spot , vol * (1 - eps), rate, div));
        product.PreProcess(false, false);
        results_down = MCSimulation<double>(product, modelDataDown, numPath, String_("sobol"), false);
        calculatedDown = results_down.aggregated_ / static_cast<double>(numPath);

        modelDataUp.reset(new BSModelData_("bsmodel", spot , vol * (1 + eps), rate, div));
        product.PreProcess(false, false);
        results_up = MCSimulation<double>(product, modelDataUp, numPath, String_("sobol"), false);
        calculatedUp = results_up.aggregated_ / static_cast<double>(numPath);
        auto dVol = (calculatedUp - calculatedDown) / (2 * vol * eps);

        modelDataDown.reset(new BSModelData_("bsmodel", spot , vol, rate - eps, div));
        product.PreProcess(false, false);
        results_down = MCSimulation<double>(product, modelDataDown, numPath, String_("sobol"), false);
        calculatedDown = results_down.aggregated_ / static_cast<double>(numPath);

        modelDataUp.reset(new BSModelData_("bsmodel", spot , vol, rate + eps, div));
        product.PreProcess(false, false);
        results_up = MCSimulation<double>(product, modelDataUp, numPath, String_("sobol"), false);
        calculatedUp = results_up.aggregated_ / static_cast<double>(numPath);
        auto dRate = (calculatedUp - calculatedDown) / (2 * eps);

        modelDataDown.reset(new BSModelData_("bsmodel", spot , vol, rate, div - eps));
        product.PreProcess(false, false);
        results_down = MCSimulation<double>(product, modelDataDown, numPath, String_("sobol"), false);
        calculatedDown = results_down.aggregated_ / static_cast<double>(numPath);

        modelDataUp.reset(new BSModelData_("bsmodel", spot , vol, rate, div + eps));
        product.PreProcess(false, false);
        results_up = MCSimulation<double>(product, modelDataUp, numPath, String_("sobol"), false);
        calculatedUp = results_up.aggregated_ / static_cast<double>(numPath);
        auto dDiv = (calculatedUp - calculatedDown) / (2 * eps);

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
                  << std::setw(widths[2]) << std::right << 1
                  << std::fixed
                  << std::setprecision(6)
                  << std::setw(widths[3]) << std::right << calculated
                  << std::setw(widths[4]) << std::right << dSpot
                  << std::setw(widths[5]) << std::right << dVol
                  << std::setw(widths[6]) << std::right << dRate
                  << std::setw(widths[7]) << std::right << dDiv
                  << std::setw(widths[8]) << std::right << dStrike
                  << std::setw(widths[9]) << std::right << int(timer.Elapsed<milliseconds>())
                  << std::endl;
    }

    {
        timer.Reset();
        Handle_<ModelData_> modelData(new BSModelData_("bsmodel", spot, vol, rate, div));
        ScriptProduct_ product(eventDates, events, "call");
        int maxNestedIfs = product.PreProcess(true, true);
        SimResults_ results = MCSimulation<Number_>(product, modelData, numPath, String_("sobol"), false, false, maxNestedIfs);

        auto calculated = results.aggregated_ / static_cast<double>(numPath);

        std::cout << std::setw(widths[0]) << std::left << "AAD"
                  << std::setw(widths[1]) << std::right << numPath
                  << std::setw(widths[2]) << std::right << 1
                  << std::fixed << std::setprecision(6)
                  << std::setw(widths[3]) << std::right << calculated
                  << std::setw(widths[4]) << std::right << results.risks_[0]
                  << std::setw(widths[5]) << std::right << results.risks_[1]
                  << std::setw(widths[6]) << std::right << results.risks_[2]
                  << std::setw(widths[7]) << std::right << results.risks_[3]
                  << std::setw(widths[8]) << std::right << results.risks_[4]
                  << std::setw(widths[9]) << std::right << int(timer.Elapsed<milliseconds>()) << std::endl;
    }

    {
        timer.Reset();
        Handle_<ModelData_> modelData(new BSModelData_("bsmodel", spot, vol, rate, div));
        ScriptProduct_ product(eventDates, events, "call");
        int maxNestedIfs = product.PreProcess(true, true);
        SimResults_ results = MCSimulation<Number_>(product, modelData, numPath, String_("sobol"), false, true, maxNestedIfs);

        auto calculated = results.aggregated_ / static_cast<double>(numPath);

        std::cout << std::setw(widths[0]) << std::left << "AAD Comp"
                  << std::setw(widths[1]) << std::right << numPath
                  << std::setw(widths[2]) << std::right << 1
                  << std::fixed << std::setprecision(6)
                  << std::setw(widths[3]) << std::right << calculated
                  << std::setw(widths[4]) << std::right << results.risks_[0]
                  << std::setw(widths[5]) << std::right << results.risks_[1]
                  << std::setw(widths[6]) << std::right << results.risks_[2]
                  << std::setw(widths[7]) << std::right << results.risks_[3]
                  << std::setw(widths[8]) << std::right << results.risks_[4]
                  << std::setw(widths[9]) << std::right << int(timer.Elapsed<milliseconds>()) << std::endl;
    }
    return 0;
}

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
T_ BlackTest(const T_& spot, const T_& vol, const T_& rate, const T_& div, const T_& strike, const T_& expiry, bool is_call) {
    static const double M_SQRT_2 = 1.4142135623730951;
    const double omega = is_call ? 1.0 : -1.0;
    T_ y(0.0);
    T_ numeraire = exp(-rate * expiry);
    T_ sqrt_var = vol * sqrt(expiry);
    T_ d_minus = (log(spot / strike) + (rate - div - 0.5 * vol * vol) * expiry) / sqrt_var;
    T_ d_plus = d_minus + sqrt_var;
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
    const int num_path = std::pow(2, 20);
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
        Number_::Tape()->Clear();

        timer.Reset();
        Number_ spot_aad(spot);
        Number_ vol_aad(vol);
        Number_ rate_aad(rate);
        Number_ div_aad(div);
        Number_ strike_aad(strike);
        Number_ expiry_aad(expiry);

        spot_aad.PutOnTape();
        vol_aad.PutOnTape();
        rate_aad.PutOnTape();
        div_aad.PutOnTape();
        strike_aad.PutOnTape();
        expiry_aad.PutOnTape();

        Number_::Tape()->Rewind();
        Number_ price_aad = BlackTest(spot_aad, vol_aad, rate_aad, div_aad, strike_aad, expiry_aad, true);
        price_aad.PropagateToStart();

        std::cout << std::setw(widths[0]) << std::left << "Analytical"
                  << std::setw(widths[1]) << std::right << "-"
                  << std::setw(widths[2]) << std::right << 1
                  << std::fixed
                  << std::setprecision(6)
                  << std::setw(widths[3]) << std::right << price_aad.value()
                  << std::setw(widths[4]) << std::right << spot_aad.Adjoint() 
                  << std::setw(widths[5]) << std::right << vol_aad.Adjoint()
                  << std::setw(widths[6]) << std::right << rate_aad.Adjoint()
                  << std::setw(widths[7]) << std::right << div_aad.Adjoint()
                  << std::setw(widths[8]) << std::right << strike_aad.Adjoint()
                  << std::setw(widths[9]) << std::right << int(timer.Elapsed<milliseconds>()) << std::endl;
    }

    {
        timer.Reset();
        Handle_<ModelData_> model_data(new BSModelData_("bsmodel", spot, vol, rate, div));
        ScriptProduct_ product(eventDates, events, "call");
        int max_nested_ifs = product.PreProcess(false, false);
        SimResults_ results = MCSimulation<double>(product, model_data, num_path, String_("sobol"), false, false);

        auto calculated = results.aggregated_ / static_cast<double>(num_path);

        std::cout << std::setw(widths[0]) << std::left << "Non-AAD"
                  << std::setw(widths[1]) << std::right << num_path
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
        Handle_<ModelData_> model_data(new BSModelData_("bsmodel", spot, vol, rate, div));

        timer.Reset();
        ScriptProduct_ product(eventDates, events, "call");
        int max_nested_ifs = product.PreProcess(false, false);
        SimResults_ results = MCSimulation<double>(product, model_data, num_path, String_("sobol"), false);
        auto calculated = results.aggregated_ / static_cast<double>(num_path);

        double eps = 0.0001;
        Handle_<ModelData_> model_data_down(new BSModelData_("bsmodel", spot * (1 - eps), vol, rate, div));
        product.PreProcess(false, false);
        SimResults_ results_down = MCSimulation<double>(product, model_data_down, num_path, String_("sobol"), false);
        auto calculated_down = results_down.aggregated_ / static_cast<double>(num_path);

        Handle_<ModelData_> model_data_up(new BSModelData_("bsmodel", spot * (1 + eps), vol, rate, div));
        product.PreProcess(false, false);
        SimResults_ results_up = MCSimulation<double>(product, model_data_up, num_path, String_("sobol"), false);
        auto calculated_up = results_up.aggregated_ / static_cast<double>(num_path);
        auto d_spot = (calculated_up - calculated_down) / (2 * spot * eps);

        model_data_down.reset(new BSModelData_("bsmodel", spot , vol * (1 - eps), rate, div));
        product.PreProcess(false, false);
        results_down = MCSimulation<double>(product, model_data_down, num_path, String_("sobol"), false);
        calculated_down = results_down.aggregated_ / static_cast<double>(num_path);

        model_data_up.reset(new BSModelData_("bsmodel", spot , vol * (1 + eps), rate, div));
        product.PreProcess(false, false);
        results_up = MCSimulation<double>(product, model_data_up, num_path, String_("sobol"), false);
        calculated_up = results_up.aggregated_ / static_cast<double>(num_path);
        auto d_vol = (calculated_up - calculated_down) / (2 * vol * eps);

        model_data_down.reset(new BSModelData_("bsmodel", spot , vol, rate - eps, div));
        product.PreProcess(false, false);
        results_down = MCSimulation<double>(product, model_data_down, num_path, String_("sobol"), false);
        calculated_down = results_down.aggregated_ / static_cast<double>(num_path);

        model_data_up.reset(new BSModelData_("bsmodel", spot , vol, rate + eps, div));
        product.PreProcess(false, false);
        results_up = MCSimulation<double>(product, model_data_up, num_path, String_("sobol"), false);
        calculated_up = results_up.aggregated_ / static_cast<double>(num_path);
        auto d_rate = (calculated_up - calculated_down) / (2 * eps);

        model_data_down.reset(new BSModelData_("bsmodel", spot , vol, rate, div - eps));
        product.PreProcess(false, false);
        results_down = MCSimulation<double>(product, model_data_down, num_path, String_("sobol"), false);
        calculated_down = results_down.aggregated_ / static_cast<double>(num_path);

        model_data_up.reset(new BSModelData_("bsmodel", spot , vol, rate, div + eps));
        product.PreProcess(false, false);
        results_up = MCSimulation<double>(product, model_data_up, num_path, String_("sobol"), false);
        calculated_up = results_up.aggregated_ / static_cast<double>(num_path);
        auto d_div = (calculated_up - calculated_down) / (2 * eps);

        auto events_down = events;
        events_down[0] = ToString(strike * (1.0 - eps));
        ScriptProduct_ product_down(eventDates, events_down);
        product_down.PreProcess(false, false);
        results_down = MCSimulation<double>(product_down, model_data, num_path, String_("sobol"), false);
        calculated_down = results_down.aggregated_ / static_cast<double>(num_path);

        auto events_up = events;
        events_up[0] = ToString(strike * (1.0 + eps));
        ScriptProduct_ product_up(eventDates, events_up);
        product_up.PreProcess(false, false);
        results_up = MCSimulation<double>(product_up, model_data, num_path, String_("sobol"), false);
        calculated_up = results_up.aggregated_ / static_cast<double>(num_path);
        auto d_strike = (calculated_up - calculated_down) / (2 * strike * eps);

        std::cout << std::setw(widths[0]) << std::left << "FDM"
                  << std::setw(widths[1]) << std::right << num_path
                  << std::setw(widths[2]) << std::right << 1
                  << std::fixed
                  << std::setprecision(6)
                  << std::setw(widths[3]) << std::right << calculated
                  << std::setw(widths[4]) << std::right << d_spot
                  << std::setw(widths[5]) << std::right << d_vol
                  << std::setw(widths[6]) << std::right << d_rate
                  << std::setw(widths[7]) << std::right << d_div
                  << std::setw(widths[8]) << std::right << d_strike
                  << std::setw(widths[9]) << std::right << int(timer.Elapsed<milliseconds>())
                  << std::endl;
    }

    {
        timer.Reset();
        Handle_<ModelData_> model_data(new BSModelData_("bsmodel", spot, vol, rate, div));
        ScriptProduct_ product(eventDates, events, "call");
        int max_nested_ifs = product.PreProcess(true, true);
        SimResults_ results = MCSimulation<Number_>(product, model_data, num_path, String_("sobol"), false, false, max_nested_ifs);

        auto calculated = results.aggregated_ / static_cast<double>(num_path);

        std::cout << std::setw(widths[0]) << std::left << "AAD"
                  << std::setw(widths[1]) << std::right << num_path
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
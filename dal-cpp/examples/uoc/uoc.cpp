//
// Created by wegam on 2020/12/21.
//

#include <charconv>
#include <iomanip>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>

#include <dal/model/blackscholes.hpp>
#include <dal/model/dupire.hpp>
#include <dal/platform/platform.hpp>
#include <dal/script/event.hpp>
#include <dal/script/simulation.hpp>
#include <dal/storage/globals.hpp>
#include <dal/time/dateincrement.hpp>
#include <dal/time/schedules.hpp>
#include <dal/utilities/timer.hpp>

using namespace std;
using namespace Dal;
using namespace Dal::Script;
using Dal::AAD::Dupire_;
using Dal::AAD::Model_;

namespace {
    std::optional<int> ParsePaths(int argc, char* argv[]) {
        if (argc > 2)
            return std::nullopt;
        if (argc == 1)
            return 1 << 20;
        int numPath = 0;
        const std::string_view text(argv[1]);
        const auto parsed = std::from_chars(text.data(), text.data() + text.size(), numPath);
        if (parsed.ec != std::errc() || parsed.ptr != text.data() + text.size() || numPath <= 0)
            return std::nullopt;
        return numPath;
    }

    struct FdmInputs_ {
        const Vector_<Cell_>& eventDates_;
        const Vector_<String_>& events_;
        const Vector_<double>& spots_;
        const Vector_<double>& times_;
        const Vector_<int>& widths_;
        int numPath_;
        int numObs_;
        double spot_;
        double vol_;
        double rate_;
        double div_;
        double strike_;
        double barrier_;
    };

    template <class F_> double CentralDifference(const F_& valueAt, double down, double up, double denominator) {
        const double valueDown = valueAt(down);
        const double valueUp = valueAt(up);
        return (valueUp - valueDown) / denominator;
    }

    double ModelValue(const FdmInputs_& input, const ScriptProduct_& product, double spot, double rate, double div) {
        Handle_<ModelData_> modelData(new DupireModelData_("dupiremodel", spot, rate, div, input.spots_, input.times_,
                                                           Matrix_<double>(input.spots_.size(), input.times_.size(), input.vol_)));
        return MCSimulation<double>(product, modelData, input.numPath_, String_("sobol"), false).aggregated_ / static_cast<double>(input.numPath_);
    }

    double EventValue(const FdmInputs_& input, const Handle_<ModelData_>& modelData, int eventIndex, double value) {
        auto events = input.events_;
        events[eventIndex] = ToString(value);
        ScriptProduct_ product(input.eventDates_, events);
        product.PreProcess(false, false);
        return MCSimulation<double>(product, modelData, input.numPath_, String_("sobol"), false).aggregated_ / static_cast<double>(input.numPath_);
    }

    void PrintFiniteDifferences(const FdmInputs_& input) {
        const auto& eventDates = input.eventDates_;
        const auto& events = input.events_;
        const auto& spots = input.spots_;
        const auto& times = input.times_;
        const auto& widths = input.widths_;
        const int numPath = input.numPath_;
        const int numObs = input.numObs_;
        const double spot = input.spot_;
        const double vol = input.vol_;
        const double rate = input.rate_;
        const double div = input.div_;
        const double strike = input.strike_;
        const double barrier = input.barrier_;

        Handle_<ModelData_> modelData(
            new DupireModelData_("dupiremodel", spot, rate, div, spots, times, Matrix_<double>(spots.size(), times.size(), vol)));
        Timer_ timer;
        timer.Reset();

        ScriptProduct_ product(eventDates, events);
        product.PreProcess(false, false);
        SimResults_ results = MCSimulation<double>(product, modelData, numPath, String_("sobol"), false, false);
        auto calculated = results.aggregated_ / static_cast<double>(numPath);

        const double eps = 0.0001;
        const double dSpot = CentralDifference([&](double bumped) { return ModelValue(input, product, bumped, rate, div); }, spot * (1 - eps),
                                               spot * (1 + eps), 2 * spot * eps);

        const double epsRate = std::abs(rate) > 0 ? abs(rate) * eps : eps;
        const double dRate = CentralDifference([&](double bumped) { return ModelValue(input, product, spot, bumped, div); }, rate - epsRate,
                                               rate + epsRate, 2 * epsRate);

        const double epsDiv = std::abs(div) > 0 ? abs(rate) * eps : eps;
        const double dDiv =
            CentralDifference([&](double bumped) { return ModelValue(input, product, spot, rate, bumped); }, div - epsDiv, div + epsDiv, 2 * epsDiv);

        const double dStrike = CentralDifference([&](double bumped) { return EventValue(input, modelData, 0, bumped); }, strike * (1.0 - eps),
                                                 strike * (1.0 + eps), 2 * strike * eps);

        const double dBarrier = CentralDifference([&](double bumped) { return EventValue(input, modelData, 1, bumped); }, barrier * (1.0 - eps),
                                                  barrier * (1.0 + eps), 2 * barrier * eps);

        std::cout << std::setw(widths[0]) << std::left << "FDM" << std::setw(widths[1]) << std::right << numPath << std::setw(widths[2]) << std::right
                  << numObs << std::fixed << std::setprecision(6) << std::setw(widths[3]) << std::right << calculated << std::setw(widths[4])
                  << std::right << dSpot << std::setw(widths[5]) << std::right << dRate << std::setw(widths[6]) << std::right << dDiv
                  << std::setw(widths[7]) << std::right << "#NA" << std::setw(widths[8]) << std::right << dBarrier << std::setw(widths[9])
                  << std::right << dStrike << std::setw(widths[10]) << std::right << int(timer.Elapsed<milliseconds>()) << std::endl;
    }
} // namespace

int main(int argc, char* argv[]) {
    const auto parsedPaths = ParsePaths(argc, argv);
    if (!parsedPaths) {
        std::cerr << "Usage: uoc [paths] (positive integer)\n";
        return 1;
    }
    const int numPath = *parsedPaths;
    Dal::RegisterAll_::Init();

    const Date_ start = Date_(2022, 9, 25);
    const Date_ maturity = Date_(2025, 9, 25);

    Global::Dates_::SetEvaluationDate(start);
    Timer_ timer;

    using Real_ = Number_;

    const double spot = 100.0;
    const double vol = 0.15;
    const double rate = 0.0;
    const double div = 0.0;
    const double strike = 120.0;
    const double barrier = 150.0;
    const String_ freq = "1W";
    const String_ fuzzy = "0.1";

    timer.Reset();

    Vector_<Cell_> eventDates;
    Vector_<String_> events;
    eventDates.push_back(Cell_("STRIKE"));
    events.push_back(ToString(strike));
    eventDates.push_back(Cell_("BARRIER"));
    events.push_back(ToString(barrier));
    eventDates.push_back(Cell_(start));
    events.push_back("alive = 1");
    eventDates.push_back(Cell_("START: " + Date::ToString(start) + " END: " + Date::ToString(maturity) + " FREQ: " + freq));
    events.push_back("if spot() >= BARRIER:" + fuzzy + " then alive = 0 end");
    eventDates.push_back(Cell_(maturity));
    events.push_back(String_("call pays alive * MAX(spot() - STRIKE, 0.0)"));


    const int numObs = freq == "1W" ? 3 * 51 : 3 * 12;

    auto times = Vector::XRange(0.0, 5.0, 61);
    auto spots = Vector::XRange(50.0, 200.0, 31);

    Vector_<int> widths = {14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14};
    std::cout << '\n' << std::string(70, '=') << "\n  UOC pricing comparison\n"
              << std::string(70, '=') << "\n\n";
    std::cout << std::setw(widths[0]) << std::left << "Method"
              << std::setw(widths[1]) << std::right << "# of paths"
              << std::setw(widths[2]) << std::right << "# of obs"
              << std::setw(widths[3]) << std::right << "PV"
              << std::setw(widths[4]) << std::right << "delta"
              << std::setw(widths[5]) << std::right << "dP/dR"
              << std::setw(widths[6]) << std::right << "dP/dDiv"
              << std::setw(widths[7]) << std::right << "vega"
              << std::setw(widths[8]) << std::right << "dP/dB"
              << std::setw(widths[9]) << std::right << "dP/dK"
              << std::setw(widths[10]) << std::right << "Elapsed (ms)"
              << std::endl;
    std::cout << std::string(154, '-') << '\n';
    {
        Handle_<ModelData_> modelData(new DupireModelData_("dupiremodel",
                                                                      spot,
                                                                      rate,
                                                                      div,
                                                                      spots,
                                                                      times,
                                                                      Matrix_<double>(spots.size(),times.size(), vol)));
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
                  << std::setw(widths[9]) << std::right << "#NA"
                  << std::setw(widths[10]) << std::right << int(timer.Elapsed<milliseconds>()) << std::endl;
    }

    {
        Handle_<ModelData_> modelData(new DupireModelData_("dupiremodel",
                                                                      spot,
                                                                      rate,
                                                                      div,
                                                                      spots,
                                                                      times,
                                                                      Matrix_<double>(spots.size(),times.size(), vol)));
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
                  << std::setw(widths[9]) << std::right << "#NA"
                  << std::setw(widths[10]) << std::right << int(timer.Elapsed<milliseconds>()) << std::endl;
    }

    PrintFiniteDifferences({eventDates, events, spots, times, widths, numPath, numObs, spot, vol, rate, div, strike, barrier});

    {
        Handle_<ModelData_> modelData(new DupireModelData_("dupiremodel",
                                                                      spot,
                                                                      rate,
                                                                      div,
                                                                      spots,
                                                                      times,
                                                                      Matrix_<double>(spots.size(),times.size(), vol)));
        timer.Reset();

        ScriptProduct_ product(eventDates, events);
        int maxNestedIfs = product.PreProcess(true, true);
        SimResults_ results = MCSimulation<Number_>(product, modelData, numPath, String_("sobol"), false, false, maxNestedIfs);

        auto calculated = results.aggregated_ / static_cast<double>(numPath);
        const int volLength = 31 * 61;
        double vega = 0.0;

        for (auto i = 3; i < 3 + volLength; ++i)
            vega += results.risks_[i];

        std::cout << std::setw(widths[0]) << std::left << "AAD"
                  << std::setw(widths[1]) << std::right << numPath
                  << std::setw(widths[2]) << std::right << numObs
                  << std::fixed << std::setprecision(6)
                  << std::setw(widths[3]) << std::right << calculated
                  << std::setw(widths[4]) << std::right << results.risks_[0]
                  << std::setw(widths[5]) << std::right << results.risks_[1]
                  << std::setw(widths[6]) << std::right << results.risks_[2]
                  << std::setw(widths[7]) << std::right << vega
                  << std::setw(widths[8]) << std::right << results.risks_[3 + volLength]
                  << std::setw(widths[9]) << std::right << results.risks_[3 + volLength + 1]
                  << std::setw(widths[10]) << std::right << int(timer.Elapsed<milliseconds>()) << std::endl;
    }

    {
        Handle_<ModelData_> modelData(new DupireModelData_("dupiremodel",
                                                                      spot,
                                                                      rate,
                                                                      div,
                                                                      spots,
                                                                      times,
                                                                      Matrix_<double>(spots.size(),times.size(), vol)));
        timer.Reset();

        ScriptProduct_ product(eventDates, events);
        int maxNestedIfs = product.PreProcess(true, true);
        SimResults_ results = MCSimulation<Number_>(product, modelData, numPath, String_("sobol"), false, true, maxNestedIfs);

        auto calculated = results.aggregated_ / static_cast<double>(numPath);
        const int volLength = 31 * 61;
        double vega = 0.0;

        for (auto i = 3; i < 3 + volLength; ++i)
            vega += results.risks_[i];

        std::cout << std::setw(widths[0]) << std::left << "AAD Comp"
                  << std::setw(widths[1]) << std::right << numPath
                  << std::setw(widths[2]) << std::right << numObs
                  << std::fixed << std::setprecision(6)
                  << std::setw(widths[3]) << std::right << calculated
                  << std::setw(widths[4]) << std::right << results.risks_[0]
                  << std::setw(widths[5]) << std::right << results.risks_[1]
                  << std::setw(widths[6]) << std::right << results.risks_[2]
                  << std::setw(widths[7]) << std::right << vega
                  << std::setw(widths[8]) << std::right << results.risks_[3 + volLength]
                  << std::setw(widths[9]) << std::right << results.risks_[3 + volLength + 1]
                  << std::setw(widths[10]) << std::right << int(timer.Elapsed<milliseconds>()) << std::endl;
    }
    std::cout << std::string(154, '-') << "\n\n";
    return 0;
}

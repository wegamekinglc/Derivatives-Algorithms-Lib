//
// Created by dal-implementer on 2026/9/20.
//

#include <cmath>
#include <iomanip>
#include <iostream>
#include <string>

#include <dal/platform/platform.hpp>

#include <dal/math/distribution/black.hpp>
#include <dal/model/blackscholes.hpp>
#include <dal/platform/initall.hpp>
#include <dal/script/diagnostics.hpp>
#include <dal/script/simulation.hpp>
#include <dal/storage/globals.hpp>

using namespace std;
using namespace Dal;
using namespace Dal::Script;

namespace {
    //  Reference market of the LSMC acceptance suite: spot 100, vol 20%, rate 5%,
    //  no dividends, strike 100, maturity 18m. The zero-dividend grid keeps the
    //  early-exercise premium above half a percent of spot, so the checks below bite.
    constexpr double SPOT = 100.0;
    constexpr double VOL = 0.20;
    constexpr double RATE = 0.05;
    constexpr double DIV = 0.0;
    constexpr double STRIKE = 100.0;

    const Date_ EVAL_DATE(2026, 9, 20);
    const Date_ BERMUDAN_MID(2027, 9, 20);
    const Date_ MATURITY(2028, 3, 20);

    constexpr size_t N_PATHS = 1u << 18;

    double YearFracTo(const Date_& date) { return static_cast<double>(date - EVAL_DATE) / 365.0; }

    Handle_<ModelData_> Model() { return Handle_<ModelData_>(new BSModelData_("bs", SPOT, VOL, RATE, DIV)); }

    double EuropeanPut() {
        const double maturity = YearFracTo(MATURITY);
        const double fwd = SPOT * std::exp((RATE - DIV) * maturity);
        return std::exp(-RATE * maturity) * Distribution::BlackOpt(fwd, VOL * std::sqrt(maturity), STRIKE, OptionType_::Value_::PUT);
    }

    //  Put exercisable on the given dates only (EXERCISE-only product, no PAYS)
    ScriptProductData_ PutOnExerciseDates(const Vector_<Date_>& dates) {
        Vector_<Cell_> cells;
        for (const auto& d : dates)
            cells.emplace_back(d);
        const String_ text = "EXERCISE MAX(" + String_(std::to_string(STRIKE)) + " - spot(), 0.0)";
        return {"", cells, Vector_<String_>(dates.size(), text)};
    }

    double Value(const ScriptProductData_& product) {
        const auto results = MCSimulation<double>(product, Model(), N_PATHS, ScriptValuationSettings_(), MonteCarloSettings_());
        return results.aggregated_ / static_cast<double>(N_PATHS);
    }

    void Require(bool condition, const string& message) {
        if (!condition)
            throw std::runtime_error("american_put_mc: " + message);
    }
} // namespace

int main() {
    Dal::RegisterAll_::Init();
    const auto restoreDate = XGLOBAL::SetEvaluationDateInScope(EVAL_DATE);

    const double european = EuropeanPut();
    const double bermudan = Value(PutOnExerciseDates({BERMUDAN_MID, MATURITY}));

    Vector_<Date_> weekly;
    for (int i = 1; i <= 78; ++i)
        weekly.push_back(EVAL_DATE.AddDays(7 * i));
    const double american = Value(PutOnExerciseDates(weekly));

    //  More exercise opportunities can only add value, and the weekly premium over
    //  the European put clears the half-a-percent-of-spot bar of the PDE benchmark.
    Require(bermudan >= european - 1e-9, "Bermudan put below the European closed form");
    Require(american >= bermudan - 1e-9, "weekly-exercise put below the two-date Bermudan");
    Require(american - european > 0.005 * SPOT, "early-exercise premium under the benchmark floor");
    //  Immediate exercise at the first opportunity bounds the value from above.
    Require(american <= STRIKE * std::exp(-RATE * YearFracTo(EVAL_DATE.AddDays(7))), "put above the discounted-strike bound");

    //  The simulation diagnostic runs the same valuation and reports per-date
    //  regression and exercise statistics (dal.script-simulation/1).
    const String_ diagnostic =
        ExplainScriptSimulation(PutOnExerciseDates({BERMUDAN_MID, MATURITY}), Model(), 4096, ScriptValuationSettings_(), MonteCarloSettings_());
    const string needle = "\"schema\":\"dal.script-simulation/1\"";
    Require(string(diagnostic.data(), diagnostic.size()).find(needle) != string::npos, "simulation diagnostic schema missing");

    cout << fixed << setprecision(4);
    cout << "European put (closed form): " << european << '\n';
    cout << "Bermudan put, 2 exercise dates, " << N_PATHS << " paths: " << bermudan << '\n';
    cout << "American put, weekly exercise (78 dates): " << american << '\n';
    cout << "Early-exercise premium (weekly - European): " << american - european << '\n';
    cout << "\nSimulation diagnostic (4096 paths, two-date Bermudan):\n" << string(diagnostic.data(), diagnostic.size()) << endl;

    return 0;
}

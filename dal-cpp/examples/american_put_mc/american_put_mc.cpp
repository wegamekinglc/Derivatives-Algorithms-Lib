//
// Created by dal-implementer on 2026/9/20.
//

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include <dal/platform/platform.hpp>

#include <dal/math/distribution/black.hpp>
#include <dal/math/interp/interpcubic.hpp>
#include <dal/math/operators.hpp>
#include <dal/math/pde/pdegrid.hpp>
#include <dal/math/pde/thetascheme.hpp>
#include <dal/model/blackscholes.hpp>
#include <dal/platform/initall.hpp>
#include <dal/script/diagnostics.hpp>
#include <dal/script/simulation.hpp>
#include <dal/storage/globals.hpp>
#include <dal/utilities/timer.hpp>

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

    constexpr int DEFAULT_PRICING_PATHS = 1 << 17;
    constexpr int DEFAULT_TRAINING_PATHS = 1 << 14;

    std::optional<int> PathCount(std::string_view text) {
        int count = 0;
        const auto parsed = std::from_chars(text.data(), text.data() + text.size(), count);
        if (parsed.ec != std::errc() || parsed.ptr != text.data() + text.size() || count <= 0)
            return std::nullopt;
        return count;
    }

    void Usage(std::ostream& out) {
        out << "Usage: american_put_mc [pricing_paths [training_paths]]\n"
            << "  Positive integers; defaults: pricing_paths=" << DEFAULT_PRICING_PATHS << ", training_paths=" << DEFAULT_TRAINING_PATHS << '\n';
    }

    struct PathCounts_ {
        int pricing_ = DEFAULT_PRICING_PATHS;
        int training_ = DEFAULT_TRAINING_PATHS;
    };

    std::optional<PathCounts_> ParsePaths(int argc, char* argv[]) {
        PathCounts_ counts;
        for (int i = 1; i < argc; ++i) {
            const auto count = PathCount(argv[i]);
            const bool pricing = i == 1;
            if (!count) {
                cerr << "Invalid " << (pricing ? "pricing_paths: " : "training_paths: ") << argv[i] << "; expected an integer in 1..2147483647\n";
                Usage(cerr);
                return std::nullopt;
            }
            (pricing ? counts.pricing_ : counts.training_) = *count;
        }
        return counts;
    }

    double YearFracTo(const Date_& date) { return static_cast<double>(date - EVAL_DATE) / 365.0; }

    Handle_<ModelData_> Model() { return Handle_<ModelData_>(new BSModelData_("bs", SPOT, VOL, RATE, DIV)); }

    double EuropeanPut() {
        const double maturity = YearFracTo(MATURITY);
        const double fwd = SPOT * std::exp((RATE - DIV) * maturity);
        return std::exp(-RATE * maturity) * Distribution::BlackOpt(fwd, VOL * std::sqrt(maturity), STRIKE, OptionType_::Value_::PUT);
    }

    String_ PutPayoff() { return "MAX(" + String_(std::to_string(STRIKE)) + " - spot(), 0.0)"; }

    //  Put exercisable on the given dates only (EXERCISE-only product, no PAYS)
    ScriptProductData_ PutOnExerciseDates(const Vector_<Date_>& dates) {
        Vector_<Cell_> cells;
        for (const auto& d : dates)
            cells.emplace_back(d);
        const String_ text = "EXERCISE " + PutPayoff();
        return {"", cells, Vector_<String_>(dates.size(), text)};
    }

    template <class T_> SimResults_ Simulate(const ScriptProductData_& product, int nPaths, const MonteCarloSettings_& simulation) {
        return MCSimulation<T_>(product, Model(), nPaths, ScriptValuationSettings_(), simulation);
    }

    //  Continuously exercisable put on a Crank-Nicolson grid: after every backward
    //  step the solution is lifted to the immediate-exercise value.
    double AmericanPutPde() {
        constexpr int SPACE_STEPS = 600;
        constexpr int TIME_STEPS = 600;
        constexpr double MAX_X = 3.0 * STRIKE;
        const int numX = SPACE_STEPS + 1;

        const PDE::CoordinateVector_ x = PDE::MakeUniformGrid(0.0, MAX_X, numX);
        const Vector_<PDE::CoordinateVector_> grids(1, x);
        const Vector_<> loc = PDE::GridLocations(x);

        Vector_<std::shared_ptr<Cube_<>>> vals(1, std::make_shared<Cube_<>>(1, 1, numX));
        for (int k = 0; k < numX; ++k)
            (*vals[0])(0, 0, k) = std::max(STRIKE - loc[k], 0.0);
        Vector_<std::shared_ptr<Cube_<>>> next(1, std::make_shared<Cube_<>>(1, 1, numX));

        const Handle_<PDE::ScalarCoeff_> disc(PDE::NewConstCoeff(RATE));
        const Handle_<PDE::VectorCoeff_> mu(PDE::NewVectorCoeff([](double s) { return (RATE - DIV) * s; }));
        const Handle_<PDE::MatrixCoeff_> var(PDE::NewMatrixCoeff([](double s) { return VOL * VOL * s * s; }));
        PDE::ThetaScheme_ scheme(0.5);
        const double dt = YearFracTo(MATURITY) / TIME_STEPS;
        scheme.Prepare(dt, grids, *disc, *mu, *var);
        for (int n = 0; n < TIME_STEPS; ++n) {
            (*next[0])(0, 0, 0) = STRIKE;
            (*next[0])(0, 0, numX - 1) = 0.0;
            scheme(dt, grids, vals, *disc, *mu, *var, &next);
            vals.Swap(&next);
            for (int k = 0; k < numX; ++k)
                (*vals[0])(0, 0, k) = std::max((*vals[0])(0, 0, k), STRIKE - loc[k]);
        }

        const Cube_<>& value = *vals[0];
        const Vector_<> res(value.SliceBegin(0, 0), value.SliceEnd(0, 0));
        Interp::Boundary_ lhs(2, 0.);
        Interp::Boundary_ rhs(2, 0);
        std::unique_ptr<Interp1_> interp(Interp::NewCubic("cubic", loc, res, lhs, rhs));
        return (*interp)(SPOT);
    }
} // namespace

int main(int argc, char* argv[]) {
    if (argc == 2 && std::string_view(argv[1]) == "--help") {
        Usage(cout);
        return 0;
    }
    if (argc > 3) {
        Usage(cerr);
        return 1;
    }
    const auto counts = ParsePaths(argc, argv);
    if (!counts)
        return 1;
    const int nPaths = counts->pricing_;
    const int trainingPaths = counts->training_;
    MonteCarloSettings_ simulation;
    simulation.lsmcTrainingPaths_ = trainingPaths;

    Dal::RegisterAll_::Init();
    Global::Dates_::SetEvaluationDate(EVAL_DATE);

    Timer_ timer;

    timer.Reset();
    const double european = EuropeanPut();
    const int64_t europeanMs = timer.Elapsed<milliseconds>();

    //  A terminal PAYS uses ordinary Monte Carlo; no exercise-policy training is needed.
    const ScriptProductData_ europeanProduct("", {Cell_(MATURITY)}, {"pay PAYS " + PutPayoff()});
    timer.Reset();
    const SimResults_ europeanMcRes = Simulate<double>(europeanProduct, nPaths, simulation);
    const int64_t europeanMcMs = timer.Elapsed<milliseconds>();
    const double europeanMc = europeanMcRes.aggregated_ / static_cast<double>(nPaths);

    timer.Reset();
    const SimResults_ europeanAadRes = Simulate<AAD::Number_>(europeanProduct, nPaths, simulation);
    const int64_t europeanAadMs = timer.Elapsed<milliseconds>();
    const double europeanAad = europeanAadRes.aggregated_ / static_cast<double>(nPaths);

    const Vector_<Date_> bermudanDates = {BERMUDAN_MID, MATURITY};
    const ScriptProductData_ bermudanProduct = PutOnExerciseDates(bermudanDates);
    timer.Reset();
    const SimResults_ bermudanRes = Simulate<double>(bermudanProduct, nPaths, simulation);
    const int64_t bermudanMs = timer.Elapsed<milliseconds>();
    const double bermudan = bermudanRes.aggregated_ / static_cast<double>(nPaths);

    timer.Reset();
    const SimResults_ bermudanAadRes = Simulate<AAD::Number_>(bermudanProduct, nPaths, simulation);
    const int64_t bermudanAadMs = timer.Elapsed<milliseconds>();
    const double bermudanAad = bermudanAadRes.aggregated_ / static_cast<double>(nPaths);

    Vector_<Date_> weekly;
    for (int i = 1; i <= (MATURITY - EVAL_DATE) / 7; ++i)
        weekly.push_back(EVAL_DATE.AddDays(7 * i));
    const ScriptProductData_ americanProduct = PutOnExerciseDates(weekly);

    timer.Reset();
    const SimResults_ americanRes = Simulate<double>(americanProduct, nPaths, simulation);
    const int64_t americanMs = timer.Elapsed<milliseconds>();
    const double american = americanRes.aggregated_ / static_cast<double>(nPaths);

    timer.Reset();
    const SimResults_ aadRes = Simulate<AAD::Number_>(americanProduct, nPaths, simulation);
    const int64_t aadMs = timer.Elapsed<milliseconds>();
    const double americanAad = aadRes.aggregated_ / static_cast<double>(nPaths);

    timer.Reset();
    const double pde = AmericanPutPde();
    const int64_t pdeMs = timer.Elapsed<milliseconds>();

    //  The weekly grid and the two-date set are not nested, so their ordering is
    //  not a theorem — with these fixed seeds the gap is ~0.4; the guards catch
    //  gross regressions. The weekly premium over the European put clears the
    //  half-a-percent-of-spot bar, and immediate exercise at the first
    //  opportunity bounds the value from above. The PDE value prices continuous
    //  exercise, and the AAD replay must reproduce the weekly price within the
    //  simulation-noise tolerance, with one-sided greeks.
    constexpr double MC_TOL = 0.05;
    REQUIRE(std::fabs(europeanMc - european) < MC_TOL, "European Monte Carlo and closed-form values disagree");
    REQUIRE(std::fabs(europeanAad - european) < MC_TOL, "European AAD and closed-form values disagree");
    REQUIRE(europeanAadRes["spot"] < 0.0, "European put delta must be negative");
    REQUIRE(europeanAadRes["vol"] > 0.0, "European put vega must be positive");
    REQUIRE(bermudan >= european - 1e-3, "Bermudan put below the European closed form");
    REQUIRE(std::fabs(bermudanAad - bermudan) < MC_TOL, "Bermudan AAD and non-AAD values disagree");
    REQUIRE(bermudanAadRes["spot"] < 0.0, "Bermudan put delta must be negative");
    REQUIRE(bermudanAadRes["vol"] > 0.0, "Bermudan put vega must be positive");
    REQUIRE(american >= bermudan - 1e-3, "weekly-exercise put below the two-date Bermudan");
    REQUIRE(american - european > 0.005 * SPOT, "early-exercise premium under the benchmark floor");
    REQUIRE(american <= STRIKE * std::exp(-RATE * YearFracTo(EVAL_DATE.AddDays(7))), "put above the discounted-strike bound");
    REQUIRE(pde >= bermudan - 1e-3, "PDE put below the two-date Bermudan");
    REQUIRE(std::fabs(american - pde) < MC_TOL, "weekly Monte Carlo and PDE values disagree");
    REQUIRE(std::fabs(americanAad - american) < MC_TOL, "AAD and non-AAD values disagree");
    REQUIRE(aadRes["spot"] < 0.0, "put delta must be negative");
    REQUIRE(aadRes["vol"] > 0.0, "put vega must be positive");

    //  The simulation diagnostic runs the same valuation and reports per-date
    //  regression and exercise statistics (dal.script-simulation/1).
    const String_ diagnostic = ExplainScriptSimulation(bermudanProduct, Model(), nPaths, ScriptValuationSettings_(), simulation);
    const string needle = "\"schema\":\"dal.script-simulation/1\"";
    REQUIRE(string(diagnostic.data(), diagnostic.size()).find(needle) != string::npos, "simulation diagnostic schema missing");

    Vector_<int> widths = {22, 14, 16, 12, 12, 12, 12, 12, 12, 14};
    cout << fixed << setprecision(4);
    cout << setw(widths[0]) << left << "Method" << setw(widths[1]) << right << "# of ex dates" << setw(widths[2]) << right << "Pricing paths"
         << setw(widths[2]) << right << "Training paths" << setw(widths[3]) << right << "price" << setw(widths[4]) << right << "premium"
         << setw(widths[5]) << right << "dP/dS" << setw(widths[6]) << right << "dP/dV" << setw(widths[7]) << right << "dP/dR" << setw(widths[8])
         << right << "dP/dDiv" << setw(widths[9]) << right << "Elapsed (ms)" << endl;
    cout << setw(widths[0]) << left << "European (closed form)" << setw(widths[1]) << right << "-" << setw(widths[2]) << right << "-"
         << setw(widths[2]) << right << "-" << setw(widths[3]) << right << european << setw(widths[4]) << right << "-" << setw(widths[5]) << right
         << "-" << setw(widths[6]) << right << "-" << setw(widths[7]) << right << "-" << setw(widths[8]) << right << "-" << setw(widths[9]) << right
         << europeanMs << endl;
    cout << setw(widths[0]) << left << "European (MC)" << setw(widths[1]) << right << 0 << setw(widths[2]) << right << nPaths << setw(widths[2])
         << right << "-" << setw(widths[3]) << right << europeanMc << setw(widths[4]) << right << "-" << setw(widths[5]) << right << "-"
         << setw(widths[6]) << right << "-" << setw(widths[7]) << right << "-" << setw(widths[8]) << right << "-" << setw(widths[9]) << right
         << europeanMcMs << endl;
    cout << setw(widths[0]) << left << "European (MC AAD)" << setw(widths[1]) << right << 0 << setw(widths[2]) << right << nPaths << setw(widths[2])
         << right << "-" << setw(widths[3]) << right << europeanAad << setw(widths[4]) << right << "-" << setw(widths[5]) << right
         << europeanAadRes["spot"] << setw(widths[6]) << right << europeanAadRes["vol"] << setw(widths[7]) << right << europeanAadRes["rate"]
         << setw(widths[8]) << right << europeanAadRes["div"] << setw(widths[9]) << right << europeanAadMs << endl;
    cout << setw(widths[0]) << left << "Bermudan (2 dates)" << setw(widths[1]) << right << bermudanDates.size() << setw(widths[2]) << right << nPaths
         << setw(widths[2]) << right << trainingPaths << setw(widths[3]) << right << bermudan << setw(widths[4]) << right << bermudan - european
         << setw(widths[5]) << right << "-" << setw(widths[6]) << right << "-" << setw(widths[7]) << right << "-" << setw(widths[8]) << right << "-"
         << setw(widths[9]) << right << bermudanMs << endl;
    cout << setw(widths[0]) << left << "Bermudan (2 dates AAD)" << setw(widths[1]) << right << bermudanDates.size() << setw(widths[2]) << right
         << nPaths << setw(widths[2]) << right << trainingPaths << setw(widths[3]) << right << bermudanAad << setw(widths[4]) << right
         << bermudanAad - european << setw(widths[5]) << right << bermudanAadRes["spot"] << setw(widths[6]) << right << bermudanAadRes["vol"]
         << setw(widths[7]) << right << bermudanAadRes["rate"] << setw(widths[8]) << right << bermudanAadRes["div"] << setw(widths[9]) << right
         << bermudanAadMs << endl;
    cout << setw(widths[0]) << left << "American (weekly)" << setw(widths[1]) << right << weekly.size() << setw(widths[2]) << right << nPaths
         << setw(widths[2]) << right << trainingPaths << setw(widths[3]) << right << american << setw(widths[4]) << right << american - european
         << setw(widths[5]) << right << "-" << setw(widths[6]) << right << "-" << setw(widths[7]) << right << "-" << setw(widths[8]) << right << "-"
         << setw(widths[9]) << right << americanMs << endl;
    cout << setw(widths[0]) << left << "American (weekly AAD)" << setw(widths[1]) << right << weekly.size() << setw(widths[2]) << right << nPaths
         << setw(widths[2]) << right << trainingPaths << setw(widths[3]) << right << americanAad << setw(widths[4]) << right << americanAad - european
         << setw(widths[5]) << right << aadRes["spot"] << setw(widths[6]) << right << aadRes["vol"] << setw(widths[7]) << right << aadRes["rate"]
         << setw(widths[8]) << right << aadRes["div"] << setw(widths[9]) << right << aadMs << endl;
    cout << setw(widths[0]) << left << "American (PDE)" << setw(widths[1]) << right << "continuous" << setw(widths[2]) << right << "-"
         << setw(widths[2]) << right << "-" << setw(widths[3]) << right << pde << setw(widths[4]) << right << pde - european << setw(widths[5])
         << right << "-" << setw(widths[6]) << right << "-" << setw(widths[7]) << right << "-" << setw(widths[8]) << right << "-" << setw(widths[9])
         << right << pdeMs << endl;
    cout << "\nSimulation diagnostic (" << nPaths << " pricing paths, " << trainingPaths << " training paths, two-date Bermudan):\n"
         << string(diagnostic.data(), diagnostic.size()) << endl;

    return 0;
}

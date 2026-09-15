// Emit the same workbook fixtures through the installed public C++ API.
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>

#include <dal-public/src/curveprotocol.hpp>
#include <dal-public/src/global.hpp>
#include <dal-public/src/script.hpp>
#include <dal-public/src/value.hpp>

using namespace Dal;

int main(int argc, char** argv) {
    REQUIRE(argc == 2, "expected output directory");
    const std::filesystem::path output(argv[1]);
    std::filesystem::create_directories(output);
    InitGlobalData(1);
    const Date_ d(2026, 9, 12), h(2026, 9, 11), p(2026, 9, 22);
    ScriptValuationSettings_ valuation;
    valuation.evaluationDate_ = d;
    valuation.modelBindings_ = {{"spot", "EQ[AAPL]"}};
    valuation.fixings_ = MarketFixingSnapshotNew({{"EQ[AAPL]", {{DateTime_(h, 0.), 80.}, {DateTime_(d, 0.), 80.}}}});
    const Handle_<ModelData_> model(new BSModelData_("zero-vol", 100., 0., 0., 0.));
    const auto mixed = NewScriptProduct("mixed", {Cell_("SCALE"), Cell_(h), Cell_(p)},
                                        {"2.0", "x = SCALE * FIX(EQ[AAPL])", "pay PAYS x + FIX(EQ[AAPL], 2026-09-15)"}, {"EQ[AAPL]"});
    const String_ name = "quote\"slash\\newline\n\xe4\xb8\xad\xf0\x9f\x98\x80-END";
    const auto longProduct = NewScriptProduct(name, Vector_<Cell_>(500, Cell_(p)), Vector_<String_>(500, "pay PAYS FIX(EQ[AAPL], 2026-09-11)"));
    const auto write = [&](const char* file, const String_& json) {
        std::ofstream stream(output / file, std::ios::binary);
        stream.write(json.data(), static_cast<std::streamsize>(json.size()));
        REQUIRE(stream.good(), "cannot write native diagnostic");
    };
    write("diagnostic-A2.json", DescribeScriptProduct(mixed));
    write("diagnostic-D2.json", ExplainScriptValuation(mixed, model, valuation));
    write("diagnostic-G2.json", DescribeScriptProduct(longProduct));
    write("diagnostic-J2.json", ExplainScriptValuation(longProduct, model, valuation));
    const auto pv = ValueByMonteCarlo(mixed, model, 257, valuation).at("PV");
    std::cout << std::setprecision(17) << "mixed PV=" << pv << "; expected=260; tolerance=2.6e-10\n";
    REQUIRE(std::abs(pv - 260.) <= 2.6e-10, "mixed independent oracle");

    const auto historical =
        NewScriptProduct("historical", {Cell_("SCALE"), Cell_(h), Cell_(p)}, {"2.0", "x = SCALE * FIX(EQ[AAPL])", "pay PAYS x"}, {"EQ[AAPL]"});
    const auto future = NewScriptProduct("future", {Cell_(p)}, {"pay PAYS FIX(EQ[AAPL], 2026-09-15)"});
    const Handle_<ModelData_> carry(new BSModelData_("carry", 100., 0., .05, .02));
    const MonteCarloSettings_ simulation{"sobol", false, true, .01, true};
    const std::map<String_, std::map<String_, double>> prices = {
        {"historical", ValueByMonteCarlo(historical, carry, 257, valuation, simulation)},
        {"historical_zero", ValueByMonteCarlo(historical, model, 257, valuation, simulation)},
        {"mixed", ValueByMonteCarlo(mixed, model, 257, valuation, simulation)},
        {"future", ValueByMonteCarlo(future, carry, 257, valuation, simulation)}};
    const double tp = 10. / 365., tf = 3. / 365.;
    const double hpv = 160. * std::exp(-.05 * tp), fpv = 100. * std::exp(.03 * tf - .05 * tp);
    const std::map<String_, std::map<String_, double>> oracles = {
        {"historical", {{"PV", hpv}, {"d_SCALE", hpv / 2.}, {"d_rate", -tp * hpv}, {"d_spot", 0.}, {"d_vol", 0.}, {"d_div", 0.}}},
        {"historical_zero", {{"PV", 160.}, {"d_SCALE", 80.}, {"d_rate", -tp * 160.}, {"d_spot", 0.}, {"d_vol", 0.}, {"d_div", 0.}}},
        {"mixed", {{"PV", 260.}, {"d_SCALE", 80.}, {"d_spot", 1.}}},
        {"future", {{"PV", fpv}, {"d_spot", fpv / 100.}, {"d_rate", (tf - tp) * fpv}, {"d_div", -tf * fpv}}}};
    std::ofstream stream(output / "prices.json");
    stream << std::setprecision(17) << "{";
    bool firstScenario = true;
    for (const auto& scenario : prices) {
        REQUIRE(scenario.second.size() == (scenario.first == "future" ? 5 : 6), "unexpected price/risk key count");
        if (!firstScenario)
            stream << ',';
        firstScenario = false;
        stream << std::quoted(std::string(scenario.first.c_str())) << ":{";
        bool firstKey = true;
        for (const auto& value : scenario.second) {
            REQUIRE(std::isfinite(value.second), "nonfinite native price/risk");
            if (!firstKey)
                stream << ',';
            firstKey = false;
            stream << std::quoted(std::string(value.first.c_str())) << ':' << value.second;
        }
        stream << '}';
        for (const auto& expected : oracles.at(scenario.first)) {
            const double tolerance = expected.first == "PV" ? 1e-12 * std::max(1., std::abs(expected.second)) : 1e-10;
            REQUIRE(std::abs(scenario.second.at(expected.first) - expected.second) <= tolerance,
                    scenario.first + "/" + expected.first + " failed independent native PV/AAD oracle");
        }
    }
    stream << "}\n";
    REQUIRE(stream.good(), "cannot write native prices");
    std::cout << "PASS: four native PV/AAD scenarios and independent analytic oracles\n";
}

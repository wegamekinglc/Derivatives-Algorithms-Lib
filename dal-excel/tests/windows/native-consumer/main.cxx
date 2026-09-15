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
}

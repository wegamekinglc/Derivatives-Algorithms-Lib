#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>

#include <dal/platform/platform.hpp>
#include <dal/script/simulation.hpp>
#include <dal/storage/globals.hpp>

#include "compiledsoaprofile.hpp"

#if !defined(DAL_SOA_PROFILE_BASELINE)
#include <dal/script/compiledsoa.hpp>
#endif

using namespace Dal;
using namespace Dal::Script;

namespace {
    ScriptProduct_ Product(const std::string& name) {
        Vector_<Cell_> dates{Cell_("STRIKE")};
        Vector_<String_> events{"100.0"};
        if (name != "vanilla") {
            REQUIRE(name == "weekly" || name == "divergent", "Unknown SoA workload");
            dates.push_back(Cell_(Date_(2024, 1, 1)));
            events.push_back("alive = 1");
            dates.push_back(Cell_("START: 2024-01-01 END: 2025-01-01 FREQ: 1W"));
            events.push_back(name == "weekly" ? "if spot() >= 150 then alive = 0 end" : "if spot() >= 110 then alive = 0 else alive = 1 end");
        }
        dates.push_back(Cell_(Date_(2025, 1, 1)));
        events.push_back(name == "vanilla" ? "pay pays MAX(spot() - STRIKE, 0.0)" : "pay pays alive * MAX(spot() - STRIKE, 0.0)");
        ScriptProduct_ product(dates, events);
        product.PreProcess(false, false);
        return product;
    }

    size_t Positive(const char* value) {
        const std::string input(value);
        REQUIRE(!input.empty() && input.find_first_not_of("0123456789") == std::string::npos, "Expected positive integer");
        const auto result = std::stoull(input);
        REQUIRE(result > 0 && result <= std::numeric_limits<size_t>::max(), "Expected positive integer");
        return static_cast<size_t>(result);
    }

    template <size_t L_> double Value(const ScriptProduct_& product, size_t paths) {
        AAD::BlackScholes_<double> model(100.0, 0.2, 0.05, 0.02);
#if defined(DAL_SOA_PROFILE_BASELINE)
        return MCDoubleSimulation(product, &model, paths, "sobol", false, true).aggregated_;
#else
        return MCDoubleSimulationSoa<L_>(product, &model, paths).aggregated_;
#endif
    }

    template <size_t L_> void Measure(const std::string& name, size_t paths, size_t iterations, bool cold) {
        const auto product = Product(name);
        const auto run = [&] { return cold ? Value<L_>(Product(name), paths) : Value<L_>(product, paths); };
        const double expected = run();
        run();
        const auto start = std::chrono::steady_clock::now();
        double checksum = 0.0;
        for (size_t iteration = 0; iteration < iterations; ++iteration)
            checksum += run();
        const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        REQUIRE(std::isfinite(expected) && std::isfinite(checksum), "Non-finite benchmark payoff");
        std::cout << std::setprecision(17) << "{\"workload\":\"" << name << "\",\"lanes\":" << L_ << ",\"paths\":" << paths
                  << ",\"iterations\":" << iterations << ",\"mode\":\"" << (cold ? "cold" : "steady")
                  << "\",\"seconds\":" << elapsed / static_cast<double>(iterations) << ",\"sum\":" << expected << ",\"checksum\":" << checksum
                  << "}\n";
    }
} // namespace

int RunCompiledSoaProfile(int argc, char** argv) {
    try {
        REQUIRE(argc == 6, "Usage: script_mc_perf --compiled-soa vanilla|weekly|divergent LANES PATHS ITERATIONS steady|cold");
#if defined(DAL_SOA_PROFILE_STANDALONE)
        RegisterAll_::Init();
#endif
        ThreadPool_::GetInstance()->Start(1, true);
        const auto date = XGLOBAL::SetEvaluationDateInScope(Date_(2024, 1, 1));
        const size_t lanes = Positive(argv[2]), paths = Positive(argv[3]), iterations = Positive(argv[4]);
        const std::string mode(argv[5]);
        REQUIRE(mode == "steady" || mode == "cold", "Unknown cost mode");
        switch (lanes) {
        case 8:
            Measure<8>(argv[1], paths, iterations, mode == "cold");
            break;
        case 16:
            Measure<16>(argv[1], paths, iterations, mode == "cold");
            break;
        case 32:
            Measure<32>(argv[1], paths, iterations, mode == "cold");
            break;
        case 64:
            Measure<64>(argv[1], paths, iterations, mode == "cold");
            break;
        default:
            THROW("Unsupported lane width");
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

#if defined(DAL_SOA_PROFILE_STANDALONE)
int main(int argc, char** argv) { return RunCompiledSoaProfile(argc, argv); }
#endif

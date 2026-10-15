//
// Created by Codex on 2026/10/16.
//

#include <charconv>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>

#include <dal/benchmarks/bench.hpp>
#include <dal/platform/initall.hpp>
#include <dal/platform/platform.hpp>

#include "workload.hpp"

namespace Dal::Script::SoaStudy {
    struct Options_ {
        std::string workload_ = "weekly", engine_ = "soa", phase_ = "eval";
        size_t paths_ = 100000, lanes_ = 32, iterations_ = 1;
        bool fallback_ = true;
    };

    Options_ ReadOptions(int argc, char** argv) {
        REQUIRE(argc % 2 == 1, "expected option/value pairs");
        Options_ result;
        for (int i = 1; i < argc; i += 2) {
            const std::string key(argv[i]), value(argv[i + 1]);
            if (key == "--workload")
                result.workload_ = value;
            else if (key == "--engine")
                result.engine_ = value;
            else if (key == "--phase")
                result.phase_ = value;
            else if (key == "--fallback") {
                REQUIRE(value == "true" || value == "false", "invalid fallback flag");
                result.fallback_ = value == "true";
            } else {
                size_t count = 0;
                const auto parsed = std::from_chars(value.data(), value.data() + value.size(), count);
                REQUIRE(parsed.ec == std::errc() && parsed.ptr == value.data() + value.size() && count > 0, "invalid count");
                if (key == "--paths")
                    result.paths_ = count;
                else if (key == "--lanes")
                    result.lanes_ = count;
                else if (key == "--iterations")
                    result.iterations_ = count;
                else
                    THROW("unknown option " + key);
            }
        }
        REQUIRE(result.paths_ <= 1000000 && result.iterations_ <= 1000000, "request too large");
        REQUIRE(result.engine_ == "scalar" || result.engine_ == "soa", "invalid engine");
        REQUIRE(result.phase_ == "eval" || result.phase_ == "model" || result.phase_ == "pipeline" || result.phase_ == "end_to_end", "invalid phase");
        return result;
    }

    template <size_t L_> int Run(const Options_& options) {
        const auto product = Product(options.workload_);
        const auto compiled = product.Compile();
        const Plan_ plan(compiled, product.VarValues(), product.ConstVarValues());
        typename Plan_::State_<L_> state(plan);
        auto scalarState = product.BuildEvalState<double>();
        const BatchModel_ batch(product.TimeLine(), product.DefLine());
        AAD::BlackScholes_<double> model(100.0, 0.2, 0.05, 0.02);
        model.Allocate(product.TimeLine(), product.DefLine());
        model.Init(product.TimeLine(), product.DefLine());
        const auto shocks = Shocks(options.paths_, model.SimDim());
        AAD::Scenario_<double> path;
        AAD::AllocatePath(product.DefLine(), path);
        AAD::InitializePath(path);
        const size_t events = path.size();
        const size_t blocks = (options.paths_ + L_ - 1) / L_;
        std::vector<std::vector<Row_<L_>>> inputs(blocks, std::vector<Row_<L_>>(events));
        Vector_<AAD::Scenario_<double>> scalarInputs;
        if (options.phase_ == "eval")
            scalarInputs.Resize(options.paths_);
        for (size_t p = 0; p < options.paths_; ++p) {
            model.GeneratePath(shocks[p], &path);
            if (options.phase_ == "eval")
                scalarInputs[p] = path;
            for (size_t e = 0; e < events; ++e)
                inputs[p / L_][e][p % L_] = path[e].spot_;
        }
        std::vector<Row_<L_>> generated(events);
        Vector_<Vector_<>> randomBlock(L_, Vector_<>(model.SimDim()));
        Vector_<> randomPath(model.SimDim());
        Counters_ counters;
        const bool soa = options.engine_ == "soa" && options.paths_ >= 8;
        double checksum = 0.0;
        auto scalarPayoff = [&](size_t p, bool generate, const Vector_<>& normal) {
            if (generate)
                model.GeneratePath(normal, &path);
            if (options.phase_ == "model") {
                Bench::DoNotOptimize(path.data());
                return path.back().spot_;
            }
            compiled.Evaluate(generate ? path : scalarInputs[p], scalarState);
            return scalarState.VarVals()[product.PayOffIdx()];
        };
        auto request = [&]() {
            const bool random = options.phase_ == "end_to_end";
            auto rng = random ? NewSobol(static_cast<int>(model.SimDim()), 0, false, false) : nullptr;
            double sum = 0.0;
            if (!soa) {
                for (size_t p = 0; p < options.paths_; ++p) {
                    if (random)
                        rng->FillNormal(&randomPath);
                    sum += scalarPayoff(p, options.phase_ != "eval", random ? randomPath : shocks[p]);
                }
                return sum;
            }
            for (size_t block = 0; block < blocks; ++block) {
                const size_t first = block * L_, count = std::min(L_, options.paths_ - first);
                if (random)
                    for (size_t lane = 0; lane < count; ++lane)
                        rng->FillNormal(&randomBlock[lane]);
                if (options.phase_ != "eval")
                    batch.Generate(random ? randomBlock : shocks, random ? 0 : first, count, &generated);
                const auto& spots = options.phase_ == "eval" ? inputs[block] : generated;
                if (options.phase_ == "model") {
                    Bench::DoNotOptimize(spots.data());
                    for (size_t lane = 0; lane < count; ++lane)
                        sum += spots.back()[lane];
                } else {
                    plan.Evaluate(spots, batch.Numeraires(), count, options.fallback_, &state, &counters);
                    for (size_t lane = 0; lane < count; ++lane)
                        sum += state.variables_[product.PayOffIdx()][lane];
                }
            }
            return sum;
        };
        checksum = request();
        counters = {};
        const auto start = std::chrono::steady_clock::now();
        for (size_t i = 0; i < options.iterations_; ++i) {
            checksum = request();
            Bench::DoNotOptimize(&checksum);
        }
        const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() / options.iterations_;
        std::cout << std::setprecision(17) << "{\"workload\":\"" << options.workload_ << "\",\"engine\":\"" << options.engine_ << "\",\"phase\":\""
                  << options.phase_ << "\",\"paths\":" << options.paths_ << ",\"lanes\":" << L_ << ",\"iterations\":" << options.iterations_
                  << ",\"fallback\":" << (options.fallback_ ? "true" : "false") << ",\"seconds\":" << seconds
                  << ",\"pv\":" << checksum / options.paths_ << ",\"shock_fingerprint\":\"" << std::hex << ShockFingerprint(shocks) << std::dec
                  << "\",\"events\":" << events << ",\"mixed\":" << counters.mixed_ << ",\"fallback_lanes\":" << counters.fallbackLanes_ << "}\n";
        return 0;
    }
} // namespace Dal::Script::SoaStudy

int main(int argc, char** argv) {
    try {
        Dal::RegisterAll_::Init(1);
        const auto options = Dal::Script::SoaStudy::ReadOptions(argc, argv);
        switch (options.lanes_) {
        case 8:
            return Dal::Script::SoaStudy::Run<8>(options);
        case 16:
            return Dal::Script::SoaStudy::Run<16>(options);
        case 32:
            return Dal::Script::SoaStudy::Run<32>(options);
        case 64:
            return Dal::Script::SoaStudy::Run<64>(options);
        default:
            THROW("invalid lane width");
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

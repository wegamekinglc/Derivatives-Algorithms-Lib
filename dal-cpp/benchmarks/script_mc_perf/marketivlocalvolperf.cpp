#include <charconv>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <set>
#include <string>

#include <dal/platform/platform.hpp>

#include <dal/benchmarks/bench.hpp>
#include <dal/concurrency/threadpool.hpp>
#include <dal/model/factory.hpp>
#include <dal/script/marketivlocalvol.hpp>
#include <dal/script/simulation.hpp>

#include "../../examples/models/market_iv_local_vol/marketivexample.hpp"
#include "marketivlocalvolperf.hpp"

namespace {
    using namespace Dal;
    using Clock_ = std::chrono::steady_clock;

    template <class F_> double Milliseconds(F_ action) {
        const auto start = Clock_::now();
        action();
        return std::chrono::duration<double, std::milli>(Clock_::now() - start).count();
    }

    template <class F_> void Measure(const std::string& label, F_ action) {
        int loops = 1;
        while (Milliseconds([&] {
                   for (int i = 0; i < loops; ++i)
                       action();
               }) < 10. &&
               loops < 16384)
            loops *= 2;
        std::cout << "CALIBRATION case=" << label << " inner_loops=" << loops << " repeats=10\n";
        Bench::Print(Bench::Run(label, action, 1, 10, loops));
    }

    Script::ScriptProductData_ Asian() {
        Vector_<Cell_> dates;
        Vector_<String_> events;
        for (int i = 1; i <= 12; ++i) {
            dates.push_back(Cell_(MarketIvExample::Today().AddDays(365 * i / 12)));
            events.push_back(i == 1 ? "total = FIX(EQ[A])" : "total = total + FIX(EQ[A])");
        }
        events.back() += "\npay PAYS MAX(total / 12 - 100, 0)";
        return {"monthly-asian", dates, events};
    }

    Handle_<ModelData_> Legacy(const MarketIvDupireResult_& grid, double maxStep) {
        const auto& surface = grid.SurfaceData();
        Vector_<> times{0.};
        times.Append(surface.times_);
        for (const auto& term : grid.Snapshot().Request().market_.Terms())
            times.push_back((term.expiry_ - MarketIvExample::Today()) / 365.);
        std::sort(times.begin(), times.end());
        times.erase(std::unique(times.begin(), times.end()), times.end());
        Vector_<> logDf;
        for (const double t : times)
            logDf.push_back(-.03 * t);
        HybridSettings_ settings;
        settings.domesticCurrency_ = "USD";
        settings.components_ = {
            Handle_<HybridComponentData_>(new HybridLocalVolEquityData_(
                "equity", "EQ[A]", "USD", "W_EQ", 100., .01,
                Handle_<LocalVolSurfaceData_>(new LocalVolSurfaceData_("legacy-grid", surface.spots_, surface.times_, surface.vols_)), maxStep)),
            Handle_<HybridComponentData_>(new HybridLogDfRateData_("rate", "USD", times, logDf))};
        settings.correlation_ = Handle_<HybridCorrelationData_>(new HybridConstantCorrelationData_("correlation", {"W_EQ"}, Matrix_<>(1, 1, 1.)));
        return Handle_<ModelData_>(new HybridModelData_("legacy", settings));
    }

    void PrintExecution(const MarketIvLocalVolExecution_& x) {
        REQUIRE(x.perPathMarketQueries_ == 0 && x.preparedColumns_ == x.internalSteps_, "Invalid preparation counters");
        REQUIRE(x.numericPayloadBytes_ <= x.plannedNumericPayloadBytes_ && x.peakWorkspaceBytes_ <= x.plannedWorkspaceBytes_, "Invalid ledger");
        std::cout << "EXECUTION steps=" << x.internalSteps_ << " dimension=" << x.simulationDimension_ << " workers=" << x.workers_
                  << " columns=" << x.preparedColumns_ << " prepare_queries=" << x.preparedMarketQueries_
                  << " per_path_market_queries=" << x.perPathMarketQueries_ << " shared_bytes=" << x.sharedNumericBytes_
                  << " per_worker_bytes=" << x.perWorkerNumericBytes_ << " payload=" << x.numericPayloadBytes_
                  << " payload_plan=" << x.plannedNumericPayloadBytes_ << " workspace_peak=" << x.peakWorkspaceBytes_
                  << " workspace_plan=" << x.plannedWorkspaceBytes_ << '\n';
    }

    void FlatPair(const MarketIvDupireResult_& grid, const Handle_<MarketIvLocalVolModelData_>& data, size_t paths, bool oldFirst) {
        const auto old = Legacy(grid, data->Configuration().maxStep_);
        auto freshModel = CreateModel<double>(Handle_<ModelData_>(data)), oldModel = CreateModel<double>(old);
        Vector_<> times(grid.SurfaceData().times_);
        for (const auto& term : grid.Snapshot().Request().market_.Terms())
            times.push_back((term.expiry_ - MarketIvExample::Today()) / 365.);
        std::sort(times.begin(), times.end());
        times.erase(std::unique(times.begin(), times.end()), times.end());
        Vector_<AAD::SampleDef_> definitions(times.size());
        definitions.back().indexNames_ = {"EQ[A]"};
        for (auto* model : {freshModel.get(), oldModel.get()}) {
            model->Allocate(times, definitions);
            model->Init(times, definitions);
        }
        REQUIRE(freshModel->SimDim() == oldModel->SimDim(), "Flat pair dimension mismatch");
        AAD::Scenario_<> freshPath, oldPath;
        AAD::AllocatePath(definitions, freshPath);
        AAD::AllocatePath(definitions, oldPath);
        AAD::InitializePath(freshPath);
        AAD::InitializePath(oldPath);
        auto draws = New(RNGType_("MRG32"), 1024, freshModel->SimDim(), true);
        Vector_<> normals(freshModel->SimDim());
        for (int i = 0; i < 16; ++i) {
            draws->FillNormal(&normals);
            freshModel->GeneratePath(normals, &freshPath);
            oldModel->GeneratePath(normals, &oldPath);
            REQUIRE(std::fabs(freshPath.back().spot_ - oldPath.back().spot_) < 2e-10, "Flat pair path mismatch");
        }
        std::cout << "FLAT_PAIR dimension=" << freshModel->SimDim() << " seed=1024 normal_precision=Precise bridge=off evaluator=none workers=1\n";
        const auto measure = [&](const std::string& label, AAD::Model_<double>* model) {
            Measure(label, [&] {
                auto rng = New(RNGType_("MRG32"), 1024, model->SimDim(), true);
                double checksum = 0.;
                for (size_t i = 0; i < paths; ++i) {
                    rng->FillNormal(&normals);
                    model->GeneratePath(normals, &freshPath);
                    checksum += freshPath.back().spot_;
                }
                REQUIRE(std::isfinite(checksum), "Invalid path checksum");
                Bench::DoNotOptimize(&checksum);
            });
        };
        if (oldFirst)
            measure("flat legacy paths", oldModel.get());
        measure("flat market paths", freshModel.get());
        if (!oldFirst)
            measure("flat legacy paths", oldModel.get());
    }
    size_t ParsePaths(const std::string& count) {
        size_t paths = 0;
        const auto parsed = std::from_chars(count.data(), count.data() + count.size(), paths);
        REQUIRE(parsed.ec == std::errc() && parsed.ptr == count.data() + count.size() && paths > 0, "Invalid PATHS");
        return paths;
    }

    int Scale(const std::string& name) { return name == "medium" ? 1 : name == "large" ? 2 : 0; }

    void Profile(const std::string& name, const std::string& mode, size_t paths) {
        const int scale = Scale(name);
        const int terms[] = {8, 64, 256}, rows[] = {21, 81, 161}, columns[] = {13, 49, 97}, steps[] = {52, 208, 832};
        const auto start = Clock_::now();
        const auto fit = MarketIvExample::Fit(terms[scale], name == "flat-pair");
        REQUIRE(fit.IsSuccess(), fit.Failure());
        const auto grid = MarketIvExample::Grid(fit.Snapshot(), rows[scale], columns[scale]);
        REQUIRE(grid.IsSuccess(), grid.Failure());
        const double setupMs = std::chrono::duration<double, std::milli>(Clock_::now() - start).count();
        const auto settings = MarketIvExample::Settings(steps[scale]);
        Handle_<MarketIvLocalVolModelData_> data;
        const double firstConstruction = Milliseconds([&] { data = BuildMarketIvLocalVolModelData("market", grid, settings); });
        const auto product = name == "monthly-asian" ? Asian() : MarketIvExample::Product("call", "pay PAYS MAX(FIX(EQ[A]) - 100, 0)");
        Script::ScriptValuationSettings_ valuation;
        valuation.evaluationDate_ = MarketIvExample::Today();
        Script::MonteCarloSettings_ simulation;
        simulation.rsg_ = "mrg32";
        std::cout << std::setprecision(17) << "MARKET_IV case=" << name << " mode=" << mode << " paths=" << paths << " carry_terms=" << terms[scale]
                  << " grid=" << rows[scale] << 'x' << columns[scale] << " base_steps=" << steps[scale] << " fit_grid_ms=" << setupMs
                  << " first_construction_ms=" << firstConstruction << "\nsource=" << data->SourceIdentity() << "\nmodel=" << data->ModelIdentity()
                  << '\n';
        Bench::PrintHeader();
        MarketIvLocalVolExecution_ execution;
        const double firstPrepare =
            Milliseconds([&] { execution = Script::InspectMarketIvLocalVolExecution(product, *data, valuation, simulation); });
        PrintExecution(execution);
        std::cout << "FIRST_PREPARE diagnostic_ms=" << firstPrepare << '\n';
        if (name == "flat-pair" && mode == "steady") {
            FlatPair(grid, data, paths, std::getenv("DAL_MARKET_IV_OLD_FIRST") != nullptr);
        } else if (mode == "prepare") {
            Measure("market constructor", [&] {
                const auto value = BuildMarketIvLocalVolModelData("market", grid, settings);
                Bench::DoNotOptimize(value.get());
            });
            Measure("market repeated preparation", [&] {
                const auto value = Script::InspectMarketIvLocalVolExecution(product, *data, valuation, simulation);
                Bench::DoNotOptimize(&value);
            });
        } else {
            double pv = 0.;
            Measure(name + " " + mode + " valuation", [&] {
                const Handle_<ModelData_> model =
                    mode == "cold" ? Handle_<ModelData_>(BuildMarketIvLocalVolModelData("market", grid, settings)) : Handle_<ModelData_>(data);
                pv = Script::MCSimulation<double>(product, model, paths, valuation, simulation).aggregated_ / paths;
                REQUIRE(std::isfinite(pv) && pv > 0., "Invalid valuation checksum");
                Bench::DoNotOptimize(&pv);
            });
            std::cout << "PV=" << pv << '\n';
        }
    }
} // namespace

int RunMarketIvLocalVolProfile(int argc, char** argv) {
    using namespace Dal;
    if (argc != 5) {
        std::cerr << "usage: script_mc_perf --market-iv-local-vol small|medium|large|flat-pair|monthly-asian cold|steady|prepare PATHS\n";
        return 2;
    }
    try {
        const std::string name = argv[2], mode = argv[3];
        const std::set<std::string> cases{"small", "medium", "large", "flat-pair", "monthly-asian"}, modes{"cold", "steady", "prepare"};
        REQUIRE(cases.count(name) != 0, "Invalid CASE");
        REQUIRE(modes.count(mode) != 0, "Invalid MODE");
        Profile(name, mode, ParsePaths(argv[4]));
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 2;
    }
}

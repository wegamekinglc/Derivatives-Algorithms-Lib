#include <charconv>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <set>
#include <string>
#include <vector>

#include <dal/platform/platform.hpp>

#include <dal/benchmarks/bench.hpp>
#include <dal/concurrency/threadpool.hpp>
#include <dal/model/factory.hpp>
#include <dal/model/marketivlocalvolmodel.hpp>
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

    template <class F_> int CalibratedLoops(F_ action) {
        int loops = 1;
        while (Milliseconds([&] {
                   for (int i = 0; i < loops; ++i)
                       action();
               }) < 10. &&
               loops < 16384)
            loops *= 2;
        return loops;
    }

    template <class F_> void Measure(const std::string& label, F_ action) {
        const int loops = CalibratedLoops(action);
        std::cout << "CALIBRATION case=" << label << " inner_loops=" << loops << " repeats=10\n";
        Bench::Print(Bench::Run(label, action, 1, 10, loops));
    }

    template <class F_> void MeasureSamples(const std::string& label, size_t dimension, F_ action) {
        const int loops = CalibratedLoops(action);
        action();
        std::vector<int64_t> totals, perCall;
        for (int repeat = 0; repeat < 10; ++repeat) {
            const auto start = Clock_::now();
            for (int i = 0; i < loops; ++i)
                action();
            const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(Clock_::now() - start).count();
            REQUIRE(ns > 0, "Nonpositive benchmark sample");
            totals.push_back(ns);
            perCall.push_back(ns / loops);
        }
        auto sorted = perCall;
        std::sort(sorted.begin(), sorted.end());
        Bench::Print({label, sorted[5], sorted.front(), sorted.back(), 10});
        std::cout << "PROFILE_SAMPLES {\"case\":\"" << label << "\",\"inner_loops\":" << loops << ",\"dimension\":" << dimension;
        const auto write = [](const char* name, const auto& values) {
            std::cout << ",\"" << name << "\":[";
            for (size_t i = 0; i < values.size(); ++i)
                std::cout << (i ? "," : "") << values[i];
            std::cout << ']';
        };
        write("samples_ns", totals);
        write("per_iteration_ns", perCall);
        std::cout << "}\n";
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

    void VerifyPath(const AAD::Scenario_<>& actual, const AAD::Scenario_<>& expected) {
        REQUIRE(actual.size() == expected.size(), "Stage path size mismatch");
        for (size_t i = 0; i < actual.size(); ++i)
            REQUIRE(actual[i].spot_ == expected[i].spot_ && actual[i].numeraire_ == expected[i].numeraire_ &&
                        actual[i].observations_ == expected[i].observations_ && actual[i].discounts_ == expected[i].discounts_,
                    "Stage fixed-normal path mismatch");
    }

    class StorageAccount_ final : public Detail::OwnedBufferAccount_ {
    public:
        size_t bytes_ = 0, peak_ = 0;
        void Reserve(size_t bytes) override {
            bytes_ += bytes;
            peak_ = std::max(peak_, bytes_);
        }
        void Release(size_t bytes) noexcept override {
            if (bytes > bytes_)
                std::terminate();
            bytes_ -= bytes;
        }
    };

    void StageResources(const MarketIvLocalVolModelData_& data, const Vector_<>& timeline, const Vector_<AAD::SampleDef_>& definitions) {
        StorageAccount_ account;
        const Detail::OwnedBufferScope_ ownership(&account);
        size_t before = 0, after = 0, initAfter = 0, peak = 0, oldShared = 0;
        MarketIvLocalVolExecution_ execution;
        {
            AAD::MarketIvLocalVolModel_ model(data);
            model.Allocate(timeline, definitions);
            model.Init(timeline, definitions);
            before = account.bytes_;
            oldShared = model.Execution().sharedNumericBytes_;
            account.peak_ = before;
            model.Allocate(timeline, definitions);
            after = account.bytes_;
            peak = account.peak_;
            model.Init(timeline, definitions);
            initAfter = account.bytes_;
            execution = model.Execution();
        }
        REQUIRE(account.bytes_ == 0, "Stage resource cleanup retained tracked storage");
        REQUIRE(before == after && after == initAfter && peak >= after, "Stage resource capacity changed");
        std::cout << "STAGE_RESOURCE scope=tracked_buffer_storage before_bytes=" << before << " after_allocate_bytes=" << after
                  << " peak_coexisting_bytes=" << peak << " old_shared_numeric_bytes=" << oldShared
                  << " new_shared_numeric_bytes=" << execution.sharedNumericBytes_ << " init_after_bytes=" << initAfter
                  << " after_cleanup_bytes=" << account.bytes_ << " workspace_peak=" << execution.peakWorkspaceBytes_
                  << " heap_string_allocations=unmeasured\n";
    }

    void Stages(const Script::ScriptProductData_& product,
                const Handle_<MarketIvLocalVolModelData_>& data,
                const Script::ScriptValuationSettings_& valuation,
                const Script::MonteCarloSettings_& simulation,
                size_t paths) {
        AAD::MarketIvLocalVolModel_ reference(*data), model(*data);
        const auto prepared = Script::PrepareScript(product, &reference, valuation, simulation);
        const auto timeline = prepared.TimeLine();
        const auto definitions = prepared.DefLine();
        model.Allocate(timeline, definitions);
        model.Init(timeline, definitions);
        REQUIRE(model.SimDim() == reference.SimDim(), "Stage simulation dimension mismatch");
        const Vector_<> normals(model.SimDim(), .37);
        AAD::Scenario_<> expected, actual;
        AAD::AllocatePath(definitions, expected);
        AAD::AllocatePath(definitions, actual);
        AAD::InitializePath(expected);
        AAD::InitializePath(actual);
        reference.GeneratePath(normals, &expected);
        model.GeneratePath(normals, &actual);
        VerifyPath(actual, expected);
        const double direct = Script::MCDoubleSimulation(prepared, &model, paths, "mrg32", false, true, true).aggregated_ / paths;
        const double whole = Script::MCSimulation<double>(product, Handle_<ModelData_>(data), paths, valuation, simulation).aggregated_ / paths;
        REQUIRE(std::fabs(direct - whole) <= 2e-12, "Stage prepared/whole price mismatch");
        size_t numeraires = 0, indices = 0, discounts = 0, libors = 0, forwardRows = 0, forwards = 0;
        for (const auto& definition : definitions) {
            numeraires += definition.numeraire_;
            indices += definition.indexNames_.size();
            discounts += definition.discountMats_.size();
            libors += definition.liborDefs_.size();
            forwardRows += definition.forwardMats_.size();
            for (const auto& row : definition.forwardMats_)
                forwards += row.size();
        }
        std::cout << "STAGE_INPUT timeline=" << timeline.size() << " definitions=" << definitions.size() << " numeraires=" << numeraires
                  << " index_names=" << indices << " discount_maturities=" << discounts << " libor_definitions=" << libors
                  << " forward_rows=" << forwardRows << " forward_maturities=" << forwards << " dimension=" << model.SimDim()
                  << " fixed_normals=verified prepared_price=" << direct << " whole_price=" << whole << '\n';
        MeasureSamples("reallocate_only", model.SimDim(), [&] { model.Allocate(timeline, definitions); });
        model.Init(timeline, definitions);
        model.GeneratePath(normals, &actual);
        VerifyPath(actual, expected);
        const auto before = model.Execution();
        MeasureSamples("init_frozen_values", model.SimDim(), [&] { model.Init(timeline, definitions); });
        model.GeneratePath(normals, &actual);
        VerifyPath(actual, expected);
        const auto after = model.Execution();
        REQUIRE(before.sharedNumericBytes_ == after.sharedNumericBytes_ && before.peakWorkspaceBytes_ == after.peakWorkspaceBytes_ &&
                    before.preparedColumns_ == after.preparedColumns_ && before.preparedMarketQueries_ == after.preparedMarketQueries_,
                "Init changed preparation ledger");
        StageResources(*data, timeline, definitions);
    }

    Script::ScriptValuationSettings_ IdentityValuation(size_t count) {
        Script::ScriptValuationSettings_ valuation;
        valuation.evaluationDate_ = MarketIvExample::Today();
        if (count) {
            MarketFixingSnapshot_::values_t values;
            for (size_t i = 1; i <= count; ++i)
                values["EQ[A]"].emplace(DateTime_(MarketIvExample::Today().AddDays(-static_cast<int>(i)), .5), 99.);
            valuation.fixings_.reset(new MarketFixingSnapshot_(values));
        }
        return valuation;
    }

    void Inspection(size_t history) {
        const auto today = MarketIvExample::Today();
        const auto expiry = today.AddDays(365);
        const MarketIvMarket_ market(today, 100., {{expiry, 102., .98}});
        const MarketIvFitRequest_ request{market, {{"atm", expiry, 102., OptionType_("CALL"), MarketIvQuoteType_("DECIMAL_IV"), .2}}, {}};
        const auto fit = FitMarketIvSurface(request);
        REQUIRE(fit.IsSuccess(), fit.Failure());
        const auto grid = CalibrateMarketIvDupire(fit.Snapshot(), {{80., 100., 120.}, {.1, 1.}, {}});
        REQUIRE(grid.IsSuccess(), grid.Failure());
        const auto data = BuildMarketIvLocalVolModelData("market", grid, MarketIvExample::Settings());
        const auto product = MarketIvExample::Product("ScriptProductData_", "pay PAYS 1");
        Script::MonteCarloSettings_ simulation;
        simulation.rsg_ = "mrg32";
        const auto valuation = IdentityValuation(history);
        const auto inspect = [&](const auto& settings) { return Script::InspectMarketIvLocalVolExecution(product, *data, settings, simulation); };
        const auto execution = inspect(valuation);
        REQUIRE(execution.schema_ == "dal.market-iv-local-vol-execution/1", "Inspection report schema");
        REQUIRE(execution.requestIdentity_ == inspect(IdentityValuation(history)).requestIdentity_, "Independent fixing copy changed identity");
        for (const size_t other : {0U, 1U, 365U})
            if (other != history)
                REQUIRE(execution.requestIdentity_ != inspect(IdentityValuation(other)).requestIdentity_, "History missing from identity");
        const double pv = Script::MCSimulation<double>(product, Handle_<ModelData_>(data), 16, valuation, simulation).aggregated_ / 16.;
        REQUIRE(std::fabs(pv - .98) <= 2e-12, "Inspection unit payment preflight");
        std::cout << std::setprecision(17) << "MARKET_IV case=identity mode=inspection history_rows=" << history << " unit_payment=verified pv=" << pv
                  << "\nsource=" << data->SourceIdentity() << "\nmodel=" << data->ModelIdentity() << "\nrequest=" << execution.requestIdentity_
                  << '\n';
        PrintExecution(execution);
        MeasureSamples("inspection_fixings_" + std::to_string(history), execution.simulationDimension_, [&] {
            const auto value = inspect(valuation);
            Bench::DoNotOptimize(&value);
        });
        REQUIRE(execution.requestIdentity_ == inspect(valuation).requestIdentity_, "Inspection changed frozen state");
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
        } else if (mode == "stages") {
            Stages(product, data, valuation, simulation, paths);
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
    if (argc != 5 && argc != 6) {
        std::cerr << "usage: script_mc_perf --market-iv-local-vol CASE cold|steady|prepare|stages PATHS; identity inspection PATHS HISTORY_ROWS\n";
        return 2;
    }
    try {
        const std::string name = argv[2], mode = argv[3];
        if (name == "identity") {
            REQUIRE(argc == 6 && mode == "inspection", "identity requires inspection PATHS HISTORY_ROWS");
            static_cast<void>(ParsePaths(argv[4]));
            const std::string count = argv[5];
            REQUIRE(count == "0" || count == "1" || count == "365", "Invalid HISTORY_ROWS");
            Inspection(static_cast<size_t>(std::stoul(count)));
            return 0;
        }
        REQUIRE(argc == 5, "Unexpected arguments for CASE");
        const std::set<std::string> cases{"small", "medium", "large", "flat-pair", "monthly-asian"}, modes{"cold", "steady", "prepare", "stages"};
        REQUIRE(cases.count(name) != 0, "Invalid CASE");
        REQUIRE(modes.count(mode) != 0, "Invalid MODE");
        REQUIRE(mode != "stages" || name == "small" || name == "monthly-asian", "stages supports small or monthly-asian");
        Profile(name, mode, ParsePaths(argv[4]));
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 2;
    }
}

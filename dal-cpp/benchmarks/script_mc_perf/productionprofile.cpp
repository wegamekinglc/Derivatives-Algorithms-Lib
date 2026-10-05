//
// Created by Codex on 2026/10/04.
//

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <numeric>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <dal/platform/platform.hpp>

#include <dal/benchmarks/bench.hpp>
#include <dal/model/blackscholes.hpp>
#include <dal/model/hybriddata.hpp>
#include <dal/script/simulation.hpp>

#include "productionprofile.hpp"

using namespace Dal;
using namespace Dal::Script;

namespace {
    struct Options_ {
        std::string scenario_, mode_, engine_, measurement_;
        size_t paths_ = 0, outputs_ = 0, grid_ = 0, training_ = 0, repeats_ = 0;
        [[nodiscard]] bool Exercise() const { return scenario_ == "lsmc-bs" || scenario_ == "lsmc-local-vol"; }
        [[nodiscard]] bool LocalVol() const { return scenario_ == "local-vol" || scenario_ == "lsmc-local-vol"; }
        [[nodiscard]] bool Aad() const { return mode_ == "aad"; }
    };

    bool Count(const char* text, size_t* value) {
        const char* end = text + std::char_traits<char>::length(text);
        const auto result = std::from_chars(text, end, *value);
        return result.ec == std::errc{} && result.ptr == end;
    }

    bool Choice(const std::string& value, std::initializer_list<const char*> choices) {
        return std::any_of(choices.begin(), choices.end(), [&](const char* choice) { return value == choice; });
    }

    bool ValidNames(const Options_& options) {
        return Choice(options.scenario_, {"short", "long", "local-vol", "lsmc-bs", "lsmc-local-vol"}) && Choice(options.mode_, {"double", "aad"}) &&
               Choice(options.engine_, {"tree", "compiled"}) && Choice(options.measurement_, {"cold", "warm", "phases"});
    }

    bool ValidOutputs(size_t outputs) { return outputs == 1 || outputs == 4 || outputs == 16 || outputs == 64; }

    bool Parse(int argc, char** argv, Options_* options) {
        if (argc != 11)
            return false;
        options->scenario_ = argv[2];
        options->mode_ = argv[4];
        options->engine_ = argv[5];
        options->measurement_ = argv[6];
        for (const auto& field :
             {std::pair{3, &options->paths_}, {7, &options->outputs_}, {8, &options->grid_}, {9, &options->training_}, {10, &options->repeats_}})
            if (!Count(argv[field.first], field.second))
                return false;
        return options->paths_ > 0 && options->repeats_ > 0 && ValidNames(*options) && ValidOutputs(options->outputs_);
    }

    void ValidateOptions(const Options_& options) {
        REQUIRE(options.LocalVol() ? options.grid_ >= 2 && options.grid_ <= 64 : options.grid_ == 0, "invalid surface grid");
        REQUIRE(options.Exercise()
                    ? options.outputs_ == 1 && options.training_ > 0 && options.training_ <= static_cast<size_t>(std::numeric_limits<int>::max())
                    : options.training_ == 0,
                "invalid training count or LSM output count");
        REQUIRE(options.measurement_ != "phases" || AAD::ProfilingAvailable(), "phase measurement requires DAL_ENABLE_AAD_PROFILING=ON");
    }

    Handle_<ModelData_> ModelData(const Options_& options) {
        if (!options.LocalVol())
            return Handle_<ModelData_>(new BSModelData_("production-bs", 100.0, 0.2, 0.05, 0.02));
        Vector_<> spots(options.grid_), times(options.grid_);
        for (size_t i = 0; i < options.grid_; ++i) {
            spots[i] = 40.0 + 160.0 * static_cast<double>(i) / static_cast<double>(options.grid_ - 1);
            times[i] = 2.0 * static_cast<double>(i) / static_cast<double>(options.grid_ - 1);
        }
        return MakeFlatRateLocalVolHybridModelData("production-lv", "EQ[DAL418_TEST]", 100.0, 0.05, 0.02, spots, times,
                                                   Matrix_<>(static_cast<int>(options.grid_), static_cast<int>(options.grid_), 0.2));
    }

    String_ OutputName(size_t output) { return String_("call-strike-" + std::to_string(90.0 + static_cast<double>(output))); }

    ScriptProductData_ OrdinaryProduct(const Options_& options, size_t output) {
        const Date_ start(2024, 1, 1), maturity(2025, 1, 1);
        Vector_<Cell_> dates;
        Vector_<String_> events;
        if (options.scenario_ != "short") {
            for (Date_ date = start.AddDays(1); date <= maturity; date = date.AddDays(1)) {
                dates.push_back(Cell_(date));
                events.push_back(options.scenario_ == "long" ? "runningTotal = runningTotal + SPOT() / 366.0" : "observedSpot = SPOT()");
            }
        }
        dates.push_back(Cell_(maturity));
        const std::string underlying = options.scenario_ == "long" ? "runningTotal" : options.scenario_ == "local-vol" ? "observedSpot" : "SPOT()";
        events.push_back(String_("call PAYS MAX(" + underlying + " - " + std::to_string(90.0 + static_cast<double>(output)) + ", 0.0)"));
        return {OutputName(output), dates, events};
    }

    struct Case_ {
        Handle_<ModelData_> data_;
        std::vector<PreparedScript_> prepared_;
        size_t parameters_ = 0, events_ = 0, timeline_ = 0, randomDimensions_ = 0;
    };

    Case_ PrepareCase(const Options_& options, ExerciseProductFactory_ exerciseFactory) {
        AAD::ProfilingSpan_ prepare(AAD::AADProfilingPhase_::Value_::PREPARE);
        Case_ result;
        result.data_ = ModelData(options);
        MonteCarloSettings_ simulation;
        simulation.enableAad_ = options.Aad();
        simulation.compiled_ = options.engine_ == "compiled";
        if (options.Exercise())
            simulation.lsmcTrainingPaths_ = static_cast<int>(options.training_);
        for (size_t output = 0; output < options.outputs_; ++output) {
            const auto product = [&]() -> ScriptProductData_ {
                if (options.Exercise())
                    return exerciseFactory(options.LocalVol() ? "1CD" : "1W", false);
                return OrdinaryProduct(options, output);
            }();
            auto model = CreateModel<double>(result.data_);
            result.prepared_.push_back(PrepareScript(product, model.get(), {}, simulation));
            const auto& prepared = result.prepared_.back();
            result.parameters_ = model->Parameters().size() + prepared.ConstVarNames().size();
            result.events_ = prepared.EventDates().size();
            result.timeline_ = prepared.TimeLine().size();
            result.randomDimensions_ = model->SimDim();
        }
        return result;
    }

    std::vector<SimResults_> Evaluate(const Case_& scriptCase, const Options_& options) {
        std::vector<SimResults_> results;
        for (const auto& prepared : scriptCase.prepared_) {
            if (options.Aad())
                results.push_back(
                    MCSimulation<AAD::Number_>(prepared, scriptCase.data_, options.paths_, "sobol", false, options.engine_ == "compiled"));
            else
                results.push_back(MCSimulation<double>(prepared, scriptCase.data_, options.paths_, "sobol", false, options.engine_ == "compiled"));
        }
        return results;
    }

    void Near(double actual, double expected, double tolerance, const char* message) {
        REQUIRE(std::isfinite(actual) && std::isfinite(expected) &&
                    std::abs(actual - expected) <= tolerance * std::max({1.0, std::abs(actual), std::abs(expected)}),
                message);
    }

    void Match(const std::vector<SimResults_>& actual, const std::vector<SimResults_>& expected, size_t paths) {
        REQUIRE(actual.size() == expected.size(), "output count differs");
        for (size_t output = 0; output < actual.size(); ++output) {
            Near(actual[output].aggregated_ / static_cast<double>(paths), expected[output].aggregated_ / static_cast<double>(paths), 1e-10,
                 "fixed-path price differs");
            REQUIRE(actual[output].names_ == expected[output].names_ && actual[output].risks_.size() == expected[output].risks_.size(),
                    "risk axes differ");
            for (size_t input = 0; input < actual[output].risks_.size(); ++input)
                Near(actual[output].risks_[input], expected[output].risks_[input], 1e-10, "fixed-path risk differs");
        }
    }

    double
    BumpedPrice(const PreparedScript_& passive, const Handle_<ModelData_>& data, const Options_& options, const Vector_<>& direction, double step) {
        auto model = CreateModel<double>(data);
        const auto& inputs = model->Parameters();
        REQUIRE(inputs.size() == direction.size(), "parameter direction differs");
        for (size_t input = 0; input < inputs.size(); ++input)
            *inputs[input] += step * direction[input];
        return MCDoubleSimulation(passive, model.get(), options.paths_, "sobol", false, options.engine_ == "compiled").aggregated_ /
               static_cast<double>(options.paths_);
    }

    Vector_<> ParameterDirection(const Vector_<double*>& inputs, bool columns, size_t axis) {
        Vector_<> direction(inputs.size(), 0.0);
        for (size_t input = 0; input < inputs.size(); ++input) {
            const double weight = columns ? (input == axis ? 1.0 : 0.0)
                                          : (axis == 0 ? 1.0 : (input % 2 == 0 ? 1.0 : -1.0)) / std::sqrt(static_cast<double>(inputs.size()));
            direction[input] = weight * std::max(1.0, std::abs(*inputs[input]));
        }
        return direction;
    }

    void DirectionOracle(const Case_& passive, const Options_& options, const std::vector<SimResults_>& aad) {
        const auto model = CreateModel<double>(passive.data_);
        const auto& inputs = model->Parameters();
        const bool columns = inputs.size() <= 8;
        const size_t directions = columns ? inputs.size() : 2;
        for (size_t axis = 0; axis < directions; ++axis) {
            const auto direction = ParameterDirection(inputs, columns, axis);
            for (size_t output = 0; output < aad.size(); ++output) {
                REQUIRE(aad[output].risks_.size() >= direction.size(), "AAD direction axes differ");
                const double expected = std::inner_product(direction.begin(), direction.end(), aad[output].risks_.begin(), 0.0);
                for (const double step : {1e-5, 5e-6}) {
                    const double actual = (BumpedPrice(passive.prepared_[output], passive.data_, options, direction, step) -
                                           BumpedPrice(passive.prepared_[output], passive.data_, options, direction, -step)) /
                                          (2.0 * step);
                    std::ostringstream context;
                    context << std::setprecision(17) << "common-path directional derivative oracle: output=" << output << " axis=" << axis
                            << " step=" << step << " actual=" << actual << " expected=" << expected;
                    Near(actual, expected, 1e-6, context.str().c_str());
                }
            }
        }
    }

    void ShortBsOracle(const Case_& passive, const Options_& options, const std::vector<SimResults_>& aad) {
        const auto& prepared = passive.prepared_.front();
        auto model = CreateModel<double>(passive.data_);
        model->Allocate(prepared.TimeLine(), prepared.DefLine());
        model->Init(prepared.TimeLine(), prepared.DefLine());
        auto random = CreateRNG("sobol", *model, false);
        random->SkipNormalTo(0);
        REQUIRE(model->SimDim() == 1, "short BS oracle requires one time step");
        Vector_<> normal(1);
        std::vector<Vector_<>> sums(options.outputs_, Vector_<>(5, 0.0));
        const double time = prepared.TimeLine().back();
        const double discount = std::exp(-0.05 * time), rootTime = std::sqrt(time);
        for (size_t path = 0; path < options.paths_; ++path) {
            random->FillNormal(&normal);
            const double terminal = std::exp(std::log(100.0) + (0.05 - 0.02 - 0.5 * 0.2 * 0.2) * time + 0.2 * rootTime * normal[0]);
            for (size_t output = 0; output < options.outputs_; ++output) {
                const double strike = 90.0 + static_cast<double>(output);
                const double intrinsic = std::max(terminal - strike, 0.0), active = terminal > strike ? terminal : 0.0;
                auto& row = sums[output];
                row[0] += discount * intrinsic;
                row[1] += discount * active / 100.0;
                row[2] += discount * active * (-0.2 * time + rootTime * normal[0]);
                row[3] += discount * time * (active - intrinsic);
                row[4] -= discount * time * active;
            }
        }
        for (size_t output = 0; output < aad.size(); ++output) {
            REQUIRE(aad[output].names_ == Vector_<String_>({"spot", "vol", "rate", "div"}), "short BS oracle axes differ");
            Near(aad[output].aggregated_ / static_cast<double>(options.paths_), sums[output][0] / static_cast<double>(options.paths_), 1e-10,
                 "short BS analytic price differs");
            for (size_t input = 0; input < 4; ++input)
                Near(aad[output].risks_[input], sums[output][input + 1] / static_cast<double>(options.paths_), 1e-10,
                     "short BS analytic risk differs");
        }
    }

    void ValidateNumeric(const Options_& options, ExerciseProductFactory_ exerciseFactory) {
        auto reference = options;
        reference.paths_ = std::min<size_t>(options.paths_, 256);
        auto alternate = reference;
        alternate.engine_ = options.engine_ == "tree" ? "compiled" : "tree";
        const auto current = Evaluate(PrepareCase(reference, exerciseFactory), reference);
        Match(current, Evaluate(PrepareCase(alternate, exerciseFactory), alternate), reference.paths_);
        if (options.Exercise())
            return;
        auto active = reference;
        active.mode_ = "aad";
        const auto aad = Evaluate(PrepareCase(active, exerciseFactory), active);
        auto passive = reference;
        passive.mode_ = "double";
        const auto passiveCase = PrepareCase(passive, exerciseFactory);
        const auto prices = Evaluate(passiveCase, passive);
        for (size_t output = 0; output < aad.size(); ++output)
            Near(aad[output].aggregated_ / static_cast<double>(reference.paths_), prices[output].aggregated_ / static_cast<double>(reference.paths_),
                 1e-10, "passive/AAD price differs");
        if (options.scenario_ == "short")
            ShortBsOracle(passiveCase, passive, aad);
        else
            DirectionOracle(passiveCase, passive, aad);
    }

    void NumberOrNull(std::uint64_t value, bool available) {
        if (available)
            std::cout << value;
        else
            std::cout << "null";
    }

    void PrintScope(const AAD::ProfilingData_& data, size_t sample, const std::string& identity) {
        std::ostringstream thread;
        thread << data.thread_;
        std::cout << "{\"record\":\"scope\",\"sample\":" << sample << ",\"scope\":" << std::quoted(identity)
                  << ",\"thread\":" << std::quoted(thread.str()) << ",\"complete\":" << (data.complete_ ? "true" : "false")
                  << ",\"wall_ns\":" << data.wallNanoseconds_ << ",\"inclusive_cpu_ns\":";
        NumberOrNull(data.cpuNanoseconds_, data.cpuAvailable_);
        std::cout << ",\"self_cpu_ns\":";
        NumberOrNull(data.selfCpuNanoseconds_, data.selfCpuAvailable_);
        std::cout << ",\"tape_samples\":" << data.tapeSamples_ << ",\"memory_samples\":" << data.memorySamples_
                  << ",\"nodes_peak\":" << data.highWater_.nodes_ << ",\"edges_peak\":" << data.highWater_.edges_
                  << ",\"tape_live_bytes_peak\":" << data.highWater_.liveBytes_ << ",\"tape_occupied_bytes_peak\":" << data.highWater_.occupiedBytes_
                  << ",\"tape_capacity_bytes_peak\":" << data.highWater_.capacityBytes_ << ",\"tape_blocks_peak\":" << data.highWater_.blocks_
                  << ",\"block_allocation_events\":" << data.blockAllocations_ << ",\"allocated_array_bytes\":" << data.allocatedArrayBytes_;
        for (const auto& category : {std::pair{"path", &data.memory_.pathArrays_},
                                     {"workspace", &data.memory_.workspaceArrays_},
                                     {"result", &data.memory_.resultArrays_},
                                     {"regression", &data.memory_.regressionArrays_}})
            std::cout << ",\"" << category.first << "_array_live_bytes\":" << category.second->liveBytes_ << ",\"" << category.first
                      << "_array_capacity_bytes\":" << category.second->capacityBytes_;
        std::cout << "}\n";
        for (const auto phase : AAD::AADProfilingPhaseListAll()) {
            const auto& statistics = data.Phase(phase);
            if (statistics.calls_ == 0)
                continue;
            std::cout << "{\"record\":\"phase\",\"sample\":" << sample << ",\"scope\":" << std::quoted(identity)
                      << ",\"phase\":" << std::quoted(std::string(phase.String())) << ",\"calls\":" << statistics.calls_
                      << ",\"inclusive_wall_ns\":" << statistics.wallNanoseconds_ << ",\"self_wall_ns\":" << statistics.selfWallNanoseconds_
                      << ",\"inclusive_cpu_ns\":";
            NumberOrNull(statistics.cpuNanoseconds_, statistics.cpuSamples_ == statistics.calls_);
            std::cout << "}\n";
        }
        size_t groupIndex = 0;
        for (const auto& group : data.taskGroups_) {
            for (size_t task = 0; task < group.size(); ++task)
                PrintScope(group[task], sample, identity + "." + std::to_string(groupIndex) + "." + std::to_string(task));
            ++groupIndex;
        }
    }

    void PrintRequest(const Options_& options, const Case_& scriptCase, size_t sample, std::int64_t elapsed) {
        const auto threads = ThreadPool_::GetInstance()->NumThreads();
        const BatchPlan_ batches(options.paths_, options.Exercise() ? 1 : threads);
        std::cout
            << "{\"record\":\"request\",\"sample\":" << sample << ",\"scenario\":" << std::quoted(options.scenario_)
            << ",\"mode\":" << std::quoted(options.mode_) << ",\"engine\":" << std::quoted(options.engine_)
            << ",\"measurement\":" << std::quoted(options.measurement_) << ",\"outputs\":" << options.outputs_
            << ",\"output_strategy\":\"sequential_single_output\",\"channel_width\":" << (options.Aad() ? 1 : 0)
            << ",\"paths_per_output\":" << options.paths_ << ",\"parameters\":" << scriptCase.parameters_
            << ",\"active_parameters\":" << (options.Aad() ? scriptCase.parameters_ : 0) << ",\"events\":" << scriptCase.events_
            << ",\"timeline_samples\":" << scriptCase.timeline_ << ",\"random_dimensions\":" << scriptCase.randomDimensions_
            << ",\"threads\":" << threads << ",\"hardware_threads\":" << std::thread::hardware_concurrency()
            << ",\"pricing_batches_per_output\":" << batches.BatchCount() << ",\"pricing_batch_size\":" << batches.BatchSize()
            << ",\"training_paths\":" << options.training_ << ",\"surface_grid\":" << options.grid_
            << ",\"training_first_path\":0,\"pricing_first_path\":" << options.training_
            << ",\"risk_basis\":\"fixed_model_and_script_parameters\",\"rng\":\"sobol\",\"scramble\":\"none\",\"seed\":null,\"brownian_bridge\":false"
            << ",\"lsm_policy\":\"Frozen\",\"smooth\":0.01,\"oracle_paths\":" << std::min<size_t>(options.paths_, 256)
            << ",\"risk_units\":\"d_mean_price_per_raw_parameter_unit\""
            << ",\"preparation_included\":" << (options.measurement_ == "warm" ? "false" : "true")
            << ",\"profiling_build\":" << (AAD::ProfilingAvailable() ? "true" : "false")
            << ",\"process_first_request\":false,\"worker_state_reused\":false"
            << ",\"wall_ns\":" << elapsed << "}\n";
    }

    void PrintNumbers(const Vector_<>& values) {
        for (size_t input = 0; input < values.size(); ++input)
            std::cout << (input ? "," : "") << values[input];
    }

    void PrintNames(const Vector_<String_>& names) {
        for (size_t input = 0; input < names.size(); ++input)
            std::cout << (input ? "," : "") << std::quoted(std::string(names[input].c_str()));
    }

    void PrintResults(const std::vector<SimResults_>& results, const Options_& options, size_t sample) {
        for (size_t output = 0; output < results.size(); ++output) {
            const auto& result = results[output];
            std::cout << "{\"record\":\"result\",\"sample\":" << sample
                      << ",\"output\":" << std::quoted(options.Exercise() ? options.scenario_ : std::string(OutputName(output).c_str()))
                      << ",\"pv\":" << result.aggregated_ / static_cast<double>(options.paths_) << ",\"risk_names\":[";
            if (options.Aad())
                PrintNames(result.names_);
            std::cout << "],\"risks\":[";
            if (options.Aad())
                PrintNumbers(result.risks_);
            std::cout << "]}\n";
        }
    }

    int Execute(const Options_& options, ExerciseProductFactory_ exerciseFactory) {
        ValidateNumeric(options, exerciseFactory);
        const auto referenceCase = PrepareCase(options, exerciseFactory);
        const auto expected = Evaluate(referenceCase, options);
        std::cout << std::setprecision(17);
        for (size_t sample = 0; sample < options.repeats_; ++sample) {
            AAD::ProfilingData_ data;
            std::vector<SimResults_> result;
            const auto begin = std::chrono::steady_clock::now();
            {
                std::optional<AAD::ProfilingScope_> profile;
                if (options.measurement_ == "phases")
                    profile.emplace(&data);
                if (options.measurement_ == "warm")
                    result = Evaluate(referenceCase, options);
                else
                    result = Evaluate(PrepareCase(options, exerciseFactory), options);
            }
            const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - begin).count();
            Match(result, expected, options.paths_);
            REQUIRE(options.measurement_ != "phases" || data.complete_, "incomplete phase measurement");
            PrintRequest(options, referenceCase, sample, elapsed);
            PrintResults(result, options, sample);
            if (options.measurement_ == "phases")
                PrintScope(data, sample, "0");
            Bench::DoNotOptimize(&result);
        }
        return 0;
    }
} // namespace

int RunProductionProfile(int argc, char** argv, ExerciseProductFactory_ exerciseFactory) {
    Options_ options;
    if (!Parse(argc, argv, &options)) {
        std::cerr << "usage: script_mc_perf --production-profile short|long|local-vol|lsmc-bs|lsmc-local-vol PATHS double|aad tree|compiled "
                     "cold|warm|phases OUTPUTS GRID TRAINING REPEATS\n";
        return 2;
    }
    try {
        ValidateOptions(options);
        return Execute(options, exerciseFactory);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

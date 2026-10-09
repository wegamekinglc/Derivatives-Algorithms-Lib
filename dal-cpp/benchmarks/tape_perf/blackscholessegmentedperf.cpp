//
// Created by Codex on 2026/10/9.
//

#include <array>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <string>

#include <dal/benchmarks/aad.hpp>
#include <dal/benchmarks/bench.hpp>
#include <dal/math/aad/segmentedpath.hpp>
#include <dal/model/blackscholes.hpp>
#include <dal/platform/platform.hpp>
#include <dal/script/preparation.hpp>

#ifndef DAL_SEGMENTED_BS_BASELINE
#include <dal/script/segmentedpath.hpp>
#endif

#include "blackscholessegmentedperf.hpp"

using namespace Dal;
using namespace Dal::AAD;

namespace {
    struct FinancialCase_ {
        const char* name_;
        size_t steps_;
        bool liveFixing_;
    };

    constexpr std::array<FinancialCase_, 3> CASES{{{"short", 16, false}, {"running", 2048, false}, {"live", 2048, true}}};

    std::shared_ptr<const Script::PreparedScript_> Prepare(const FinancialCase_& request) {
        Vector_<Cell_> dates;
        Vector_<String_> events;
        for (size_t i = 0; i < request.steps_; ++i) {
            dates.emplace_back(Date_(2026, 10, 2).AddDays(static_cast<int>(i)));
            String_ event = "running = running + FIX(EQ[DAL196_TEST])";
            if (i == 0 && request.liveFixing_)
                event += " pay PAYS running ON 2032-10-01";
            if (i + 1 == request.steps_)
                event += request.liveFixing_ ? " pay PAYS running + FIX(EQ[DAL196_TEST], 2026-10-02) + pay ON 2032-10-01" : " pay PAYS running";
            events.push_back(std::move(event));
        }
        Script::ScriptValuationSettings_ valuation;
        valuation.evaluationDate_ = Date_(2026, 10, 1);
        Script::MonteCarloSettings_ simulation;
        simulation.enableAad_ = true;
        simulation.compiled_ = true;
        BlackScholes_<> model(1.0, 0.0);
        return std::make_shared<const Script::PreparedScript_>(
            Script::PrepareScript(Script::ScriptProductData_("", dates, events), &model, valuation, simulation));
    }

    SegmentedPathResult_ FullPath(const Script::PreparedScript_& prepared, const Vector_<>& parameters, const Vector_<>& gaussian) {
        const auto mode = SetNumResultsForAAD(false, 1);
        TapeCapacityBudget_ budget(std::numeric_limits<size_t>::max());
        TapeCapacityScope_ capacity(&budget, true);
        const Vector_<> fixedParameters = parameters;
        const Vector_<> fixedGaussian = gaussian;
        RecordingScope_ recording;
        Vector_<Number_> inputs(parameters.size());
        for (size_t i = 0; i < inputs.size(); ++i)
            recording.RegisterInput(inputs[i], fixedParameters[i]);
        recording.StartRecording();
        BlackScholes_<Number_> model(inputs[0], inputs[1], inputs[2], inputs[3]);
        auto state = prepared.BuildEvalState<Number_>();
        model.Allocate(prepared.TimeLine(), prepared.DefLine());
        model.Init(prepared.TimeLine(), prepared.DefLine());
        prepared.InitializeHistoricalState(&state);
        Scenario_<Number_> scenario;
        AllocatePath(prepared.DefLine(), scenario);
        InitializePath(scenario);
        model.GeneratePath(fixedGaussian, &scenario);
        prepared.CompiledProgram(true).Evaluate(scenario, state);
        Number_ root = state.VarVals()[prepared.PayOffIdx()];
        const double value = Value(root);
        recording.FinishRecording();
        NativeOperations_::AddSeed(root, 1.0);
        recording.Reverse();
        Vector_<> gradient(parameters.size());
        for (size_t i = 0; i < inputs.size(); ++i)
            gradient[i] = NativeOperations_::ReadAdjoint(inputs[i]);
        recording.Close();
        SegmentedPathExecution_ execution;
        execution.peakTapeBytes_ = budget.PeakCapacityBytes();
        execution.cleanupReserveBytes_ = TapeCleanupCapacityBytes();
        capacity.Close();
        return {value, std::move(gradient), execution};
    }

    void Verify(const SegmentedPathResult_& result, const SegmentedPathResult_& expected) {
        Bench::VerifyAadResult(result.Value(), expected.Value());
        REQUIRE(result.Gradient().size() == expected.Gradient().size(), "financial path gradient shape differs");
        for (size_t i = 0; i < result.Gradient().size(); ++i)
            Bench::VerifyAadResult(result.Gradient()[i], expected.Gradient()[i]);
    }

    void Run(const FinancialCase_& request, bool segmented, bool cold) {
        const auto prepared = Prepare(request);
        const Vector_<> parameters{100.0, 0.2, 0.03, 0.01};
        const Vector_<> gaussian(request.steps_, 0.01);
        auto expected = FullPath(*prepared, parameters, gaussian);
        Clear(*Tape());
#ifndef DAL_SEGMENTED_BS_BASELINE
        const Script::BlackScholesSegmentedPath_ kernel(prepared);
        SegmentedPathSettings_ settings;
        settings.segmentSteps_ = 64;
        auto evaluate = [&] { return segmented ? kernel.Evaluate(parameters, gaussian, settings) : FullPath(*prepared, parameters, gaussian); };
#else
        REQUIRE(!segmented, "segmented execution is unavailable on the baseline");
        auto evaluate = [&] { return FullPath(*prepared, parameters, gaussian); };
#endif
        auto result = evaluate();
        Verify(result, expected);
        Clear(*Tape());
        const std::string mode = std::string(segmented ? "segmented" : "full") + (cold ? "-cold" : "-warm");
        const auto timing = Bench::Run(
            std::string("financial ") + request.name_ + ' ' + mode,
            [&] {
                if (cold)
                    Clear(*Tape());
                result = evaluate();
                Bench::DoNotOptimize(&result);
            },
            1, 3);
        Verify(result, expected);
        Bench::Print(timing);
        const auto& execution = result.Execution();
        std::cout << "financial_path_cost " << request.name_ << ' ' << mode << ' ' << timing.minNs << ' ' << execution.peakTapeBytes_ << ' '
                  << execution.checkpointBytes_ << ' ' << execution.cleanupReserveBytes_ << ' ' << std::setprecision(17) << result.Value();
        for (double gradient : result.Gradient())
            std::cout << ' ' << gradient;
        std::cout << '\n';
        Clear(*Tape());
    }

    int CommandLine(int argc, char** argv) {
        if (argc != 5 || std::string(argv[1]) != "--case" || std::string(argv[3]) != "--mode")
            return 2;
        struct Mode_ {
            const char* name_;
            bool segmented_;
            bool cold_;
        };
        constexpr std::array<Mode_, 4> modes{
            {{"full-cold", false, true}, {"full-warm", false, false}, {"segmented-cold", true, true}, {"segmented-warm", true, false}}};
        for (const auto& request : CASES) {
            if (request.name_ == std::string(argv[2])) {
                for (const auto& mode : modes) {
                    if (mode.name_ == std::string(argv[4])) {
                        Run(request, mode.segmented_, mode.cold_);
                        return 0;
                    }
                }
            }
        }
        return 2;
    }
} // namespace

int RunBlackScholesSegmentedBenchmarks(int argc, char** argv) {
    try {
        RegisterAll_::Init();
        Bench::PrintHeader();
        return CommandLine(argc, argv);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

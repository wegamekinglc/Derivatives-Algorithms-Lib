//
// Created by Codex on 2026/10/09.
//

#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>

#include <dal/benchmarks/bench.hpp>
#include <dal/math/aad/segmentedpath.hpp>

#include "segmentedpathperf.hpp"

using namespace Dal;
using namespace Dal::AAD;

namespace {
    struct Case_ {
        const char* name_;
        size_t steps_;
        size_t segmentSteps_;
        bool contributions_;
    };

    const std::array<Case_, 4> CASES = {
        {{"short", 32, 8, false}, {"long64", 16384, 64, false}, {"long256", 16384, 256, false}, {"cashflows", 16384, 128, true}}};

    class Path_ final : public SegmentedPathKernel_ {
        Case_ request_;

        template <class T_> static Vector_<T_> Initial(const Vector_<T_>& parameters) { return {parameters[1]}; }

        template <class T_>
        SegmentedPathTransition_<T_> Step(size_t step, const Vector_<T_>& state, const Vector_<T_>& parameters, const Vector_<>& drivers) const {
            const T_ contribution = request_.contributions_ ? T_(parameters[1] * state[0] / request_.steps_) : T_(0.0);
            return {{T_(state[0] + parameters[0] * drivers[step])}, contribution, {}};
        }

        template <class T_> static T_ Payoff(const Vector_<T_>& state, const Vector_<T_>& parameters) {
            return T_(state[0] * state[0] + parameters[0] * parameters[1]);
        }

    public:
        explicit Path_(const Case_& request) : request_(request) {}
        SegmentedPathDimensions_ Dimensions() const override { return {request_.steps_, 1, 0}; }
        Vector_<> InitialState(const Vector_<>& parameters) const override { return Initial(parameters); }
        Vector_<Number_> InitialState(const Vector_<Number_>& parameters) const override { return Initial(parameters); }
        SegmentedPathTransition_<double>
        Advance(size_t step, const Vector_<>& state, const Vector_<>& parameters, const Vector_<>& drivers) const override {
            return Step(step, state, parameters, drivers);
        }
        SegmentedPathTransition_<Number_>
        Advance(size_t step, const Vector_<Number_>& state, const Vector_<Number_>& parameters, const Vector_<>& drivers) const override {
            return Step(step, state, parameters, drivers);
        }
        double Terminal(const Vector_<>& state, const Vector_<>& parameters) const override { return Payoff(state, parameters); }
        Number_ Terminal(const Vector_<Number_>& state, const Vector_<Number_>& parameters) const override { return Payoff(state, parameters); }
    };

    SegmentedPathResult_ FullPath(const Path_& kernel, const Vector_<>& parameters, const Vector_<>& drivers) {
        const auto mode = SetNumResultsForAAD(false, 1);
        TapeCapacityBudget_ budget(std::numeric_limits<size_t>::max());
        TapeCapacityScope_ capacity(&budget, true);
        const Vector_<> fixedParameters = parameters;
        const Vector_<> fixedDrivers = drivers;
        RecordingScope_ recording;
        Vector_<Number_> inputs(parameters.size());
        for (size_t i = 0; i < inputs.size(); ++i)
            recording.RegisterInput(inputs[i], fixedParameters[i]);
        recording.StartRecording();
        auto state = kernel.InitialState(inputs);
        Number_ objective(0.0);
        for (size_t step = 0; step < kernel.Dimensions().steps_; ++step) {
            auto next = kernel.Advance(step, state, inputs, fixedDrivers);
            objective += next.contribution_;
            state = std::move(next.state_);
        }
        objective += kernel.Terminal(state, inputs);
        const auto value = Value(objective);
        recording.FinishRecording();
        NativeOperations_::SetSeed(objective, 1.0);
        recording.Reverse();
        Vector_<> gradient(inputs.size());
        for (size_t i = 0; i < inputs.size(); ++i)
            gradient[i] = NativeOperations_::ReadAdjoint(inputs[i]);
        recording.Close();
        SegmentedPathExecution_ execution;
        execution.peakTapeBytes_ = budget.PeakCapacityBytes();
        execution.cleanupReserveBytes_ = TapeCleanupCapacityBytes();
        capacity.Close();
        return {value, std::move(gradient), execution};
    }

    void Verify(const SegmentedPathResult_& result, const Case_& request) {
        const double fraction = (request.steps_ - 1.0) / (2.0 * request.steps_);
        const double extra = request.contributions_ ? 1.0 : 0.0;
        REQUIRE(std::abs(result.Value() - (11.0 + extra * (1.0 + 2.0 * fraction))) < 1e-10,
                "Segmented path benchmark value differs from independent oracle");
        REQUIRE(result.Gradient().size() == 2, "Segmented path benchmark gradient shape differs");
        REQUIRE(std::abs(result.Gradient()[0] - (7.0 + extra * fraction)) < 1e-10,
                "Segmented path benchmark first derivative differs from independent oracle");
        REQUIRE(std::abs(result.Gradient()[1] - (8.0 + extra * (2.0 + 2.0 * fraction))) < 1e-10,
                "Segmented path benchmark second derivative differs from independent oracle");
    }

    void Run(const Case_& request, bool segmented) {
        const Path_ path(request);
        const Vector_<> parameters = {2.0, 1.0};
        const Vector_<> drivers(request.steps_, 1.0 / request.steps_);
        SegmentedPathSettings_ settings;
        settings.segmentSteps_ = request.segmentSteps_;
        auto result = SegmentedPathResult_(0.0, {}, {});
        const std::string mode = segmented ? "segmented" : "full";
        const auto timing = Bench::Run(
            request.name_ + std::string(" ") + mode,
            [&] {
                // Cold requests prevent a previous full graph from inflating the segmented tape.
                Clear(*Tape());
                result = segmented ? ExecuteSegmentedPath(path, parameters, drivers, settings) : FullPath(path, parameters, drivers);
                Bench::DoNotOptimize(&result);
            },
            1, 3);
        Verify(result, request);
        Bench::Print(timing);
        const auto& execution = result.Execution();
        std::cout << "path_cost " << request.name_ << ' ' << mode << ' ' << timing.minNs << ' ' << execution.peakTapeBytes_ << ' '
                  << execution.checkpointBytes_ << ' ' << execution.cleanupReserveBytes_ << ' ' << std::setprecision(17) << result.Value() << ' '
                  << result.Gradient()[0] << ' ' << result.Gradient()[1] << '\n';
        Clear(*Tape());
    }

    int RunSelected(const std::string& name, bool segmented) {
        for (const auto& request : CASES) {
            if (request.name_ == name) {
                Run(request, segmented);
                return 0;
            }
        }
        return 2;
    }

    int CommandLine(int argc, char** argv) {
        if (argc != 5 || std::string(argv[1]) != "--case" || std::string(argv[3]) != "--mode")
            return 2;
        const std::string mode(argv[4]);
        if (mode != "full" && mode != "segmented")
            return 2;
        return RunSelected(argv[2], mode == "segmented");
    }
} // namespace

int RunSegmentedPathBenchmarks(int argc, char** argv) {
    try {
        Bench::PrintHeader();
        if (argc == 1) {
            for (const auto& request : CASES) {
                Run(request, false);
                Run(request, true);
            }
            return 0;
        }
        return CommandLine(argc, argv);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

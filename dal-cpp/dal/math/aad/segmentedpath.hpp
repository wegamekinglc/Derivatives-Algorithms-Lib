//
// Created by Codex on 2026/10/09.
//

#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>

#include <dal/math/aad/native.hpp>
#include <dal/math/aad/recording.hpp>
#include <dal/math/aad/tapecapacity.hpp>
#include <dal/platform/platform.hpp>

namespace Dal::AAD {
    struct SegmentedPathDimensions_ {
        size_t steps_ = 0;
        size_t stateSize_ = 0;
        size_t traceSlots_ = 0;
    };

    template <class T_> struct SegmentedPathTransition_ {
        Vector_<T_> state_;
        T_ contribution_ = T_(0.0);
        Vector_<std::uint64_t> branchTrace_;
    };

    class SegmentedPathKernel_ {
    public:
        virtual ~SegmentedPathKernel_() = default;
        [[nodiscard]] virtual SegmentedPathDimensions_ Dimensions() const = 0;
        [[nodiscard]] virtual Vector_<> InitialState(const Vector_<>& parameters) const = 0;
        [[nodiscard]] virtual Vector_<Number_> InitialState(const Vector_<Number_>& parameters) const = 0;
        [[nodiscard]] virtual SegmentedPathTransition_<double>
        Advance(size_t step, const Vector_<>& state, const Vector_<>& parameters, const Vector_<>& drivers) const = 0;
        [[nodiscard]] virtual SegmentedPathTransition_<Number_>
        Advance(size_t step, const Vector_<Number_>& state, const Vector_<Number_>& parameters, const Vector_<>& drivers) const = 0;
        [[nodiscard]] virtual double Terminal(const Vector_<>& state, const Vector_<>& parameters) const = 0;
        [[nodiscard]] virtual Number_ Terminal(const Vector_<Number_>& state, const Vector_<Number_>& parameters) const = 0;
    };

    struct SegmentedPathSettings_ {
        size_t segmentSteps_ = 64;
        std::optional<size_t> checkpointCapacityBudgetBytes_;
        std::optional<size_t> recordingCapacityBudgetBytes_;
    };

    struct SegmentedPathExecution_ {
        size_t passiveSteps_ = 0;
        size_t recomputedSteps_ = 0;
        size_t segments_ = 0;
        size_t reverseSweeps_ = 0;
        size_t checkpointBytes_ = 0;
        size_t peakTapeBytes_ = 0;
        size_t cleanupReserveBytes_ = 0;
    };

    class SegmentedPathResult_ {
        double value_;
        Vector_<> gradient_;
        SegmentedPathExecution_ execution_;

    public:
        SegmentedPathResult_(double value, Vector_<>&& gradient, const SegmentedPathExecution_& execution)
            : value_(value), gradient_(std::move(gradient)), execution_(execution) {}
        [[nodiscard]] double Value() const { return value_; }
        [[nodiscard]] const Vector_<>& Gradient() const { return gradient_; }
        [[nodiscard]] const SegmentedPathExecution_& Execution() const { return execution_; }
    };

    namespace SegmentedPathDetail {
        inline size_t Sum(size_t lhs, size_t rhs) {
            REQUIRE(rhs <= std::numeric_limits<size_t>::max() - lhs, "SegmentedPath: capacity sum overflow");
            return lhs + rhs;
        }

        inline size_t Product(size_t lhs, size_t rhs) {
            REQUIRE(rhs == 0 || lhs <= std::numeric_limits<size_t>::max() / rhs, "SegmentedPath: capacity product overflow");
            return lhs * rhs;
        }

        inline void RequireCheckpointBudget(size_t bytes, const SegmentedPathSettings_& settings) {
            REQUIRE(!settings.checkpointCapacityBudgetBytes_ || bytes <= *settings.checkpointCapacityBudgetBytes_,
                    "SegmentedPath: checkpoint capacity budget exceeded");
        }

        template <class T_> void RequireFinite(const Vector_<T_>& values) {
            for (const auto& value : values)
                REQUIRE(std::isfinite(Value(value)), "SegmentedPath: nonfinite state or input");
        }

        template <class T_> void RequireState(const Vector_<T_>& state, size_t size) {
            REQUIRE(state.size() == size, "SegmentedPath: state shape mismatch");
            RequireFinite(state);
        }

        template <class T_>
        void RequireTransition(const SegmentedPathTransition_<T_>& transition, const SegmentedPathDimensions_& dimensions, size_t step) {
            try {
                RequireState(transition.state_, dimensions.stateSize_);
                REQUIRE(transition.branchTrace_.size() == dimensions.traceSlots_, "SegmentedPath: branch trace shape mismatch");
                REQUIRE(std::isfinite(Value(transition.contribution_)), "SegmentedPath: nonfinite direct contribution");
            } catch (const Exception_& error) {
                THROW("SegmentedPath: invalid transition; step=" + String_(std::to_string(step)) + "; " + error.what());
            }
        }

        inline void RequireMatch(double actual, double expected, const char* kind) {
            REQUIRE(std::isfinite(actual) && std::abs(actual - expected) <= 1e-12 * std::max(1.0, std::abs(expected)),
                    String_("SegmentedPath: replay mismatch; kind=") + kind);
        }

        template <class T_> void RequireMatches(const Vector_<T_>& actual, const Vector_<>& expected) {
            RequireState(actual, expected.size());
            for (size_t i = 0; i < actual.size(); ++i)
                RequireMatch(Value(actual[i]), expected[i], "state");
        }

        struct Workspace_ {
            Vector_<Vector_<>> states_;
            Vector_<> contributions_;
            Vector_<std::uint64_t> traces_;
            double terminal_ = 0.0;
            double value_ = 0.0;

            Workspace_(size_t segments, const SegmentedPathDimensions_& dimensions)
                : states_(Sum(segments, 1)), contributions_(segments, 0.0), traces_(Product(dimensions.steps_, dimensions.traceSlots_)) {
                for (auto& state : states_)
                    state.Resize(dimensions.stateSize_);
            }

            [[nodiscard]] size_t CapacityBytes() const {
                size_t result = Product(states_.capacity(), sizeof(Vector_<>));
                for (const auto& state : states_)
                    result = Sum(result, Product(state.capacity(), sizeof(double)));
                result = Sum(result, Product(contributions_.capacity(), sizeof(double)));
                return Sum(result, Product(traces_.capacity(), sizeof(std::uint64_t)));
            }
        };

        inline size_t Plan(size_t segments, const SegmentedPathDimensions_& dimensions, const SegmentedPathSettings_& settings) {
            const auto stateBytes = Sum(sizeof(Vector_<>), Product(dimensions.stateSize_, sizeof(double)));
            auto bytes = Product(Sum(segments, 1), stateBytes);
            bytes = Sum(bytes, Product(segments, sizeof(double)));
            bytes = Sum(bytes, Product(Product(dimensions.steps_, dimensions.traceSlots_), sizeof(std::uint64_t)));
            RequireCheckpointBudget(bytes, settings);
            return bytes;
        }

        inline Workspace_ Forward(const SegmentedPathKernel_& kernel,
                                  const SegmentedPathDimensions_& dimensions,
                                  const Vector_<>& parameters,
                                  const Vector_<>& drivers,
                                  const SegmentedPathSettings_& settings,
                                  SegmentedPathExecution_* execution) {
            Workspace_ work(execution->segments_, dimensions);
            execution->checkpointBytes_ = work.CapacityBytes();
            RequireCheckpointBudget(execution->checkpointBytes_, settings);
            auto state = kernel.InitialState(parameters);
            RequireState(state, dimensions.stateSize_);
            work.states_[0] = state;
            size_t step = 0;
            for (size_t segment = 0; segment < execution->segments_; ++segment) {
                const auto end = step + std::min(settings.segmentSteps_, dimensions.steps_ - step);
                for (; step < end; ++step) {
                    auto next = kernel.Advance(step, state, parameters, drivers);
                    RequireTransition(next, dimensions, step);
                    if (dimensions.traceSlots_ != 0)
                        std::copy(next.branchTrace_.begin(), next.branchTrace_.end(), work.traces_.begin() + Product(step, dimensions.traceSlots_));
                    work.contributions_[segment] += next.contribution_;
                    state = std::move(next.state_);
                    ++execution->passiveSteps_;
                }
                work.states_[segment + 1] = state;
                work.value_ += work.contributions_[segment];
            }
            work.terminal_ = kernel.Terminal(state, parameters);
            RequireMatch(work.terminal_, work.terminal_, "terminal");
            work.value_ += work.terminal_;
            REQUIRE(std::isfinite(work.value_), "SegmentedPath: nonfinite total objective");
            return work;
        }

        inline Vector_<Number_> Register(const Vector_<>& values, RecordingScope_* recording) {
            Vector_<Number_> result(values.size());
            for (size_t i = 0; i < values.size(); ++i)
                recording->RegisterInput(result[i], values[i]);
            return result;
        }

        struct Targets_ {
            Vector_<Number_> state_;
            Number_ contribution_;
        };

        template <class F_>
        Vector_<> Reverse(const Vector_<>& state,
                          const Vector_<>& parameters,
                          const Vector_<>& seeds,
                          const F_& evaluate,
                          Vector_<>* gradient,
                          SegmentedPathExecution_* execution) {
            RecordingScope_ recording;
            auto activeState = Register(state, &recording);
            auto activeParameters = Register(parameters, &recording);
            recording.StartRecording();
            auto targets = evaluate(activeState, activeParameters);
            REQUIRE(targets.state_.size() == seeds.size(), "SegmentedPath: state seed shape mismatch");
            recording.FinishRecording();
            // Multiple state coordinates may share a node.
            for (size_t i = 0; i < seeds.size(); ++i)
                NativeOperations_::AddSeed(targets.state_[i], seeds[i]);
            NativeOperations_::AddSeed(targets.contribution_, 1.0);
            recording.Reverse();
            Vector_<> stateSeeds(activeState.size());
            for (size_t i = 0; i < activeState.size(); ++i)
                stateSeeds[i] = NativeOperations_::ReadAdjoint(activeState[i]);
            for (size_t i = 0; i < activeParameters.size(); ++i)
                (*gradient)[i] += NativeOperations_::ReadAdjoint(activeParameters[i]);
            RequireFinite(stateSeeds);
            RequireFinite(*gradient);
            ++execution->reverseSweeps_;
            recording.Close();
            return stateSeeds;
        }

        inline Targets_ Recompute(const SegmentedPathKernel_& kernel,
                                  const SegmentedPathDimensions_& dimensions,
                                  const Workspace_& work,
                                  const SegmentedPathSettings_& settings,
                                  size_t segment,
                                  Vector_<Number_> state,
                                  const Vector_<Number_>& parameters,
                                  const Vector_<>& drivers,
                                  SegmentedPathExecution_* execution) {
            const auto first = Product(segment, settings.segmentSteps_);
            const auto end = first + std::min(settings.segmentSteps_, dimensions.steps_ - first);
            Number_ contribution(0.0);
            for (size_t step = first; step < end; ++step) {
                auto next = kernel.Advance(step, state, parameters, drivers);
                RequireTransition(next, dimensions, step);
                const auto offset = Product(step, dimensions.traceSlots_);
                REQUIRE(dimensions.traceSlots_ == 0 || std::equal(next.branchTrace_.begin(), next.branchTrace_.end(), work.traces_.begin() + offset),
                        "SegmentedPath: replay mismatch; kind=branch; step=" + String_(std::to_string(step)));
                contribution += next.contribution_;
                state = std::move(next.state_);
                ++execution->recomputedSteps_;
            }
            try {
                RequireMatches(state, work.states_[segment + 1]);
                RequireMatch(Value(contribution), work.contributions_[segment], "contribution");
            } catch (const Exception_& error) {
                THROW("SegmentedPath: replay mismatch; segment=" + String_(std::to_string(segment)) + "; " + error.what());
            }
            return {std::move(state), contribution};
        }
    } // namespace SegmentedPathDetail

    [[nodiscard]] inline SegmentedPathResult_ ExecuteSegmentedPath(const SegmentedPathKernel_& kernel,
                                                                   const Vector_<>& parameters,
                                                                   const Vector_<>& drivers,
                                                                   const SegmentedPathSettings_& settings = {}) {
        using namespace SegmentedPathDetail;
        RequireRecordingModeChangeAllowed();
        REQUIRE(settings.segmentSteps_ > 0, "SegmentedPath: segment length must be positive");
        RequireFinite(parameters);
        RequireFinite(drivers);
        const auto dimensions = kernel.Dimensions();
        SegmentedPathExecution_ execution;
        execution.segments_ = Sum(dimensions.steps_ / settings.segmentSteps_, static_cast<size_t>(dimensions.steps_ % settings.segmentSteps_ != 0));
        (void)Plan(execution.segments_, dimensions, settings);
        const auto mode = SetNumResultsForAAD(false, 1);
        TapeCapacityBudget_ budget(settings.recordingCapacityBudgetBytes_.value_or(std::numeric_limits<size_t>::max()));
        TapeCapacityScope_ capacity(&budget, true);
        const Vector_<> fixedParameters = parameters;
        const Vector_<> fixedDrivers = drivers;
        const auto work = Forward(kernel, dimensions, fixedParameters, fixedDrivers, settings, &execution);
        Vector_<> gradient(parameters.size(), 0.0);
        auto seeds = Reverse(
            work.states_.back(), fixedParameters, {},
            [&](const auto& state, const auto& inputs) -> Targets_ {
                auto terminal = kernel.Terminal(state, inputs);
                RequireMatch(Value(terminal), work.terminal_, "terminal");
                return {{}, terminal};
            },
            &gradient, &execution);
        for (size_t segment = execution.segments_; segment > 0; --segment) {
            const auto index = segment - 1;
            seeds = Reverse(
                work.states_[index], fixedParameters, seeds,
                [&](const auto& state, const auto& inputs) {
                    return Recompute(kernel, dimensions, work, settings, index, state, inputs, fixedDrivers, &execution);
                },
                &gradient, &execution);
        }
        (void)Reverse(
            {}, fixedParameters, seeds,
            [&](const auto&, const auto& inputs) -> Targets_ {
                auto initial = kernel.InitialState(inputs);
                RequireMatches(initial, work.states_.front());
                return {std::move(initial), Number_(0.0)};
            },
            &gradient, &execution);
        execution.peakTapeBytes_ = budget.PeakCapacityBytes();
        execution.cleanupReserveBytes_ = TapeCleanupCapacityBytes();
        capacity.Close();
        return {work.value_, std::move(gradient), execution};
    }
} // namespace Dal::AAD

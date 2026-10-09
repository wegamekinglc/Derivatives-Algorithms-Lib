//
// Created by Codex on 2026/10/9.
//

#pragma once

#include <cmath>
#include <memory>
#include <optional>
#include <utility>

#include <dal/math/aad/segmentedpath.hpp>
#include <dal/model/blackscholessteps.hpp>
#include <dal/platform/platform.hpp>
#include <dal/script/detail/segmentedtrace.hpp>
#include <dal/script/preparation.hpp>

namespace Dal::Script {
    class BlackScholesSegmentedPreparation_ {
        PreparedScript_ prepared_;

        explicit BlackScholesSegmentedPreparation_(PreparedScript_ prepared) : prepared_(std::move(prepared)) {}
        friend BlackScholesSegmentedPreparation_ PrepareBlackScholesSegmentedScript(
            const ScriptProductData_&, const ScriptValuationSettings_&, const Handle_<MarketFixingSnapshot_>&, const ScriptProductSettings_&, double);

    public:
        BlackScholesSegmentedPreparation_(BlackScholesSegmentedPreparation_&&) = default;
        BlackScholesSegmentedPreparation_& operator=(BlackScholesSegmentedPreparation_&&) = default;
        [[nodiscard]] const PreparedScript_& Prepared() const { return prepared_; }
    };

    [[nodiscard]] inline BlackScholesSegmentedPreparation_ PrepareBlackScholesSegmentedScript(const ScriptProductData_& product,
                                                                                              const ScriptValuationSettings_& settings,
                                                                                              const Handle_<MarketFixingSnapshot_>& snapshot = {},
                                                                                              const ScriptProductSettings_& contract = {},
                                                                                              double smoothing = DEFAULT_SMOOTH) {
        MonteCarloSettings_ simulation;
        simulation.enableAad_ = true;
        simulation.compiled_ = true;
        simulation.smooth_ = smoothing;
        AAD::BlackScholes_<> model(1.0, 0.0);
        return BlackScholesSegmentedPreparation_(Detail::PrepareScriptForSegmentation(product, &model, settings, simulation, snapshot, contract));
    }

    class BlackScholesSegmentedPath_ final : public AAD::SegmentedPathKernel_ {
        std::shared_ptr<const PreparedScript_> prepared_;
        std::optional<AAD::BlackScholesStepPlan_> model_;
        Detail::SegmentedObservationPlan_ observations_;
        Detail::SegmentedEvalLayout_ layout_;
        Detail::SegmentedTracePlan_ trace_;
        Vector_<String_> labels_;

        [[nodiscard]] static std::shared_ptr<const PreparedScript_> Validate(std::shared_ptr<const BlackScholesSegmentedPreparation_> preparation) {
            REQUIRE2(preparation, "BlackScholesSegmentedPath: prepared script must be present", ScriptError_);
            const std::shared_ptr<const PreparedScript_> prepared(preparation, &preparation->Prepared());
            prepared->RequireExecutable();
            REQUIRE2(prepared->Simulation().enableAad_ && prepared->Simulation().compiled_,
                     "BlackScholesSegmentedPath: native compiled AAD preparation is required", ScriptError_);
            REQUIRE2(!prepared->Product().ContainsExercise(), "BlackScholesSegmentedPath: EXERCISE is unsupported", ScriptError_);
            REQUIRE2(prepared->PayOffIdx() < prepared->Product().VarNames().size(), "BlackScholesSegmentedPath: payoff receiver is missing",
                     ScriptError_);
            (void)prepared->CompiledProgram(true);
            return prepared;
        }

        [[nodiscard]] static std::optional<AAD::BlackScholesStepPlan_> Model(const PreparedScript_& prepared) {
            if (prepared.TimeLine().empty())
                return {};
            return AAD::BlackScholesStepPlan_(prepared.TimeLine(), prepared.DefLine());
        }

        template <class T_> void ValidateParameters(const Vector_<T_>& parameters) const {
            REQUIRE2(parameters.size() == labels_.size(), "BlackScholesSegmentedPath: parameter size must equal ParameterLabels", ScriptError_);
            AAD::Detail::ValidateBlackScholesParameters(parameters[0], parameters[1], parameters[2], parameters[3]);
            for (size_t i = 4; i < parameters.size(); ++i)
                REQUIRE2(std::isfinite(AAD::Value(parameters[i])), "BlackScholesSegmentedPath: script parameter must be finite", ScriptError_);
        }

        template <class T_> [[nodiscard]] EvalState_<T_> Evaluator(const Vector_<T_>& parameters) const {
            const Vector_<T_> constants(parameters.begin() + 4, parameters.end());
            return EvalState_<T_>(Vector_<>(prepared_->Product().VarNames().size(), 0.0), constants, prepared_->MaxNestedIfs(),
                                  prepared_->Simulation().smooth_, layout_.VectorBounds());
        }

        template <class T_> [[nodiscard]] Vector_<T_> Initial(const Vector_<T_>& parameters) const {
            ValidateParameters(parameters);
            auto state = Evaluator(parameters);
            prepared_->InitializeHistoricalState(&state);
            state.Init();
            const T_ logSpot = model_ ? T_(Dal::log(parameters[0])) : T_(0.0);
            return layout_.Pack(logSpot, state, Vector_<T_>(observations_.Slots(), T_(0.0)));
        }

        template <class T_>
        [[nodiscard]] AAD::SegmentedPathTransition_<T_>
        Step(size_t sampleId, const Vector_<T_>& packed, const Vector_<T_>& parameters, const Vector_<>& gaussian) const {
            ValidateParameters(parameters);
            REQUIRE2(model_, "BlackScholesSegmentedPath: historical-only product has no future steps", ScriptError_);
            auto state = Evaluator(parameters);
            Vector_<T_> observations(observations_.Slots(), T_(0.0));
            T_ logSpot;
            layout_.Restore(packed, &logSpot, &state, &observations);
            const Vector_<T_> modelParameters(parameters.begin(), parameters.begin() + 4);
            auto next = model_->Advance(sampleId, std::move(logSpot), modelParameters, gaussian);
            observations_.Capture(sampleId, next.sample_, &observations);
            Vector_<std::uint64_t> trace(trace_.MaxWords(), 0);
            const auto& program = prepared_->CompiledProgram(true);
            for (size_t event : trace_.Events(sampleId)) {
                const Detail::SegmentedCompiledPolicy_<T_> policy(&observations_, observations, trace_.View(event, &trace));
                Detail::EvalCompiledEvents<true>(
                    1,
                    [&](size_t) {
                        return Detail::CompiledEventView_<T_, Detail::SegmentedCompiledPolicy_<T_>>{
                            program.NodeStreams()[event], program.ConstStreams()[event], next.sample_, 0, 0, true, policy};
                    },
                    &state);
            }
            return {layout_.Pack(next.logSpot_, state, observations), T_(0.0), std::move(trace)};
        }

        template <class T_> [[nodiscard]] T_ Payoff(const Vector_<T_>& state, const Vector_<T_>& parameters) const {
            ValidateParameters(parameters);
            REQUIRE2(state.size() == layout_.Size(), "BlackScholesSegmentedPath: terminal state size mismatch", ScriptError_);
            return state[1 + prepared_->PayOffIdx()];
        }

    public:
        explicit BlackScholesSegmentedPath_(std::shared_ptr<const BlackScholesSegmentedPreparation_> prepared)
            : prepared_(Validate(std::move(prepared))), model_(Model(*prepared_)), observations_(prepared_->PlanHandle()),
              layout_(prepared_->Product(), observations_.Slots()),
              trace_(prepared_->CompiledProgram(true), layout_.VectorBounds(), prepared_->Plan().EventToSample(), prepared_->TimeLine().size()),
              labels_(AAD::BlackScholes_<>(1.0, 0.0).ParameterLabels()) {
            labels_.Append(prepared_->ConstVarNames());
        }

        [[nodiscard]] size_t SimDim() const { return model_ ? model_->SimDim() : 0; }
        [[nodiscard]] const Vector_<String_>& ParameterLabels() const { return labels_; }
        [[nodiscard]] AAD::SegmentedPathDimensions_ Dimensions() const override {
            return {prepared_->TimeLine().size(), layout_.Size(), trace_.MaxWords()};
        }

        void ValidateRequest(const Vector_<>& parameters, const AAD::SegmentedPathSettings_& settings = {}) const {
            ValidateParameters(parameters);
            REQUIRE2(settings.segmentSteps_ > 0, "BlackScholesSegmentedPath: segment length must be positive", ScriptError_);
            const auto dimensions = Dimensions();
            const size_t segments = dimensions.steps_ / settings.segmentSteps_ + static_cast<size_t>(dimensions.steps_ % settings.segmentSteps_ != 0);
            (void)AAD::SegmentedPathDetail::Plan(segments, dimensions, settings);
        }

        [[nodiscard]] AAD::SegmentedPathResult_
        Evaluate(const Vector_<>& parameters, const Vector_<>& gaussian, const AAD::SegmentedPathSettings_& settings = {}) const {
            ValidateParameters(parameters);
            REQUIRE2(gaussian.size() == SimDim(), "BlackScholesSegmentedPath: Gaussian size must equal SimDim", ScriptError_);
            return AAD::ExecuteSegmentedPath(*this, parameters, gaussian, settings);
        }

        [[nodiscard]] Vector_<> InitialState(const Vector_<>& parameters) const override { return Initial(parameters); }
        [[nodiscard]] Vector_<AAD::Number_> InitialState(const Vector_<AAD::Number_>& parameters) const override { return Initial(parameters); }
        [[nodiscard]] AAD::SegmentedPathTransition_<double>
        Advance(size_t step, const Vector_<>& state, const Vector_<>& parameters, const Vector_<>& gaussian) const override {
            return Step(step, state, parameters, gaussian);
        }
        [[nodiscard]] AAD::SegmentedPathTransition_<AAD::Number_>
        Advance(size_t step, const Vector_<AAD::Number_>& state, const Vector_<AAD::Number_>& parameters, const Vector_<>& gaussian) const override {
            return Step(step, state, parameters, gaussian);
        }
        [[nodiscard]] double Terminal(const Vector_<>& state, const Vector_<>& parameters) const override { return Payoff(state, parameters); }
        [[nodiscard]] AAD::Number_ Terminal(const Vector_<AAD::Number_>& state, const Vector_<AAD::Number_>& parameters) const override {
            return Payoff(state, parameters);
        }
    };
} // namespace Dal::Script

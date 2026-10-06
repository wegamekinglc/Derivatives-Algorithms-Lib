//
// Created by Codex on 2026/10/6.
//

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <set>
#include <type_traits>

#include <dal/platform/platform.hpp>
#include <dal/script/blockedreplay.hpp>
#include <dal/script/portfoliobatch.hpp>
#include <dal/script/portfoliopassive.hpp>
#include <dal/script/simulation.hpp>

namespace Dal::Script::Detail {
    namespace {
        struct BatchSelection_ {
            Vector_<size_t> tradePositions_;
            Vector_<size_t> outputEvaluators_;
            Vector_<double> weights_;
            Vector_<String_> outputContexts_;
        };

        class WeightedObjective_ {
        public:
            using Result_ = PortfolioWeightedBatchResult_;
            static constexpr bool BLOCKED = false;
            explicit WeightedObjective_(size_t) {}
            [[nodiscard]] size_t Width() const { return 1; }
            void RegisterZero(AAD::RecordingScope_*) const {}
            [[nodiscard]] AAD::Number_ Root(const Vector_<AAD::Number_>& values, const Vector_<double>& weights) const {
                return AAD::WeightedPayoffRoot(values, weights);
            }
            void Seed(AAD::Number_* root) const { Adjoint(*root) = 1.0; }
        };

        class BlockObjective_ {
            size_t width_;
            AAD::Number_ zero_;
            Vector_<AAD::Number_> roots_;

        public:
            using Result_ = PortfolioJacobianBatchResult_;
            static constexpr bool BLOCKED = true;
            explicit BlockObjective_(size_t width) : width_(width) { roots_.reserve(width); }
            [[nodiscard]] size_t Width() const { return width_; }
            void RegisterZero(AAD::RecordingScope_* recording) { recording->RegisterInput(zero_, 0.0); }
            [[nodiscard]] AAD::Number_ Root(const Vector_<AAD::Number_>& values, const Vector_<double>&) {
                AAD::SeedAdjointBlock(values, {0, values.size(), width_}, zero_, &roots_);
                return roots_[0];
            }
            void Seed(AAD::Number_*) const {}
        };

        String_ BatchFailureContext(const PreparedPortfolio_& portfolio, size_t group, const Vector_<PortfolioBatchOutput_>& outputs) {
            String_ context = "; trades=[";
            for (const auto trade : portfolio.Groups()[group].tradePositions_)
                context += portfolio.Portfolio()->TradeIds()[trade] + ";";
            context += "]; outputs=[";
            for (const auto& output : outputs)
                context += output.coordinate_.id_ + ";";
            return context + "]";
        }

        void ValidateBatchRange(const PreparedPortfolio_& portfolio, size_t group, const PathBatch_& batch) {
            REQUIRE2(group < portfolio.Groups().size(), "InvalidPortfolioBatch: group is out of range; field=group", ScriptError_);
            const size_t paths = static_cast<size_t>(portfolio.PathCount());
            REQUIRE2(batch.pathCount_ > 0 && batch.firstPath_ <= paths && batch.pathCount_ <= paths - batch.firstPath_,
                     "InvalidPortfolioBatch: absolute path range is outside the request; field=batch", ScriptError_);
        }

        void ValidateOutput(const PreparedPortfolio_& portfolio, const PortfolioScenarioGroup_& group, const PortfolioBatchOutput_& output) {
            REQUIRE2(std::find(group.tradePositions_.begin(), group.tradePositions_.end(), output.tradePosition_) != group.tradePositions_.end(),
                     "InvalidPortfolioBatch: output trade is outside the group; output=" + output.coordinate_.id_, ScriptError_);
            const auto& trade = portfolio.Trades()[output.tradePosition_];
            trade.RequireExecutable();
            if (trade.Simulation().enableAad_) {
                ValidateAADHistory(trade);
                ValidateAADExecution(trade, trade.Simulation().compiled_.value_or(false), trade.Simulation().smooth_);
            }
            const auto axis = ScriptRiskOutputAxis(trade.Product());
            REQUIRE2(output.coordinate_.slot_ < axis.size(), "InvalidPortfolioBatch: scalar slot is out of range; output=" + output.coordinate_.id_,
                     ScriptError_);
            const auto& expected = axis[output.coordinate_.slot_];
            const auto id = "trade:" + String_(std::to_string(output.tradePosition_)) + ":" + expected.id_;
            REQUIRE2(output.coordinate_.id_ == id && output.coordinate_.label_ == expected.label_,
                     "InvalidPortfolioBatch: output coordinate changed; output=" + output.coordinate_.id_, ScriptError_);
            REQUIRE2(std::isfinite(output.weight_), "InvalidPortfolioBatch: weight must be finite; output=" + output.coordinate_.id_, ScriptError_);
        }

        BatchSelection_
        SelectBatchOutputs(const PreparedPortfolio_& portfolio, const PortfolioScenarioGroup_& group, const Vector_<PortfolioBatchOutput_>& outputs) {
            REQUIRE2(!outputs.empty(), "InvalidPortfolioBatch: outputs must not be empty; field=outputs", ScriptError_);
            std::set<String_> ids;
            std::set<size_t> selectedTrades;
            BatchSelection_ result;
            for (const auto& output : outputs) {
                ValidateOutput(portfolio, group, output);
                REQUIRE2(ids.insert(output.coordinate_.id_).second, "InvalidPortfolioBatch: repeated output; output=" + output.coordinate_.id_,
                         ScriptError_);
                selectedTrades.insert(output.tradePosition_);
                result.weights_.push_back(output.weight_);
                result.outputContexts_.push_back(output.coordinate_.id_ + "; trade=" + portfolio.Portfolio()->TradeIds()[output.tradePosition_]);
            }
            for (const auto trade : group.tradePositions_)
                if (selectedTrades.count(trade))
                    result.tradePositions_.push_back(trade);
            for (const auto& output : outputs) {
                const auto position = std::find(result.tradePositions_.begin(), result.tradePositions_.end(), output.tradePosition_);
                result.outputEvaluators_.push_back(static_cast<size_t>(position - result.tradePositions_.begin()));
            }
            return result;
        }

        template <class F_> auto BuildStates(const PreparedPortfolio_& portfolio, const BatchSelection_& selection, const F_& build) {
            using E_ = decltype(build(portfolio.Trades()[selection.tradePositions_.front()]));
            Vector_<std::unique_ptr<E_>> states;
            states.reserve(selection.tradePositions_.size());
            for (const auto trade : selection.tradePositions_)
                states.push_back(std::unique_ptr<E_>(new E_(build(portfolio.Trades()[trade]))));
            return states;
        }

        template <class E_, class O_>
        AAD::Checkpoint_ InitializeRecording(const PreparedPortfolio_& portfolio,
                                             const BatchSelection_& selection,
                                             AAD::Model_<AAD::Number_>* model,
                                             AAD::Scenario_<AAD::Number_>* path,
                                             Vector_<std::unique_ptr<E_>>* states,
                                             AAD::RecordingScope_* recording,
                                             O_* objective) {
            AAD::Rewind(*AAD::Tape());
            for (auto* parameter : model->Parameters())
                recording->RegisterInput(*parameter, Value(*parameter));
            for (auto& state : *states)
                for (auto& constant : state->ConstVarVals())
                    recording->RegisterInput(constant, Value(constant));
            objective->RegisterZero(recording);
            recording->StartRecording();
            const auto& representative = portfolio.Trades()[selection.tradePositions_.front()];
            model->Init(representative.TimeLine(), representative.DefLine());
            AAD::InitializePath(*path);
            for (size_t state = 0; state < states->size(); ++state)
                portfolio.Trades()[selection.tradePositions_[state]].InitializeHistoricalState((*states)[state].get());
            return recording->MakeCheckpoint();
        }

        void AddValue(double value, double* sum, const String_& output) {
            REQUIRE2(std::isfinite(value) && std::isfinite(*sum + value), "InvalidPortfolioPayoff: non-finite value or sum; output=" + output,
                     ScriptError_);
            *sum += value;
        }

        template <class E_, class R_>
        void CollectOutputs(const Vector_<PortfolioBatchOutput_>& outputs,
                            const BatchSelection_& selection,
                            const Vector_<std::unique_ptr<E_>>& states,
                            Vector_<AAD::Number_>* values,
                            R_* result) {
            for (size_t component = 0; component < outputs.size(); ++component) {
                (*values)[component] = states[selection.outputEvaluators_[component]]->VarVals()[outputs[component].coordinate_.slot_];
                AddValue(Value((*values)[component]), &result->componentSums_[component], selection.outputContexts_[component]);
            }
        }

        template <class G_, class F_> G_ ExtractInputs(size_t width, size_t count, const F_& input) {
            G_ result;
            if constexpr (std::is_same_v<G_, Vector_<double>>) {
                result.reserve(count);
                for (size_t column = 0; column < count; ++column)
                    result.push_back(Adjoint(input(column)));
            } else {
                REQUIRE2(count <= static_cast<size_t>(std::numeric_limits<int>::max()), "InvalidPortfolioBatch: input extent exceeds matrix columns",
                         ScriptError_);
                result = Matrix_<double>(static_cast<int>(width), static_cast<int>(count));
                for (size_t column = 0; column < count; ++column)
                    for (size_t lane = 0; lane < width; ++lane)
                        result(static_cast<int>(lane), static_cast<int>(column)) = AAD::NativeOperations_::ReadAdjoint(input(column), lane);
            }
            return result;
        }

        template <class E_, class G_>
        void ExtractGradients(const AAD::Model_<AAD::Number_>& model,
                              const Vector_<std::unique_ptr<E_>>& states,
                              size_t width,
                              PortfolioBatchResult_<G_>* result) {
            const auto& parameters = model.Parameters();
            result->modelGradientSums_ =
                ExtractInputs<G_>(width, parameters.size(), [&](size_t column) -> const auto& { return *parameters[column]; });
            for (const auto& state : states) {
                result->constantGradientSums_.push_back(ExtractInputs<G_>(
                    width, state->ConstVarVals().size(), [&](size_t column) -> const auto& { return state->ConstVarVals()[column]; }));
            }
        }

        template <class T_, class E_, class F_>
        void EvaluateTrade(const PreparedPortfolio_& portfolio, size_t trade, const AAD::Scenario_<T_>& path, E_* state, const F_& evaluate) {
            try {
                evaluate(portfolio.Trades()[trade], path, *state);
            } catch (const std::exception& error) {
                THROW2("PortfolioTradeEvaluationFailed: trade=" + portfolio.Portfolio()->TradeIds()[trade] + "; cause=" + String_(error.what()),
                       ScriptError_);
            }
        }

        template <class E_> class PassiveWeightedCollector_ {
            const Vector_<PortfolioBatchOutput_>& outputs_;
            const BatchSelection_& selection_;
            const Vector_<std::unique_ptr<E_>>& states_;
            PortfolioWeightedBatchResult_* result_;

        public:
            PassiveWeightedCollector_(const Vector_<PortfolioBatchOutput_>& outputs,
                                      const BatchSelection_& selection,
                                      const Vector_<std::unique_ptr<E_>>& states,
                                      PortfolioWeightedBatchResult_* result)
                : outputs_(outputs), selection_(selection), states_(states), result_(result) {}

            double operator()(const Vector_<double>&, size_t, double) const {
                double weighted = 0.0;
                for (size_t component = 0; component < outputs_.size(); ++component) {
                    const auto value = states_[selection_.outputEvaluators_[component]]->VarVals()[outputs_[component].coordinate_.slot_];
                    const auto& context = selection_.outputContexts_[component];
                    AddValue(value, &result_->componentSums_[component], context);
                    AddValue(value * selection_.weights_[component], &weighted, context);
                }
                return weighted;
            }

            [[nodiscard]] AAD::ProfilingArrayStatistics_ WorkspaceArrays() const {
                auto result = ProfileArrays(result_->componentSums_, selection_.weights_, selection_.outputEvaluators_);
                for (size_t state = 1; state < states_.size(); ++state)
                    result = JoinProfileArrays(result, ProfileEvaluatorArrays(*states_[state]));
                return result;
            }
        };

        template <class F_, class G_>
        PortfolioWeightedBatchResult_ RunPassiveBatch(const PreparedPortfolio_& portfolio,
                                                      size_t group,
                                                      const PathBatch_& batch,
                                                      const Vector_<PortfolioBatchOutput_>& outputs,
                                                      const BatchSelection_& selection,
                                                      const F_& build,
                                                      const G_& evaluate) {
            const auto& representative = portfolio.Trades()[selection.tradePositions_.front()];
            auto model = CreateModel<double>(portfolio.Portfolio()->Models()[portfolio.Groups()[group].modelOwner_]);
            model->Allocate(representative.TimeLine(), representative.DefLine());
            model->Init(representative.TimeLine(), representative.DefLine());
            PortfolioPassivePathState_ pathState(representative, *model);
            auto states = BuildStates(portfolio, selection, build);
            PortfolioWeightedBatchResult_ result(outputs.size());
            result.tradePositions_ = selection.tradePositions_;
            const PassiveWeightedCollector_ collector(outputs, selection, states, &result);
            result.weightedSum_ = EvaluateDoubleBatch(
                *model, &pathState, states.front().get(), batch, representative.PayOffIdx(),
                [&](const auto& path, auto&) {
                    ++result.generatedScenarios_;
                    for (size_t state = 0; state < states.size(); ++state) {
                        EvaluateTrade(portfolio, selection.tradePositions_[state], path, states[state].get(), evaluate);
                        ++result.evaluatorCalls_;
                    }
                },
                collector);
            REQUIRE2(std::isfinite(result.weightedSum_), "InvalidPortfolioPayoff: non-finite weighted sum", ScriptError_);
            return result;
        }

        template <class O_, class F_, class G_>
        typename O_::Result_ RunBatch(const PreparedPortfolio_& portfolio,
                                      size_t group,
                                      const PathBatch_& batch,
                                      const Vector_<PortfolioBatchOutput_>& outputs,
                                      const BatchSelection_& selection,
                                      const F_& build,
                                      const G_& evaluate,
                                      AAD::TapeCapacityBudget_* tape,
                                      size_t width) {
            std::optional<AAD::TapeCapacityScope_> capacity;
            if (tape)
                capacity.emplace(tape, true);
            auto mode = AAD::SetNumResultsForAAD(O_::BLOCKED, width);
            O_ objective(width);
            AAD::RecordingScope_ recording;
            auto model = CreateModel<AAD::Number_>(portfolio.Portfolio()->Models()[portfolio.Groups()[group].modelOwner_]);
            const auto& representative = portfolio.Trades()[selection.tradePositions_.front()];
            model->Allocate(representative.TimeLine(), representative.DefLine());
            auto random = CreateRNG(representative.Simulation().rsg_, *model, representative.Simulation().useBb_);
            Vector_<double> gauss(model->SimDim());
            AAD::Scenario_<AAD::Number_> path;
            AAD::AllocatePath(representative.DefLine(), path);
            auto states = BuildStates(portfolio, selection, build);
            Vector_<AAD::Number_> values(outputs.size());
            typename O_::Result_ result(outputs.size());
            result.tradePositions_ = selection.tradePositions_;
            if (random)
                random->SkipNormalTo(batch.firstPath_);
            const auto checkpoint = InitializeRecording(portfolio, selection, model.get(), &path, &states, &recording, &objective);
            recording.FinishRecording();
            WithPathGenerator(*model, [&](const auto& generate) {
                for (size_t i = 0; i < batch.pathCount_; ++i) {
                    recording.Restore(checkpoint);
                    if (random)
                        random->FillNormal(&gauss);
                    generate(gauss, &path);
                    ++result.generatedScenarios_;
                    for (size_t state = 0; state < states.size(); ++state) {
                        EvaluateTrade(portfolio, selection.tradePositions_[state], path, states[state].get(), evaluate);
                        ++result.evaluatorCalls_;
                    }
                    CollectOutputs(outputs, selection, states, &values, &result);
                    auto root = objective.Root(values, selection.weights_);
                    if constexpr (!O_::BLOCKED)
                        AddValue(Value(root), &result.weightedSum_, "weighted");
                    recording.FinishRecording();
                    objective.Seed(&root);
                    recording.ReverseSuffix(checkpoint);
                    ++result.suffixReversals_;
                }
            });
            recording.ReversePrefix(checkpoint);
            ++result.prefixReversals_;
            ExtractGradients(*model, states, objective.Width(), &result);
            recording.Close();
            return result;
        }

        template <class O_>
        typename O_::Result_ RunSelectedNative(const PreparedPortfolio_& portfolio,
                                               size_t group,
                                               const PathBatch_& batch,
                                               const Vector_<PortfolioBatchOutput_>& outputs,
                                               const BatchSelection_& selection,
                                               AAD::TapeCapacityBudget_* tape,
                                               size_t width) {
            const auto& representative = portfolio.Trades()[selection.tradePositions_.front()];
            REQUIRE2(representative.Simulation().enableAad_, "UnsupportedPortfolioJacobianBatch: native preparation is required", ScriptError_);
            if (representative.Simulation().compiled_.value_or(false))
                return RunBatch<O_>(
                    portfolio, group, batch, outputs, selection, [](const auto& trade) { return trade.template BuildEvalState<AAD::Number_>(); },
                    [](const auto& trade, const auto& path, auto& state) { trade.CompiledProgram(true).Evaluate(path, state); }, tape, width);
            return RunBatch<O_>(
                portfolio, group, batch, outputs, selection,
                [](const auto& trade) { return trade.template BuildFuzzyEvaluator<AAD::Number_>(0, trade.Simulation().smooth_); },
                [](const auto& trade, const auto& path, auto& state) { trade.Evaluate(path, state); }, tape, width);
        }

        PortfolioWeightedBatchResult_ RunSelectedPassive(const PreparedPortfolio_& portfolio,
                                                         size_t group,
                                                         const PathBatch_& batch,
                                                         const Vector_<PortfolioBatchOutput_>& outputs,
                                                         const BatchSelection_& selection) {
            const auto& representative = portfolio.Trades()[selection.tradePositions_.front()];
            if (representative.Simulation().compiled_.value_or(false))
                return RunPassiveBatch(
                    portfolio, group, batch, outputs, selection, [](const auto& trade) { return trade.template BuildEvalState<double>(); },
                    [](const auto& trade, const auto& path, auto& state) { trade.CompiledProgram(false).Evaluate(path, state); });
            return RunPassiveBatch(
                portfolio, group, batch, outputs, selection, [](const auto& trade) { return trade.template BuildEvaluator<double>(); },
                [](const auto& trade, const auto& path, auto& state) { trade.Evaluate(path, state); });
        }

        template <class O_>
        typename O_::Result_ RunSelectedBatch(const PreparedPortfolio_& portfolio,
                                              size_t group,
                                              const PathBatch_& batch,
                                              const Vector_<PortfolioBatchOutput_>& outputs,
                                              const BatchSelection_& selection,
                                              AAD::TapeCapacityBudget_* tape,
                                              size_t width) {
            if constexpr (!O_::BLOCKED)
                if (!portfolio.Trades()[selection.tradePositions_.front()].Simulation().enableAad_)
                    return RunSelectedPassive(portfolio, group, batch, outputs, selection);
            return RunSelectedNative<O_>(portfolio, group, batch, outputs, selection, tape, width);
        }

        template <class O_>
        typename O_::Result_ ExecuteBatch(const PreparedPortfolio_& portfolio,
                                          size_t group,
                                          const PathBatch_& batch,
                                          const Vector_<PortfolioBatchOutput_>& requestedOutputs,
                                          size_t width,
                                          BufferCapacityBudget_* scratch,
                                          AAD::TapeCapacityBudget_* tape) {
            ValidateBatchRange(portfolio, group, batch);
            const auto execute = [&] {
                const auto outputs = requestedOutputs;
                const auto selection = SelectBatchOutputs(portfolio, portfolio.Groups()[group], outputs);
                const auto& representative = portfolio.Trades()[selection.tradePositions_.front()];
                ValidateRNG(representative.Simulation().rsg_);
                const auto run = [&] { return RunSelectedBatch<O_>(portfolio, group, batch, outputs, selection, tape, width); };
                if (scratch) {
                    const auto fixed =
                        O_::BLOCKED ? PortfolioJacobianWorkerFixedBytes(representative.Simulation().compiled_.value_or(false),
                                                                        selection.tradePositions_.size())
                                    : PortfolioWeightedWorkerFixedBytes(
                                          representative.Simulation().compiled_.value_or(false), selection.tradePositions_.size(),
                                          representative.Simulation().enableAad_,
                                          typeid(*portfolio.Portfolio()->Models()[portfolio.Groups()[group].modelOwner_]) == typeid(BSModelData_));
                    auto capacity = BufferCapacityScope_::ForWorker(scratch, fixed);
                    return run();
                }
                return run();
            };
            try {
                if (scratch) {
                    auto capacity = BufferCapacityScope_::ForWorker(scratch);
                    return execute();
                }
                return execute();
            } catch (const std::exception& error) {
                const String_ type = O_::BLOCKED ? "PortfolioJacobianBatchFailed" : "PortfolioWeightedBatchFailed";
                THROW2(type + ": group=" + String_(std::to_string(group)) + BatchFailureContext(portfolio, group, requestedOutputs) +
                           "; cause=" + String_(error.what()),
                       ScriptError_);
            }
        }
    } // namespace

    size_t PortfolioWeightedWorkerFixedBytes(bool compiled, size_t trades, bool native, bool checkedPaths) {
        if (!native) {
            const auto state = compiled ? sizeof(EvalState_<double>) : sizeof(Evaluator_<double>);
            auto fixed = ReplayExtentProduct(trades, state);
            fixed = ReplayExtentSum(fixed, sizeof(PortfolioPassivePathState_) + sizeof(Vector_<std::unique_ptr<Evaluator_<double>>>));
            fixed = ReplayExtentSum(fixed, sizeof(PortfolioWeightedBatchResult_) + sizeof(BatchSelection_) +
                                               sizeof(PassiveWeightedCollector_<Evaluator_<double>>));
            return ReplayExtentSum(fixed, checkedPaths ? sizeof(LocalCheckedPaths_) : 0);
        }
        const auto state = compiled ? sizeof(EvalState_<AAD::Number_>) : sizeof(FuzzyEvaluator_<AAD::Number_>);
        const auto past = compiled ? sizeof(EvalState_<AAD::Number_>) : sizeof(PastEvaluator_<AAD::Number_>);
        auto fixed = ReplayExtentSum(ReplayExtentProduct(trades, state), std::max(state, past));
        fixed = ReplayExtentSum(fixed, sizeof(PortfolioWeightedBatchResult_) + sizeof(BatchSelection_));
        return ReplayExtentSum(fixed, sizeof(Vector_<AAD::Number_>) + sizeof(Scenario_<AAD::Number_>) + sizeof(Vector_<double>));
    }

    size_t PortfolioJacobianWorkerFixedBytes(bool compiled, size_t trades) {
        const auto fixed = PortfolioWeightedWorkerFixedBytes(compiled, trades) - sizeof(PortfolioWeightedBatchResult_);
        return ReplayExtentSum(fixed, sizeof(PortfolioJacobianBatchResult_) + sizeof(BlockObjective_));
    }

    PortfolioWeightedBatchResult_ EvaluatePortfolioWeightedBatch(const PreparedPortfolio_& portfolio,
                                                                 size_t group,
                                                                 const PathBatch_& batch,
                                                                 const Vector_<PortfolioBatchOutput_>& requestedOutputs,
                                                                 BufferCapacityBudget_* scratch,
                                                                 AAD::TapeCapacityBudget_* tape) {
        return ExecuteBatch<WeightedObjective_>(portfolio, group, batch, requestedOutputs, 1, scratch, tape);
    }

    PortfolioJacobianBatchResult_ EvaluatePortfolioJacobianBatch(const PreparedPortfolio_& portfolio,
                                                                 size_t group,
                                                                 const PathBatch_& batch,
                                                                 const Vector_<PortfolioBatchOutput_>& outputs,
                                                                 size_t width,
                                                                 BufferCapacityBudget_* scratch,
                                                                 AAD::TapeCapacityBudget_* tape) {
        REQUIRE2(width > 0 && width <= AAD::ADJ_SIZE && !outputs.empty() && outputs.size() <= width,
                 "InvalidPortfolioJacobianBatch: positive live outputs must fit the bounded width; field=width", ScriptError_);
        return ExecuteBatch<BlockObjective_>(portfolio, group, batch, outputs, width, scratch, tape);
    }
} // namespace Dal::Script::Detail

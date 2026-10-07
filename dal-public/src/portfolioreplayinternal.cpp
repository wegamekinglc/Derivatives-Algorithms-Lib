//
// Created by Codex on 2026/10/6.
//

#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <type_traits>

#include <dal-public/src/portfolioplaninternal.hpp>
#include <dal-public/src/portfolioreplayinternal.hpp>
#include <dal-public/src/portfoliorisk.hpp>
#include <dal-public/src/riskvalueinternal.hpp>
#include <dal/model/factory.hpp>
#include <dal/platform/platform.hpp>
#include <dal/script/blockedreplay.hpp>
#include <dal/script/riskaxisinternal.hpp>
#include <dal/script/simulation.hpp>

namespace Dal::Detail {
    namespace {
        using Script::Detail::PortfolioBatchOutput_;
        using Script::Detail::PortfolioJacobianBatchResult_;
        using Script::Detail::PortfolioWeightedBatchResult_;
        using Script::Detail::PreparedPortfolio_;

        struct ReplaySelection_ {
            Vector_<Vector_<PortfolioBatchOutput_>> groupOutputs_;
            Vector_<Vector_<size_t>> outputPositions_;
            Vector_<int> inputColumns_;
            const Script::ScriptPortfolioData_* portfolio_ = nullptr;
            std::optional<Script::Detail::PortfolioGradientSelection_> gradients_;

            [[nodiscard]] const Script::Detail::PortfolioGradientSelection_* Gradients() const { return gradients_ ? &*gradients_ : nullptr; }
        };

        bool ValidatePreparedMode(const PreparedPortfolio_& portfolio, const Vector_<size_t>& inputs) {
            const bool native = portfolio.Trades().front().Simulation().enableAad_;
            REQUIRE2(native || inputs.empty(), "InvalidPortfolioReplay: passive execution cannot select risk inputs; field=inputs", ScriptError_);
            for (const auto& trade : portfolio.Trades())
                REQUIRE2(trade.Simulation().enableAad_ == native, "InvalidPortfolioReplay: inconsistent preparation modes", ScriptError_);
            return native;
        }

        void ValidateGroupWidths(const ReplaySelection_& selection, const Vector_<size_t>& widths, bool native) {
            for (size_t group = 0; group < widths.size(); ++group) {
                if (selection.groupOutputs_[group].empty())
                    continue;
                REQUIRE2(widths[group] > 0 && widths[group] <= AAD::ADJ_SIZE && (native || widths[group] == 1),
                         "InvalidPortfolioReplay: invalid selected group width; field=widths; group=" + String_(std::to_string(group)), ScriptError_);
            }
        }

        void ValidatePreparedAxes(const PreparedPortfolio_& portfolio, const PortfolioRiskAxes_& axes) {
            for (size_t trade = 0; trade < portfolio.Trades().size(); ++trade) {
                const auto& prepared = portfolio.Trades()[trade];
                const auto model = CreateModel<double>(portfolio.Portfolio()->Models()[portfolio.Portfolio()->ModelOwners()[trade]]);
                auto local = ScriptRiskInputAxis(*model, prepared.Product());
                const auto& positions = axes.TradeInputPositions()[trade];
                REQUIRE2(local.size() == positions.size(), "InvalidPortfolioReplay: prepared input extent changed", ScriptError_);
                Vector_<Script::RiskCoordinate_> expected;
                for (size_t input = 0; input < local.size(); ++input) {
                    expected.push_back(axes.InputAxis()[positions[input]]);
                    local[input].id_ = expected.back().id_;
                }
                Script::Detail::ValidatePreparedRiskInputs(expected, local, "InvalidPortfolioReplay");
            }
        }

        Vector_<int> SelectInputColumns(const PortfolioRiskAxes_& axes, const Vector_<size_t>& inputs) {
            REQUIRE2(inputs.size() <= static_cast<size_t>(std::numeric_limits<int>::max()), "InvalidPortfolioReplay: too many selected inputs",
                     ScriptError_);
            Vector_<int> columns(axes.InputAxis().size(), -1);
            for (size_t column = 0; column < inputs.size(); ++column) {
                REQUIRE2(inputs[column] < columns.size(), "InvalidPortfolioReplay: input position is out of range; field=inputs", ScriptError_);
                REQUIRE2(columns[inputs[column]] == -1, "InvalidPortfolioReplay: repeated input position; field=inputs", ScriptError_);
                columns[inputs[column]] = static_cast<int>(column);
            }
            return columns;
        }

        size_t FullModelInputCount(const PortfolioRiskAxes_& axes, size_t trade) {
            const auto& positions = axes.TradeInputPositions()[trade];
            size_t inputs = 0;
            while (inputs < positions.size() && axes.InputAxis()[positions[inputs]].family_ == "model")
                ++inputs;
            return inputs;
        }

        Script::Detail::PortfolioInputColumns_
        SelectGradientColumns(const Vector_<size_t>& positions, size_t first, size_t count, const Vector_<int>& columns) {
            size_t selectedCount = 0;
            for (size_t ordinal = 0; ordinal < count; ++ordinal)
                selectedCount += columns[positions[first + ordinal]] >= 0;
            if (selectedCount == count)
                return Script::Detail::PortfolioInputColumns_(count);
            Vector_<size_t> selected;
            selected.reserve(selectedCount);
            for (size_t ordinal = 0; ordinal < count; ++ordinal)
                if (columns[positions[first + ordinal]] >= 0)
                    selected.push_back(ordinal);
            return Script::Detail::PortfolioInputColumns_(count, std::move(selected));
        }

        Script::Detail::PortfolioGradientSelection_
        SelectGradientInputs(const Script::ScriptPortfolioData_& portfolio, const PortfolioRiskAxes_& axes, const Vector_<int>& columns) {
            Script::Detail::PortfolioGradientSelection_ selection;
            selection.models_.reserve(portfolio.Models().size());
            for (size_t owner = 0; owner < portfolio.Models().size(); ++owner) {
                const auto first = std::find(portfolio.ModelOwners().begin(), portfolio.ModelOwners().end(), static_cast<int>(owner));
                const auto trade = static_cast<size_t>(first - portfolio.ModelOwners().begin());
                selection.models_.push_back(SelectGradientColumns(axes.TradeInputPositions()[trade], 0, FullModelInputCount(axes, trade), columns));
            }
            selection.constants_.reserve(portfolio.TradeIds().size());
            for (size_t trade = 0; trade < portfolio.TradeIds().size(); ++trade) {
                const auto& positions = axes.TradeInputPositions()[trade];
                const auto modelInputs = FullModelInputCount(axes, trade);
                selection.constants_.push_back(SelectGradientColumns(positions, modelInputs, positions.size() - modelInputs, columns));
            }
            return selection;
        }

        size_t ValidateOutput(const PortfolioRiskAxes_& axes, const std::map<String_, size_t>& available, const PortfolioBatchOutput_& output) {
            const auto found = available.find(output.coordinate_.id_);
            REQUIRE2(found != available.end(), "InvalidPortfolioReplay: unknown output; output=" + output.coordinate_.id_, ScriptError_);
            const auto& expected = axes.OutputAxis()[found->second];
            REQUIRE2(output.tradePosition_ == axes.OutputTrades()[found->second] && output.coordinate_.label_ == expected.label_ &&
                         output.coordinate_.slot_ == expected.slot_,
                     "InvalidPortfolioReplay: output coordinate changed; output=" + output.coordinate_.id_, ScriptError_);
            REQUIRE2(std::isfinite(output.weight_), "InvalidPortfolioReplay: weight must be finite; output=" + output.coordinate_.id_, ScriptError_);
            return output.tradePosition_;
        }

        ReplaySelection_ SelectOutputs(const Vector_<Script::Detail::PortfolioScenarioGroup_>& groups,
                                       const Handle_<Script::ScriptPortfolioData_>& portfolio,
                                       const PortfolioRiskAxes_& axes,
                                       const Vector_<PortfolioBatchOutput_>& outputs,
                                       const Vector_<size_t>& inputs,
                                       bool native) {
            REQUIRE2(!outputs.empty(), "InvalidPortfolioReplay: outputs must not be empty; field=outputs", ScriptError_);
            ReplaySelection_ selection{Vector_<Vector_<PortfolioBatchOutput_>>(groups.size()),
                                       Vector_<Vector_<size_t>>(groups.size()),
                                       SelectInputColumns(axes, inputs),
                                       portfolio.get(),
                                       {}};
            if (native && inputs.size() != axes.InputAxis().size())
                selection.gradients_.emplace(SelectGradientInputs(*portfolio, axes, selection.inputColumns_));
            Vector_<size_t> tradeGroups(portfolio->TradeIds().size());
            for (size_t group = 0; group < groups.size(); ++group)
                for (const auto trade : groups[group].tradePositions_)
                    tradeGroups[trade] = group;
            std::map<String_, size_t> available;
            for (size_t output = 0; output < axes.OutputAxis().size(); ++output)
                available.emplace(axes.OutputAxis()[output].id_, output);
            std::set<String_> ids;
            for (size_t component = 0; component < outputs.size(); ++component) {
                const auto& output = outputs[component];
                const auto trade = ValidateOutput(axes, available, output);
                REQUIRE2(ids.insert(output.coordinate_.id_).second, "InvalidPortfolioReplay: repeated output; output=" + output.coordinate_.id_,
                         ScriptError_);
                selection.groupOutputs_[tradeGroups[trade]].push_back(output);
                selection.outputPositions_[tradeGroups[trade]].push_back(component);
            }
            return selection;
        }

        void AddValue(double value, double* sum, const String_& coordinate) {
            REQUIRE2(std::isfinite(value) && std::isfinite(*sum + value), "InvalidPortfolioReplay: non-finite value or sum; coordinate=" + coordinate,
                     ScriptError_);
            *sum += value;
        }

        void AddGradient(double value,
                         size_t position,
                         const PortfolioRiskAxes_& axes,
                         const ReplaySelection_& selection,
                         PortfolioWeightedReplayResult_* result) {
            const auto column = selection.inputColumns_[position];
            if (column >= 0)
                AddValue(value, &result->gradient_[static_cast<size_t>(column)], axes.InputAxis()[position].id_);
        }

        template <class G_> size_t GradientColumns(const G_& gradients) {
            if constexpr (std::is_same_v<G_, Vector_<double>>)
                return gradients.size();
            else
                return static_cast<size_t>(gradients.Cols());
        }

        size_t OriginalInputOrdinal(const Script::Detail::PortfolioInputColumns_* columns, size_t packed) {
            return columns ? columns->Original(packed) : packed;
        }

        template <class G_, class F_>
        void VisitGradients(const Script::Detail::PortfolioBatchResult_<G_>& batch,
                            const PortfolioRiskAxes_& axes,
                            const ReplaySelection_& selection,
                            const F_& accept) {
            if (batch.constantGradientSums_.empty())
                return;
            const auto* modelColumns =
                selection.gradients_
                    ? &selection.gradients_->Model(static_cast<size_t>(selection.portfolio_->ModelOwners()[batch.tradePositions_.front()]))
                    : nullptr;
            const auto modelInputs = modelColumns ? modelColumns->SourceExtent() : GradientColumns(batch.modelGradientSums_);
            const auto& representative = axes.TradeInputPositions()[batch.tradePositions_.front()];
            for (size_t input = 0; input < GradientColumns(batch.modelGradientSums_); ++input)
                accept(batch.modelGradientSums_, input, representative[OriginalInputOrdinal(modelColumns, input)]);
            for (size_t trade = 0; trade < batch.tradePositions_.size(); ++trade) {
                const auto& positions = axes.TradeInputPositions()[batch.tradePositions_[trade]];
                const auto* columns = selection.gradients_ ? &selection.gradients_->Constants(batch.tradePositions_[trade]) : nullptr;
                for (size_t constant = 0; constant < GradientColumns(batch.constantGradientSums_[trade]); ++constant)
                    accept(batch.constantGradientSums_[trade], constant, positions[modelInputs + OriginalInputOrdinal(columns, constant)]);
            }
        }

        void ScatterGradients(const PortfolioWeightedBatchResult_& batch,
                              const PortfolioRiskAxes_& axes,
                              const ReplaySelection_& selection,
                              PortfolioWeightedReplayResult_* result) {
            VisitGradients(batch, axes, selection, [&](const auto& gradients, size_t local, size_t global) {
                AddGradient(gradients[local], global, axes, selection, result);
            });
        }

        void AddCount(size_t value, size_t* sum) {
            REQUIRE2(value <= std::numeric_limits<size_t>::max() - *sum, "InvalidPortfolioReplay: execution counter overflows", ScriptError_);
            *sum += value;
        }

        void ReduceGroup(const Vector_<PortfolioWeightedBatchResult_>& batches,
                         size_t group,
                         const PortfolioRiskAxes_& axes,
                         const ReplaySelection_& selection,
                         PortfolioWeightedReplayResult_* result) {
            auto& counters = result->groupCounters_[group];
            for (const auto& batch : batches) {
                AddValue(batch.weightedSum_, &result->weightedValue_, "weighted");
                for (size_t component = 0; component < batch.componentSums_.size(); ++component)
                    AddValue(batch.componentSums_[component], &result->componentMeans_[selection.outputPositions_[group][component]],
                             selection.groupOutputs_[group][component].coordinate_.id_);
                ScatterGradients(batch, axes, selection, result);
                AddCount(batch.generatedScenarios_, &counters.generatedScenarios_);
                AddCount(batch.evaluatorCalls_, &counters.evaluatorCalls_);
                AddCount(batch.suffixReversals_, &counters.suffixReversals_);
                AddCount(batch.prefixReversals_, &counters.prefixReversals_);
            }
        }

        template <class R_, class F_, class G_>
        Vector_<R_> RunBatches(const Script::BatchPlan_& batches, size_t outputs, BufferCapacityBudget_* scratch, const F_& run, const G_& single) {
            Vector_<R_> slots;
            slots.reserve(batches.BatchCount());
            for (size_t batch = 0; batch < batches.BatchCount(); ++batch)
                slots.emplace_back(outputs);
            {
                const auto workers = std::min(ThreadPool_::GetInstance()->NumThreads(), batches.BatchCount());
                auto futures = BufferCapacityScope_::ForWorker(scratch, Script::Detail::ReplayExtentProduct(workers, sizeof(TaskHandle_)));
                Dal::Detail::BufferCapacitySuspension_ suspension;
                Script::SimulationTaskGroup_ tasks(ThreadPool_::GetInstance(), workers);
                for (size_t worker = 0; worker < workers; ++worker)
                    tasks.Spawn([&, worker] {
                        auto capacity = BufferCapacityScope_::ForWorker(scratch);
                        if (workers == batches.BatchCount())
                            slots[worker] = single(batches.BatchAt(worker));
                        else
                            run(worker, workers, &slots);
                        return true;
                    });
                tasks.Complete();
            }
            return slots;
        }

        void RunGroup(const PreparedPortfolio_& portfolio,
                      size_t group,
                      const Script::BatchPlan_& batches,
                      const PortfolioRiskAxes_& axes,
                      const ReplaySelection_& selection,
                      PortfolioWeightedReplayResult_* result,
                      BufferCapacityBudget_* scratch,
                      AAD::TapeCapacityBudget_* tape) {
            try {
                const auto slots = RunBatches<PortfolioWeightedBatchResult_>(
                    batches, selection.groupOutputs_[group].size(), scratch,
                    [&](size_t worker, size_t workers, auto* results) {
                        Script::Detail::EvaluatePortfolioWeightedWorker(portfolio, group, batches, worker, workers, selection.groupOutputs_[group],
                                                                        results, {scratch, tape, selection.Gradients()});
                    },
                    [&](const auto& paths) {
                        return Script::Detail::EvaluatePortfolioWeightedBatch(portfolio, group, paths, selection.groupOutputs_[group],
                                                                              {scratch, tape, selection.Gradients()});
                    });
                ReduceGroup(slots, group, axes, selection, result);
            } catch (const std::exception& error) {
                THROW2("PortfolioWeightedReplayFailed: group=" + String_(std::to_string(group)) + "; cause=" + String_(error.what()), ScriptError_);
            }
        }

        template <class R_> void AddCounters(const R_& batch, PortfolioGroupCounters_* counters) {
            AddCount(batch.generatedScenarios_, &counters->generatedScenarios_);
            AddCount(batch.evaluatorCalls_, &counters->evaluatorCalls_);
            AddCount(batch.suffixReversals_, &counters->suffixReversals_);
            AddCount(batch.prefixReversals_, &counters->prefixReversals_);
        }

        template <class G_> double RowGradient(const G_& gradients, size_t lane, size_t column) {
            if constexpr (std::is_same_v<G_, Vector_<double>>)
                return gradients[column];
            else
                return gradients(static_cast<int>(lane), static_cast<int>(column));
        }

        template <class G_>
        void ScatterJacobianRow(const Script::Detail::PortfolioBatchResult_<G_>& batch,
                                size_t lane,
                                size_t row,
                                const PortfolioRiskAxes_& axes,
                                const ReplaySelection_& selection,
                                PortfolioJacobianReplayResult_* result) {
            VisitGradients(batch, axes, selection, [&](const auto& gradients, size_t local, size_t global) {
                const auto column = selection.inputColumns_[global];
                if (column >= 0)
                    AddValue(RowGradient(gradients, lane, local), &result->jacobian_(static_cast<int>(row), column), axes.InputAxis()[global].id_);
            });
        }

        template <class R_>
        void ReduceJacobianBlock(const Vector_<R_>& batches,
                                 size_t group,
                                 const AAD::AdjointBlock_& block,
                                 const PortfolioRiskAxes_& axes,
                                 const ReplaySelection_& selection,
                                 PortfolioJacobianReplayResult_* result) {
            for (const auto& batch : batches) {
                for (size_t lane = 0; lane < block.outputs_; ++lane) {
                    const auto component = block.firstOutput_ + lane;
                    const auto row = selection.outputPositions_[group][component];
                    AddValue(batch.componentSums_[lane], &result->componentMeans_[row], selection.groupOutputs_[group][component].coordinate_.id_);
                    ScatterJacobianRow(batch, lane, row, axes, selection, result);
                }
                AddCounters(batch, &result->groupCounters_[group]);
            }
        }

        void RunJacobianBlock(const PreparedPortfolio_& portfolio,
                              size_t group,
                              const Script::BatchPlan_& batches,
                              const AAD::AdjointBlock_& block,
                              const PortfolioRiskAxes_& axes,
                              const ReplaySelection_& selection,
                              PortfolioJacobianReplayResult_* result,
                              BufferCapacityBudget_* scratch,
                              AAD::TapeCapacityBudget_* tape) {
            auto& counters = result->groupCounters_[group];
            AddCount(1, &counters.replayAttempts_);
            counters.actualWidths_.push_back(block.width_);
            try {
                const auto begin = selection.groupOutputs_[group].begin() + static_cast<ptrdiff_t>(block.firstOutput_);
                const Vector_<PortfolioBatchOutput_> outputs(begin, begin + static_cast<ptrdiff_t>(block.outputs_));
                const auto slots = RunBatches<PortfolioJacobianBatchResult_>(
                    batches, outputs.size(), scratch,
                    [&](size_t worker, size_t workers, auto* results) {
                        Script::Detail::EvaluatePortfolioJacobianWorker(portfolio, group, batches, worker, workers, outputs, block.width_, results,
                                                                        {scratch, tape, selection.Gradients()});
                    },
                    [&](const auto& paths) {
                        return Script::Detail::EvaluatePortfolioJacobianBatch(portfolio, group, paths, outputs, block.width_,
                                                                              {scratch, tape, selection.Gradients()});
                    });
                ReduceJacobianBlock(slots, group, block, axes, selection, result);
            } catch (const std::exception& error) {
                const auto& output = selection.groupOutputs_[group][block.firstOutput_];
                THROW2("PortfolioJacobianReplayFailed: group=" + String_(std::to_string(group)) +
                           "; trade=" + portfolio.Portfolio()->TradeIds()[output.tradePosition_] + "; output=" + output.coordinate_.id_ +
                           "; width=" + String_(std::to_string(block.width_)) + "; attempt=" + String_(std::to_string(counters.replayAttempts_)) +
                           "; cause=" + String_(error.what()),
                       ScriptError_);
            }
        }

        void RunJacobianGroup(const PreparedPortfolio_& portfolio,
                              size_t group,
                              size_t width,
                              const Script::BatchPlan_& batches,
                              const PortfolioRiskAxes_& axes,
                              const ReplaySelection_& selection,
                              PortfolioJacobianReplayResult_* result,
                              BufferCapacityBudget_* scratch,
                              AAD::TapeCapacityBudget_* tape) {
            if (selection.groupOutputs_[group].empty())
                return;
            AAD::AdjointBlockSettings_ settings;
            settings.maxWidth_ = width;
            const auto plan = AAD::PlanAdjointBlocks(selection.groupOutputs_[group].size(), result->jacobian_.Cols(), settings);
            for (size_t block = 0; block < plan.BlockCount(); ++block)
                RunJacobianBlock(portfolio, group, batches, plan.Block(block), axes, selection, result, scratch, tape);
        }

        void RunPassiveJacobianGroup(const PreparedPortfolio_& portfolio,
                                     size_t group,
                                     const Script::BatchPlan_& batches,
                                     const PortfolioRiskAxes_& axes,
                                     const ReplaySelection_& selection,
                                     PortfolioJacobianReplayResult_* result,
                                     BufferCapacityBudget_* scratch) {
            if (selection.groupOutputs_[group].empty())
                return;
            AddCount(1, &result->groupCounters_[group].replayAttempts_);
            try {
                auto outputs = selection.groupOutputs_[group];
                for (auto& output : outputs)
                    output.weight_ = 0.0;
                const auto slots = RunBatches<PortfolioWeightedBatchResult_>(
                    batches, outputs.size(), scratch,
                    [&](size_t worker, size_t workers, auto* results) {
                        Script::Detail::EvaluatePortfolioWeightedWorker(portfolio, group, batches, worker, workers, outputs, results, {scratch});
                    },
                    [&](const auto& paths) { return Script::Detail::EvaluatePortfolioWeightedBatch(portfolio, group, paths, outputs, {scratch}); });
                ReduceJacobianBlock(slots, group, {0, outputs.size(), 1}, axes, selection, result);
            } catch (const std::exception& error) {
                THROW2("PortfolioJacobianReplayFailed: passive; group=" + String_(std::to_string(group)) + "; cause=" + String_(error.what()),
                       ScriptError_);
            }
        }

        struct AdmissionTrades_ {
            Vector_<const Script::PreparedScript_*> plans_;
            Vector_<size_t> positions_;
        };

        AdmissionTrades_ SelectedPlans(const Vector_<Script::Detail::PlannedScript_>& plans,
                                       const Script::Detail::PortfolioScenarioGroup_& group,
                                       const Vector_<PortfolioBatchOutput_>& outputs,
                                       bool selectedInputs) {
            AdmissionTrades_ trades;
            for (const auto trade : group.tradePositions_)
                if (std::any_of(outputs.begin(), outputs.end(), [&](const auto& output) { return output.tradePosition_ == trade; })) {
                    trades.plans_.push_back(&plans[trade].View());
                    if (selectedInputs)
                        trades.positions_.push_back(trade);
                }
            return trades;
        }

        template <class G_>
        Vector_<Script::Detail::PortfolioBatchResult_<G_>> AdmissionSlots(const AdmissionTrades_& trades,
                                                                          size_t modelInputs,
                                                                          size_t outputs,
                                                                          size_t count,
                                                                          bool native,
                                                                          const Script::Detail::PortfolioAdmissionSettings_& settings) {
            if (settings.gradients_) {
                const auto& columns = settings.gradients_->Model(settings.modelOwner_);
                columns.ValidateExtent(modelInputs);
                modelInputs = columns.Size();
            }
            Vector_<Script::Detail::PortfolioBatchResult_<G_>> slots;
            slots.reserve(count);
            for (size_t batch = 0; batch < count; ++batch) {
                slots.emplace_back(outputs);
                slots.back().tradePositions_.Resize(trades.plans_.size());
                if (native) {
                    Script::Detail::ResizePortfolioGradientStorage(&slots.back().modelGradientSums_, settings.width_, modelInputs);
                    slots.back().constantGradientSums_.Resize(trades.plans_.size());
                    for (size_t trade = 0; trade < trades.plans_.size(); ++trade) {
                        auto inputs = trades.plans_[trade]->ConstVarNames().size();
                        if (settings.gradients_) {
                            const auto& columns = settings.gradients_->Constants(trades.positions_[trade]);
                            columns.ValidateExtent(inputs);
                            inputs = columns.Size();
                        }
                        Script::Detail::ResizePortfolioGradientStorage(&slots.back().constantGradientSums_[trade], settings.width_, inputs);
                    }
                }
            }
            return slots;
        }

        template <class G_>
        void AdmitGroup(const PortfolioRiskPlan_& plan,
                        const Vector_<Script::Detail::PlannedScript_>& plans,
                        const Script::Detail::PortfolioScenarioGroup_& group,
                        const Vector_<PortfolioBatchOutput_>& outputs,
                        const Script::BatchPlan_& batches,
                        Script::Detail::PortfolioAdmissionSettings_ settings,
                        size_t workers,
                        BufferCapacityBudget_* scratch,
                        const Script::Detail::PortfolioCapacityLimits_& limits) {
            const auto trades = SelectedPlans(plans, group, outputs, settings.gradients_ != nullptr);
            const auto representative = group.tradePositions_.front();
            settings.modelOwner_ = group.modelOwner_;
            settings.tradePositions_ = &trades.positions_;
            const auto slots = AdmissionSlots<G_>(trades, plans[representative].Model().Parameters().size(), outputs.size(), batches.BatchCount(),
                                                  plan.EnableAad(), settings);
            const auto futures = Script::Detail::ReplayExtentProduct(workers, sizeof(TaskHandle_));
            auto taskCapacity = BufferCapacityScope_::ForWorker(scratch, futures);
            settings.scratchQuota_ = (scratch->LimitBytes() - scratch->CapacityBytes()) / workers;
            settings.tapeQuota_ = limits.tapeBudgetBytes_.value_or(std::numeric_limits<size_t>::max()) / workers;
            Dal::Detail::BufferCapacitySuspension_ suspension;
            if (plan.EnableAad()) {
                if constexpr (std::is_same_v<G_, Matrix_<double>>)
                    Script::Detail::AdmitPortfolioJacobianWorker(trades.plans_, plan.Portfolio()->Models()[group.modelOwner_], outputs, settings);
                else
                    Script::Detail::AdmitPortfolioWeightedWorker(trades.plans_, plan.Portfolio()->Models()[group.modelOwner_], outputs, settings);
            } else
                Script::Detail::AdmitPortfolioPassiveWorker(trades.plans_, plan.Portfolio()->Models()[group.modelOwner_], outputs,
                                                            settings.scratchQuota_);
        }

        void PreflightWeighted(const PortfolioWeightedPlan_& plan,
                               const Vector_<Script::Detail::PlannedScript_>& plans,
                               size_t paths,
                               const Script::Detail::PortfolioCapacityLimits_& limits) {
            if (!limits.scratchBudgetBytes_ && (!plan.EnableAad() || !limits.tapeBudgetBytes_))
                return;
            const auto groups = Script::Detail::PlanPortfolioScenarioGroups(*plan.Portfolio(), plans);
            const auto threads = ThreadPool_::GetInstance()->NumThreads();
            const Script::BatchPlan_ batches(paths, threads);
            const auto workers = std::min(threads, batches.BatchCount());
            BufferCapacityBudget_ scratch(limits.scratchBudgetBytes_.value_or(std::numeric_limits<size_t>::max()));
            BufferCapacityScope_ coordinator(&scratch, sizeof(PortfolioWeightedReplayResult_) + sizeof(ReplaySelection_));
            const auto axes = ScriptPortfolioRiskAxes(plan.Portfolio());
            const auto outputs = plan.Outputs();
            const auto inputs = plan.InputPositions();
            const auto selection = SelectOutputs(groups, plan.Portfolio(), axes, outputs, inputs, plan.EnableAad());
            const PortfolioWeightedReplayResult_ result(outputs.size(), inputs.size(), groups.size());
            for (size_t group = 0; group < groups.size(); ++group) {
                if (selection.groupOutputs_[group].empty())
                    continue;
                try {
                    AdmitGroup<Vector_<double>>(plan, plans, groups[group], selection.groupOutputs_[group], batches, {1, 0, 0, selection.Gradients()},
                                                workers, &scratch, limits);
                } catch (const std::exception& error) {
                    THROW2("PortfolioWeightedPreflightFailed: group=" + String_(std::to_string(group)) +
                               "; trade=" + plan.Portfolio()->TradeIds()[groups[group].tradePositions_.front()] + "; cause=" + String_(error.what()),
                           ScriptError_);
                }
            }
        }

        void AdmitJacobianWidth(const PortfolioJacobianPlan_& plan,
                                const Vector_<Script::Detail::PlannedScript_>& plans,
                                const Script::Detail::PortfolioScenarioGroup_& group,
                                const Vector_<PortfolioBatchOutput_>& outputs,
                                const Script::BatchPlan_& batches,
                                const Script::Detail::PortfolioAdmissionSettings_& settings,
                                size_t workers,
                                BufferCapacityBudget_* scratch,
                                const Script::Detail::PortfolioCapacityLimits_& limits) {
            if (!plan.EnableAad()) {
                AdmitGroup<Vector_<double>>(plan, plans, group, outputs, batches, settings, workers, scratch, limits);
                return;
            }
            const auto width = settings.width_;
            const auto attempts = outputs.size() / width + (outputs.size() % width != 0);
            const Vector_<size_t> reportedWidths(attempts, width);
            for (size_t first = 0; first < outputs.size(); first += width) {
                const auto end = std::min(outputs.size(), first + width);
                const Vector_<PortfolioBatchOutput_> block(outputs.begin() + static_cast<ptrdiff_t>(first),
                                                           outputs.begin() + static_cast<ptrdiff_t>(end));
                AdmitGroup<Matrix_<double>>(plan, plans, group, block, batches, settings, workers, scratch, limits);
            }
        }

        bool HasPortfolioBudget(bool native, const Script::Detail::PortfolioCapacityLimits_& limits) {
            return limits.scratchBudgetBytes_.has_value() || (native && limits.tapeBudgetBytes_.has_value());
        }

        size_t AdmitJacobianGroup(const PortfolioJacobianPlan_& plan,
                                  const Vector_<Script::Detail::PlannedScript_>& plans,
                                  const Vector_<Script::Detail::PortfolioScenarioGroup_>& groups,
                                  size_t group,
                                  const ReplaySelection_& selection,
                                  const Script::BatchPlan_& batches,
                                  size_t workers,
                                  BufferCapacityBudget_* scratch,
                                  const Script::Detail::PortfolioCapacityLimits_& limits) {
            const auto& outputs = selection.groupOutputs_[group];
            if (outputs.empty())
                return 0;
            auto width = plan.EnableAad() ? std::min(plan.MaxBlockWidth(), outputs.size()) : 1;
            if (!HasPortfolioBudget(plan.EnableAad(), limits))
                return width;
            for (;;) {
                try {
                    AdmitJacobianWidth(plan, plans, groups[group], outputs, batches, {width, 0, 0, selection.Gradients()}, workers, scratch, limits);
                    return width;
                } catch (const Exception_& error) {
                    if (std::string(error.what()).find("capacity budget exceeded") == std::string::npos)
                        throw;
                    if (width == 1)
                        THROW2("PortfolioJacobianPreflightFailed: group=" + String_(std::to_string(group)) +
                                   "; trade=" + plan.Portfolio()->TradeIds()[outputs.front().tradePosition_] +
                                   "; output=" + outputs.front().coordinate_.id_ + "; width=1; cause=" + String_(error.what()),
                               ScriptError_);
                    --width;
                }
            }
        }

        Vector_<size_t> PreflightJacobian(const PortfolioJacobianPlan_& plan,
                                          const Vector_<Script::Detail::PlannedScript_>& plans,
                                          size_t paths,
                                          const Script::Detail::PortfolioCapacityLimits_& limits) {
            const auto groups = Script::Detail::PlanPortfolioScenarioGroups(*plan.Portfolio(), plans);
            const auto threads = ThreadPool_::GetInstance()->NumThreads();
            const Script::BatchPlan_ batches(paths, threads);
            const auto workers = std::min(threads, batches.BatchCount());
            BufferCapacityBudget_ scratch(limits.scratchBudgetBytes_.value_or(std::numeric_limits<size_t>::max()));
            BufferCapacityScope_ coordinator(&scratch, sizeof(PortfolioJacobianReplayResult_) + sizeof(ReplaySelection_));
            const auto axes = ScriptPortfolioRiskAxes(plan.Portfolio());
            const auto outputs = plan.Outputs();
            const auto inputs = plan.InputPositions();
            const auto selection = SelectOutputs(groups, plan.Portfolio(), axes, outputs, inputs, plan.EnableAad());
            const PortfolioJacobianReplayResult_ result(outputs.size(), inputs.size(), groups.size());
            Vector_<size_t> widths(groups.size(), 0);
            for (size_t group = 0; group < groups.size(); ++group)
                widths[group] = AdmitJacobianGroup(plan, plans, groups, group, selection, batches, workers, &scratch, limits);
            return widths;
        }
    } // namespace

    PreparedPortfolio_ PreparePortfolioWeightedReplay(const PortfolioWeightedPlan_& requestedPlan,
                                                      int paths,
                                                      const Script::ScriptValuationSettings_& valuation,
                                                      const Script::MonteCarloSettings_& simulation,
                                                      Script::Detail::PortfolioCapacityLimits_ limits) {
        const auto plan = requestedPlan;
        const auto execution = simulation;
        const auto settings = valuation;
        REQUIRE2(plan.EnableAad() == execution.enableAad_, "InvalidPortfolioReplay: request and preparation modes must agree", ScriptError_);
        return Script::Detail::PrepareScriptPortfolio(plan.Portfolio(), paths, settings, execution,
                                                      [&](const auto& plans) { PreflightWeighted(plan, plans, static_cast<size_t>(paths), limits); });
    }

    PortfolioJacobianPreparation_ PreparePortfolioJacobianReplay(const PortfolioJacobianPlan_& requestedPlan,
                                                                 int paths,
                                                                 const Script::ScriptValuationSettings_& valuation,
                                                                 const Script::MonteCarloSettings_& simulation,
                                                                 Script::Detail::PortfolioCapacityLimits_ limits) {
        const auto plan = requestedPlan;
        const auto execution = simulation;
        const auto settings = valuation;
        REQUIRE2(plan.EnableAad() == execution.enableAad_, "InvalidPortfolioReplay: request and preparation modes must agree", ScriptError_);
        Vector_<size_t> widths;
        auto prepared = Script::Detail::PrepareScriptPortfolio(plan.Portfolio(), paths, settings, execution, [&](const auto& plans) {
            widths = PreflightJacobian(plan, plans, static_cast<size_t>(paths), limits);
        });
        return {std::move(prepared), std::move(widths)};
    }

    PortfolioWeightedReplayResult_ EvaluatePortfolioWeightedReplay(const PreparedPortfolio_& portfolio,
                                                                   const Vector_<PortfolioBatchOutput_>& requestedOutputs,
                                                                   const Vector_<size_t>& selectedInputs,
                                                                   Script::Detail::PortfolioCapacityLimits_ limits) {
        const auto threads = ThreadPool_::GetInstance()->NumThreads();
        const bool native = ValidatePreparedMode(portfolio, selectedInputs);
        Script::BatchPlan_ batches(static_cast<size_t>(portfolio.PathCount()), threads);
        const auto cleanup =
            native ? Script::Detail::ReplayExtentProduct(std::min(threads, batches.BatchCount()), AAD::TapeCleanupCapacityBytes()) : 0;
        Script::Detail::RequireReplayCapacity(limits.tapeBudgetBytes_, cleanup, "Tape");
        BufferCapacityBudget_ scratch(limits.scratchBudgetBytes_.value_or(std::numeric_limits<size_t>::max()));
        AAD::TapeCapacityBudget_ tape(limits.tapeBudgetBytes_.value_or(std::numeric_limits<size_t>::max()));
        BufferCapacityScope_ coordinator(&scratch, sizeof(PortfolioWeightedReplayResult_) + sizeof(ReplaySelection_));
        const auto outputs = requestedOutputs;
        const auto inputs = selectedInputs;
        const auto axes = ScriptPortfolioRiskAxes(portfolio.Portfolio());
        ValidatePreparedAxes(portfolio, axes);
        const auto selection = SelectOutputs(portfolio.Groups(), portfolio.Portfolio(), axes, outputs, inputs, native);
        PortfolioWeightedReplayResult_ result(outputs.size(), inputs.size(), portfolio.Groups().size());
        for (size_t group = 0; group < portfolio.Groups().size(); ++group)
            if (!selection.groupOutputs_[group].empty())
                RunGroup(portfolio, group, batches, axes, selection, &result, &scratch, native ? &tape : nullptr);
        const auto paths = static_cast<double>(portfolio.PathCount());
        result.weightedValue_ /= paths;
        for (auto& value : result.componentMeans_)
            value /= paths;
        for (auto& value : result.gradient_)
            value /= paths;
        result.peakScratchBytes_ = scratch.PeakCapacityBytes();
        result.peakTapeBytes_ = tape.PeakCapacityBytes();
        return result;
    }

    PortfolioJacobianReplayResult_ EvaluatePortfolioJacobianReplay(const PreparedPortfolio_& portfolio,
                                                                   const Vector_<PortfolioBatchOutput_>& requestedOutputs,
                                                                   const Vector_<size_t>& selectedInputs,
                                                                   const Vector_<size_t>& groupWidths,
                                                                   Script::Detail::PortfolioCapacityLimits_ limits) {
        const bool native = ValidatePreparedMode(portfolio, selectedInputs);
        const auto threads = ThreadPool_::GetInstance()->NumThreads();
        const Script::BatchPlan_ batches(static_cast<size_t>(portfolio.PathCount()), threads);
        const auto cleanup =
            native ? Script::Detail::ReplayExtentProduct(std::min(threads, batches.BatchCount()), AAD::TapeCleanupCapacityBytes()) : 0;
        if (native)
            Script::Detail::RequireReplayCapacity(limits.tapeBudgetBytes_, cleanup, "Tape");
        BufferCapacityBudget_ scratch(limits.scratchBudgetBytes_.value_or(std::numeric_limits<size_t>::max()));
        AAD::TapeCapacityBudget_ tape(limits.tapeBudgetBytes_.value_or(std::numeric_limits<size_t>::max()));
        BufferCapacityScope_ coordinator(&scratch, sizeof(PortfolioJacobianReplayResult_) + sizeof(ReplaySelection_));
        const auto outputs = requestedOutputs;
        const auto inputs = selectedInputs;
        const auto widths = groupWidths;
        REQUIRE2(widths.size() == portfolio.Groups().size(), "InvalidPortfolioReplay: widths must match groups; field=widths", ScriptError_);
        const auto axes = ScriptPortfolioRiskAxes(portfolio.Portfolio());
        ValidatePreparedAxes(portfolio, axes);
        const auto selection = SelectOutputs(portfolio.Groups(), portfolio.Portfolio(), axes, outputs, inputs, native);
        PortfolioJacobianReplayResult_ result(outputs.size(), inputs.size(), portfolio.Groups().size());
        ValidateGroupWidths(selection, widths, native);
        for (size_t group = 0; group < portfolio.Groups().size(); ++group) {
            if (native)
                RunJacobianGroup(portfolio, group, widths[group], batches, axes, selection, &result, &scratch, &tape);
            else
                RunPassiveJacobianGroup(portfolio, group, batches, axes, selection, &result, &scratch);
        }
        const auto paths = static_cast<double>(portfolio.PathCount());
        for (auto& value : result.componentMeans_)
            value /= paths;
        for (int row = 0; row < result.jacobian_.Rows(); ++row)
            for (int column = 0; column < result.jacobian_.Cols(); ++column)
                result.jacobian_(row, column) /= paths;
        result.peakScratchBytes_ = scratch.PeakCapacityBytes();
        result.peakTapeBytes_ = tape.PeakCapacityBytes();
        return result;
    }
} // namespace Dal::Detail

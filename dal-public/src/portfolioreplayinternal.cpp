//
// Created by Codex on 2026/10/6.
//

#include <cmath>
#include <limits>
#include <map>
#include <set>

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
        using Script::Detail::PortfolioWeightedBatchResult_;
        using Script::Detail::PreparedPortfolio_;

        struct ReplaySelection_ {
            Vector_<Vector_<PortfolioBatchOutput_>> groupOutputs_;
            Vector_<Vector_<size_t>> outputPositions_;
            Vector_<int> inputColumns_;
        };

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
                                       size_t trades,
                                       const PortfolioRiskAxes_& axes,
                                       const Vector_<PortfolioBatchOutput_>& outputs,
                                       const Vector_<size_t>& inputs) {
            REQUIRE2(!outputs.empty(), "InvalidPortfolioReplay: outputs must not be empty; field=outputs", ScriptError_);
            ReplaySelection_ selection{Vector_<Vector_<PortfolioBatchOutput_>>(groups.size()), Vector_<Vector_<size_t>>(groups.size()),
                                       SelectInputColumns(axes, inputs)};
            Vector_<size_t> tradeGroups(trades);
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

        void ScatterGradients(const PortfolioWeightedBatchResult_& batch,
                              const PortfolioRiskAxes_& axes,
                              const ReplaySelection_& selection,
                              PortfolioWeightedReplayResult_* result) {
            if (batch.constantGradientSums_.empty())
                return;
            const auto modelInputs = batch.modelGradientSums_.size();
            const auto& representative = axes.TradeInputPositions()[batch.tradePositions_.front()];
            for (size_t input = 0; input < modelInputs; ++input)
                AddGradient(batch.modelGradientSums_[input], representative[input], axes, selection, result);
            for (size_t trade = 0; trade < batch.tradePositions_.size(); ++trade) {
                const auto& positions = axes.TradeInputPositions()[batch.tradePositions_[trade]];
                for (size_t constant = 0; constant < batch.constantGradientSums_[trade].size(); ++constant)
                    AddGradient(batch.constantGradientSums_[trade][constant], positions[modelInputs + constant], axes, selection, result);
            }
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

        void RunGroup(const PreparedPortfolio_& portfolio,
                      size_t group,
                      const Script::BatchPlan_& batches,
                      const PortfolioRiskAxes_& axes,
                      const ReplaySelection_& selection,
                      PortfolioWeightedReplayResult_* result,
                      BufferCapacityBudget_* scratch,
                      AAD::TapeCapacityBudget_* tape) {
            try {
                Vector_<PortfolioWeightedBatchResult_> slots;
                slots.reserve(batches.BatchCount());
                for (size_t batch = 0; batch < batches.BatchCount(); ++batch)
                    slots.emplace_back(selection.groupOutputs_[group].size());
                {
                    auto futures =
                        BufferCapacityScope_::ForWorker(scratch, Script::Detail::ReplayExtentProduct(batches.BatchCount(), sizeof(TaskHandle_)));
                    Dal::Detail::BufferCapacitySuspension_ suspension;
                    Script::SimulationTaskGroup_ tasks(ThreadPool_::GetInstance(), batches.BatchCount());
                    for (size_t batch = 0; batch < batches.BatchCount(); ++batch)
                        tasks.Spawn([&, batch] {
                            auto capacity = BufferCapacityScope_::ForWorker(scratch);
                            slots[batch] = Script::Detail::EvaluatePortfolioWeightedBatch(portfolio, group, batches.BatchAt(batch),
                                                                                          selection.groupOutputs_[group], scratch, tape);
                            return true;
                        });
                    tasks.Complete();
                }
                ReduceGroup(slots, group, axes, selection, result);
            } catch (const std::exception& error) {
                THROW2("PortfolioWeightedReplayFailed: group=" + String_(std::to_string(group)) + "; cause=" + String_(error.what()), ScriptError_);
            }
        }

        Vector_<const Script::PreparedScript_*> SelectedPlans(const Vector_<Script::Detail::PlannedScript_>& plans,
                                                              const Script::Detail::PortfolioScenarioGroup_& group,
                                                              const Vector_<PortfolioBatchOutput_>& outputs) {
            Vector_<const Script::PreparedScript_*> trades;
            for (const auto trade : group.tradePositions_)
                if (std::any_of(outputs.begin(), outputs.end(), [&](const auto& output) { return output.tradePosition_ == trade; }))
                    trades.push_back(&plans[trade].View());
            return trades;
        }

        void AdmitGroup(const PortfolioWeightedPlan_& plan,
                        const Vector_<Script::Detail::PlannedScript_>& plans,
                        const Script::Detail::PortfolioScenarioGroup_& group,
                        const Vector_<PortfolioBatchOutput_>& outputs,
                        const Script::BatchPlan_& batches,
                        size_t workers,
                        BufferCapacityBudget_* scratch,
                        const Script::Detail::PortfolioCapacityLimits_& limits) {
            const auto trades = SelectedPlans(plans, group, outputs);
            const auto representative = group.tradePositions_.front();
            Vector_<PortfolioWeightedBatchResult_> slots;
            slots.reserve(batches.BatchCount());
            for (size_t batch = 0; batch < batches.BatchCount(); ++batch) {
                slots.emplace_back(outputs.size());
                slots.back().tradePositions_.Resize(trades.size());
                if (plan.EnableAad()) {
                    slots.back().modelGradientSums_.Resize(plans[representative].Model().Parameters().size());
                    slots.back().constantGradientSums_.Resize(trades.size());
                    for (size_t trade = 0; trade < trades.size(); ++trade)
                        slots.back().constantGradientSums_[trade].Resize(trades[trade]->ConstVarNames().size());
                }
            }
            const auto futures = Script::Detail::ReplayExtentProduct(batches.BatchCount(), sizeof(TaskHandle_));
            auto taskCapacity = BufferCapacityScope_::ForWorker(scratch, futures);
            const auto remaining = scratch->LimitBytes() - scratch->CapacityBytes();
            const auto tapeQuota = limits.tapeBudgetBytes_.value_or(std::numeric_limits<size_t>::max()) / workers;
            Dal::Detail::BufferCapacitySuspension_ suspension;
            if (plan.EnableAad())
                Script::Detail::AdmitPortfolioWeightedWorker(trades, plan.Portfolio()->Models()[group.modelOwner_], outputs, remaining / workers,
                                                             tapeQuota);
            else
                Script::Detail::AdmitPortfolioPassiveWorker(trades, plan.Portfolio()->Models()[group.modelOwner_], outputs, remaining / workers);
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
            const auto selection = SelectOutputs(groups, plans.size(), axes, outputs, inputs);
            const PortfolioWeightedReplayResult_ result(outputs.size(), inputs.size(), groups.size());
            for (size_t group = 0; group < groups.size(); ++group) {
                if (selection.groupOutputs_[group].empty())
                    continue;
                try {
                    AdmitGroup(plan, plans, groups[group], selection.groupOutputs_[group], batches, workers, &scratch, limits);
                } catch (const std::exception& error) {
                    THROW2("PortfolioWeightedPreflightFailed: group=" + String_(std::to_string(group)) +
                               "; trade=" + plan.Portfolio()->TradeIds()[groups[group].tradePositions_.front()] + "; cause=" + String_(error.what()),
                           ScriptError_);
                }
            }
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

    PortfolioWeightedReplayResult_ EvaluatePortfolioWeightedReplay(const PreparedPortfolio_& portfolio,
                                                                   const Vector_<PortfolioBatchOutput_>& requestedOutputs,
                                                                   const Vector_<size_t>& selectedInputs,
                                                                   Script::Detail::PortfolioCapacityLimits_ limits) {
        const auto threads = ThreadPool_::GetInstance()->NumThreads();
        const bool native = portfolio.Trades().front().Simulation().enableAad_;
        REQUIRE2(native || selectedInputs.empty(), "InvalidPortfolioReplay: passive execution cannot select risk inputs; field=inputs", ScriptError_);
        for (const auto& trade : portfolio.Trades())
            REQUIRE2(trade.Simulation().enableAad_ == native, "InvalidPortfolioReplay: inconsistent preparation modes", ScriptError_);
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
        const auto selection = SelectOutputs(portfolio.Groups(), portfolio.Trades().size(), axes, outputs, inputs);
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
} // namespace Dal::Detail

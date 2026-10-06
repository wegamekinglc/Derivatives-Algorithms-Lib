//
// Created by Codex on 2026/10/7.
//

#include <algorithm>
#include <cmath>

#include <dal-public/src/portfolioplaninternal.hpp>
#include <dal-public/src/portfolioreplayinternal.hpp>
#include <dal-public/src/riskvalueinternal.hpp>
#include <dal-public/src/value.hpp>
#include <dal/platform/platform.hpp>

namespace Dal {
    namespace {
        String_ InputFailureContext(const Detail::PortfolioWeightedPlan_& plan,
                                    const PortfolioRiskExecution_& execution,
                                    const Script::RiskCoordinate_& input) {
            const auto& complete = plan.Axes().InputAxis();
            const auto found = std::find_if(complete.begin(), complete.end(), [&](const auto& coordinate) { return coordinate.id_ == input.id_; });
            const auto position = static_cast<size_t>(found - complete.begin());
            for (const auto& output : plan.Outputs()) {
                const auto& positions = plan.Axes().TradeInputPositions()[output.tradePosition_];
                if (std::find(positions.begin(), positions.end(), position) == positions.end())
                    continue;
                const auto context = "; trade=" + plan.Portfolio()->TradeIds()[output.tradePosition_] + "; output=" + output.coordinate_.id_;
                for (size_t group = 0; group < execution.groups_.size(); ++group) {
                    const auto& trades = execution.groups_[group].tradePositions_;
                    if (std::find(trades.begin(), trades.end(), output.tradePosition_) != trades.end())
                        return context + "; group=" + String_(std::to_string(group));
                }
                return context;
            }
            return {};
        }

        PortfolioRiskProvenance_ CaptureProvenance(const Script::Detail::PreparedPortfolio_& prepared) {
            PortfolioRiskProvenance_ result;
            const bool native = prepared.Trades().front().Simulation().enableAad_;
            result.method_ = native ? "NativeAADWeightedPortfolio" : "PriceOnlyWeightedPortfolio";
            result.engine_ = native ? "native" : "passive";
            result.evaluationDate_ = *prepared.Valuation().evaluationDate_;
            result.tradeIds_ = prepared.Portfolio()->TradeIds();
            result.modelOwners_ = prepared.Portfolio()->ModelOwners();
            for (size_t trade = 0; trade < prepared.Trades().size(); ++trade) {
                const auto owner = prepared.Portfolio()->ModelOwners()[trade];
                result.trades_.push_back(Detail::CaptureScriptRiskProvenance(prepared.Trades()[trade], *prepared.Portfolio()->Products()[trade],
                                                                             *prepared.Portfolio()->Models()[owner], prepared.PathCount()));
                result.trades_.back().engine_ = result.engine_;
            }
            return result;
        }

        PortfolioRiskExecution_ CaptureExecution(const Script::Detail::PreparedPortfolio_& prepared,
                                                 const Detail::PortfolioWeightedReplayResult_& source) {
            PortfolioRiskExecution_ execution;
            execution.peakRecordingBytes_ = source.peakTapeBytes_;
            execution.peakScratchBytes_ = source.peakScratchBytes_;
            for (size_t group = 0; group < prepared.Groups().size(); ++group) {
                const auto& planned = prepared.Groups()[group];
                const auto& representative = prepared.Trades()[planned.tradePositions_.front()];
                const auto& counters = source.groupCounters_[group];
                execution.groups_.push_back({planned.modelOwner_, planned.tradePositions_, planned.randomDimension_, planned.factors_,
                                             planned.deterministicNumeraire_, representative.Plan().SampleDates(), representative.TimeLine(),
                                             representative.DefLine(), representative.Simulation(), counters.generatedScenarios_,
                                             counters.evaluatorCalls_, counters.suffixReversals_, counters.prefixReversals_});
            }
            return execution;
        }
    } // namespace

    PortfolioWeightedRiskResult_::PortfolioWeightedRiskResult_(const Detail::PortfolioWeightedPlan_& plan,
                                                               Detail::PortfolioWeightedReplayResult_&& source,
                                                               PortfolioRiskProvenance_ provenance,
                                                               PortfolioRiskExecution_ execution)
        : weightedValue_(source.weightedValue_), componentMeans_(std::move(source.componentMeans_)), gradient_(std::move(source.gradient_)),
          completeOutputAxis_(plan.Axes().OutputAxis()), inputAxis_(plan.InputAxis()), completeInputAxis_(plan.Axes().InputAxis()),
          provenance_(std::move(provenance)), execution_(std::move(execution)) {
        REQUIRE2(gradient_.size() == inputAxis_.size() && componentMeans_.size() == plan.Outputs().size(),
                 "InvalidPortfolioResult: numeric extents changed", ScriptError_);
        weights_.reserve(plan.Outputs().size());
        outputAxis_.reserve(plan.Outputs().size());
        for (const auto& output : plan.Outputs()) {
            outputAxis_.push_back(output.coordinate_);
            weights_.push_back(output.weight_);
        }
        for (size_t input = 0; input < gradient_.size(); ++input)
            REQUIRE2(std::isfinite(gradient_[input] * inputAxis_[input].reportScale_),
                     "InvalidPortfolioResult: non-finite report projection; input=" + inputAxis_[input].id_ +
                         InputFailureContext(plan, execution_, inputAxis_[input]),
                     ScriptError_);
    }

    Matrix_<> PortfolioWeightedRiskResult_::Jacobian() const {
        Matrix_<> result(1, static_cast<int>(gradient_.size()));
        for (size_t column = 0; column < gradient_.size(); ++column)
            result(0, static_cast<int>(column)) = gradient_[column];
        return result;
    }

    Matrix_<> PortfolioWeightedRiskResult_::ReportedJacobian() const {
        auto result = Jacobian();
        for (size_t column = 0; column < gradient_.size(); ++column)
            result(0, static_cast<int>(column)) *= inputAxis_[column].reportScale_;
        return result;
    }

    PortfolioWeightedRiskResult_ ValuePortfolioByMonteCarloWithWeightedRisk(const Handle_<Script::ScriptPortfolioData_>& portfolio,
                                                                            int numPath,
                                                                            const PortfolioWeightedRiskRequest_& requested,
                                                                            const ScriptValuationSettings_& valuation,
                                                                            const MonteCarloSettings_& simulation) {
        const auto request = requested;
        const auto execution = simulation;
        const auto settings = valuation;
        REQUIRE2(numPath > 0, "InvalidPortfolioPaths: path count must be positive; field=numPath", ScriptError_);
        const auto plan = Detail::PlanPortfolioWeightedRequest(portfolio, {request.selection_, request.weights_}, execution.enableAad_);
        const Script::Detail::PortfolioCapacityLimits_ limits{request.scratchCapacityBudgetBytes_, request.recordingCapacityBudgetBytes_};
        const auto prepared = Detail::PreparePortfolioWeightedReplay(plan, numPath, settings, execution, limits);
        auto source = Detail::EvaluatePortfolioWeightedReplay(prepared, plan.Outputs(), plan.InputPositions(), limits);
        auto provenance = CaptureProvenance(prepared);
        auto diagnostics = CaptureExecution(prepared, source);
        return {plan, std::move(source), std::move(provenance), std::move(diagnostics)};
    }
} // namespace Dal

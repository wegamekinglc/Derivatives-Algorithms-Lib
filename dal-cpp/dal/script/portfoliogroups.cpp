//
// Created by Codex on 2026/10/6.
//

#include <string_view>
#include <tuple>

#include <dal/platform/platform.hpp>
#include <dal/script/portfoliogroups.hpp>

namespace Dal::Script::Detail {
    namespace {
        bool SameSimulation(const MonteCarloSettings_& lhs, const MonteCarloSettings_& rhs) {
            return std::tie(lhs.rsg_, lhs.useBb_, lhs.enableAad_, lhs.smooth_, lhs.compiled_, lhs.lsmcBasisDegree_, lhs.lsmcTrainingPaths_,
                            lhs.lsmcValidationPaths_, lhs.lsmcRqmcReplicates_, lhs.lsmcTrainingSeed_, lhs.lsmcPricingSeed_, lhs.lsmcPolicyRiskMode_,
                            lhs.lsmcPolicyBumpRelative_) == std::tie(rhs.rsg_, rhs.useBb_, rhs.enableAad_, rhs.smooth_, rhs.compiled_,
                                                                     rhs.lsmcBasisDegree_, rhs.lsmcTrainingPaths_, rhs.lsmcValidationPaths_,
                                                                     rhs.lsmcRqmcReplicates_, rhs.lsmcTrainingSeed_, rhs.lsmcPricingSeed_,
                                                                     rhs.lsmcPolicyRiskMode_, rhs.lsmcPolicyBumpRelative_);
        }

        bool SameSlot(const std::optional<ModelObservation_>& lhs, const std::optional<ModelObservation_>& rhs) {
            return lhs.has_value() == rhs.has_value() && (!lhs || (lhs->sampleId_ == rhs->sampleId_ && lhs->outputId_ == rhs->outputId_));
        }

        bool SameObservation(const ObservationPlan_& lhs, size_t firstId, const ObservationPlan_& rhs, size_t secondId) {
            const auto& first = lhs.Requests()[firstId];
            const auto& second = rhs.Requests()[secondId];
            return first.historical_ == second.historical_ && SameSlot(first.modelSlot_, second.modelSlot_) &&
                   lhs.TryKnownValue(firstId) == rhs.TryKnownValue(secondId);
        }

        bool SameObservations(const ObservationPlan_& lhs, const ObservationPlan_& rhs) {
            if (lhs.Requests().size() != rhs.Requests().size())
                return false;
            for (size_t id = 0; id < lhs.Requests().size(); ++id) {
                const auto& request = lhs.Requests()[id];
                const auto found = std::find_if(rhs.Requests().begin(), rhs.Requests().end(), [&](const auto& other) {
                    return request.key_.canonicalIndex_ == other.key_.canonicalIndex_ && request.key_.fixingTime_ == other.key_.fixingTime_;
                });
                if (found == rhs.Requests().end())
                    return false;
                const size_t otherId = static_cast<size_t>(found - rhs.Requests().begin());
                if (!SameObservation(lhs, id, rhs, otherId))
                    return false;
            }
            return true;
        }

        bool SameSamplingGeometry(const ObservationPlan_& first, const ObservationPlan_& second) {
            if (std::tie(first.SampleDates(), first.TimeLine(), first.ModelBindingNames()) !=
                std::tie(second.SampleDates(), second.TimeLine(), second.ModelBindingNames()))
                return false;
            return first.DefLine().size() == second.DefLine().size() &&
                   std::equal(first.DefLine().begin(), first.DefLine().end(), second.DefLine().begin(), SamePortfolioSampleDefinition) &&
                   SameObservations(first, second);
        }

        bool SamePreparedContract(const PreparedScript_& first, const PreparedScript_& second) {
            return std::tie(first.EvaluationDate(), first.Settings().todayFixingPolicy_) ==
                       std::tie(second.EvaluationDate(), second.Settings().todayFixingPolicy_) &&
                   std::string_view(FixingSourceKind(first.Settings())) == std::string_view(FixingSourceKind(second.Settings())) &&
                   SameSimulation(first.Simulation(), second.Simulation()) && SameSamplingGeometry(first.Plan(), second.Plan());
        }

        bool SameContract(const PreparedPortfolioTradeView_& lhs, const PreparedPortfolioTradeView_& rhs) {
            return lhs.canShareScenario_ && rhs.canShareScenario_ &&
                   std::tie(lhs.modelOwner_, lhs.randomDimension_, lhs.factors_, lhs.deterministicNumeraire_) ==
                       std::tie(rhs.modelOwner_, rhs.randomDimension_, rhs.factors_, rhs.deterministicNumeraire_) &&
                   SamePreparedContract(*lhs.prepared_, *rhs.prepared_);
        }

        void ValidateView(const PreparedPortfolioTradeView_& trade, size_t position, bool executable) {
            const String_ context = "; tradePosition=" + String_(std::to_string(position));
            REQUIRE2(trade.prepared_, "InvalidPortfolioGrouping: prepared trade must not be null" + context, ScriptError_);
            REQUIRE2(!trade.prepared_->AllExpired() && !trade.prepared_->Product().ContainsExercise(),
                     "UnsupportedPortfolioRisk: exercise and fully expired trades are unsupported" + context, ScriptError_);
            if (executable)
                trade.prepared_->RequireExecutable();
        }
    } // namespace

    bool SamePortfolioSampleDefinition(const AAD::SampleDef_& lhs, const AAD::SampleDef_& rhs) {
        if (std::tie(lhs.numeraire_, lhs.indexNames_, lhs.discountMats_, lhs.forwardMats_) !=
                std::tie(rhs.numeraire_, rhs.indexNames_, rhs.discountMats_, rhs.forwardMats_) ||
            lhs.liborDefs_.size() != rhs.liborDefs_.size())
            return false;
        return std::equal(lhs.liborDefs_.begin(), lhs.liborDefs_.end(), rhs.liborDefs_.begin(),
                          [](const auto& a, const auto& b) { return std::tie(a.start_, a.end_, a.curve_) == std::tie(b.start_, b.end_, b.curve_); });
    }

    namespace {
        Vector_<PortfolioScenarioGroup_> GroupViews(const Vector_<PreparedPortfolioTradeView_>& trades, bool executable) {
            REQUIRE2(!trades.empty(), "InvalidPortfolioGrouping: prepared trades must be nonempty", ScriptError_);
            for (size_t position = 0; position < trades.size(); ++position)
                ValidateView(trades[position], position, executable);
            Vector_<PortfolioScenarioGroup_> groups;
            for (size_t position = 0; position < trades.size(); ++position) {
                const auto& trade = trades[position];
                const auto found = std::find_if(groups.begin(), groups.end(),
                                                [&](const auto& group) { return SameContract(trades[group.tradePositions_.front()], trade); });
                if (found == groups.end())
                    groups.push_back({trade.modelOwner_, Vector_<size_t>{position}});
                else
                    found->tradePositions_.push_back(position);
            }
            return groups;
        }
    } // namespace

    Vector_<PortfolioScenarioGroup_> GroupPreparedPortfolio(const Vector_<PreparedPortfolioTradeView_>& trades) { return GroupViews(trades, true); }

    Vector_<PortfolioScenarioGroup_> GroupPlannedPortfolio(const Vector_<PreparedPortfolioTradeView_>& trades) { return GroupViews(trades, false); }
} // namespace Dal::Script::Detail

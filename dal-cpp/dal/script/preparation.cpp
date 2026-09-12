//
// Created by Codex on 2026/9/13.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>

#include <dal/indice/detail/fixingobserver.hpp>
#include <dal/indice/detail/snapshoterror.hpp>
#include <dal/indice/index/equity.hpp>
#include <dal/indice/index/fx.hpp>
#include <dal/script/preparation.hpp>
#include <typeinfo>

namespace Dal::Script {
    namespace {
        String_ Context(const ObservationRequest_& request) {
            return "; index=" + request.key_.canonicalIndex_ + "; fixing=" + DateTime::ToString(request.key_.fixingTime_) + "; " +
                   request.uses_[0].source_.Describe();
        }

        bool IsHistorical(const Date_& fixingDate, const Date_& evaluationDate, const ScriptValuationSettings_& settings) {
            return fixingDate < evaluationDate ||
                   (fixingDate == evaluationDate && settings.todayFixingPolicy_ == TodayFixingPolicy_::Value_::REQUIREHISTORICAL);
        }

        void ValidateIndex(const NodeFix_& node, bool historical) {
            REQUIRE2(node.index_, "InvalidIndex: null parsed index; " + node.source_.Describe(), ScriptError_);
            if (historical) {
                const auto& type = typeid(*node.index_);
                REQUIRE2(type == typeid(Index::Equity_) || type == typeid(Index::Fx_),
                         "UnsupportedHistoricalIndex: " + node.literal_.raw_ + "; " + node.source_.Describe(), ScriptError_);
            }
        }

        class Collector_ {
            const Date_ evaluationDate_;
            const ScriptValuationSettings_ settings_;
            std::map<ObservationKey_, size_t> ids_;
            size_t nodeId_ = 0;

            void Add(const NodeFix_& node, const Date_& eventDate, const ObservationUse_& use) {
                const Date_ fixingDate = node.fixingDate_.value_or(eventDate);
                REQUIRE2(fixingDate <= eventDate, "LookAheadObservation: " + node.literal_.raw_ + "; " + node.source_.Describe(), ScriptError_);
                ValidateIndex(node, IsHistorical(fixingDate, evaluationDate_, settings_));
                const ObservationKey_ key{node.index_->Name(), DateTime_(fixingDate, 0.0)};
                auto inserted = ids_.emplace(key, requests_.size());
                if (inserted.second)
                    requests_.push_back({node.index_, key, {}, std::nullopt});
                requests_[inserted.first->second].uses_.push_back(use);
            }

            void Collect(const Node_& node, const Date_& date, size_t eventId, size_t statementId) {
                const size_t nodeId = nodeId_++;
                hasPayoff_ = hasPayoff_ || dynamic_cast<const NodePays_*>(&node);
                if (const auto* fix = dynamic_cast<const NodeFix_*>(&node))
                    Add(*fix, date, {fix->source_, eventId, statementId, nodeId});
                for (const auto& child : node.arguments_)
                    Collect(*child, date, eventId, statementId);
            }

        public:
            Vector_<ObservationRequest_> requests_;
            bool hasPayoff_ = false;
            Collector_(const Date_& evaluationDate, const ScriptValuationSettings_& settings)
                : evaluationDate_(evaluationDate), settings_(settings) {}

            void Collect(const ScriptProduct_& product) {
                for (size_t event = 0; event < product.Events().size(); ++event)
                    for (size_t statement = 0; statement < product.Events()[event].size(); ++statement)
                        Collect(*product.Events()[event][statement], product.EventDates()[event], event, statement);
            }
        };

        Vector_<FixingRequest_>
        HistoricalRequests(const Vector_<ObservationRequest_>& requests, const Date_& evaluationDate, const ScriptValuationSettings_& settings) {
            Vector_<FixingRequest_> result;
            for (const auto& request : requests)
                if (IsHistorical(request.key_.fixingTime_.Date(), evaluationDate, settings))
                    result.push_back({request.key_.canonicalIndex_, request.key_.fixingTime_});
            return result;
        }

        String_
        SnapshotFailureContext(const std::exception& error, const Vector_<ObservationRequest_>& requests, const Vector_<FixingRequest_>& historical) {
            const auto* failure = dynamic_cast<const Dal::Detail::SnapshotFixingError_*>(&error);
            String_ context;
            for (const auto& request : requests) {
                const auto dependencies = HistoricalFixingDependencies({{request.key_.canonicalIndex_, request.key_.fixingTime_}});
                for (const auto& dependency : dependencies) {
                    if (failure && SameFixingRequest(dependency, failure->Request()))
                        return Context(request);
                }
                if (request.key_.canonicalIndex_ == historical[0].indexName_ && request.key_.fixingTime_ == historical[0].fixingTime_)
                    context = Context(request);
            }
            return context;
        }

        double Resolve(const ObservationRequest_& request, const Environment_* environment) {
            double value;
            try {
                if (auto* observer = Dal::Detail::FixingReadObserver())
                    observer->BeforeFixing(*request.index_, environment, request.key_.fixingTime_);
                value = request.index_->Fixing(environment, request.key_.fixingTime_);
            } catch (const std::exception& error) {
                THROW2("MissingFixing: " + String_(error.what()) + Context(request), ScriptError_);
            }
            REQUIRE2(std::isfinite(value), "InvalidFixing: expected a finite fixing" + Context(request), ScriptError_);
            if (dynamic_cast<const Index::Fx_*>(request.index_.get()))
                REQUIRE2(value > 0.0, "InvalidFixing: expected a positive FX fixing" + Context(request), ScriptError_);
            return value;
        }

        Vector_<> ResolveHistory(Vector_<ObservationRequest_>* requests,
                                 const Date_& evaluationDate,
                                 const ScriptValuationSettings_& settings,
                                 const Handle_<MarketFixingSnapshot_>& explicitSnapshot) {
            const auto historical = HistoricalRequests(*requests, evaluationDate, settings);
            if (historical.empty())
                return {};
            Handle_<MarketFixingSnapshot_> snapshot = explicitSnapshot;
            if (!snapshot) {
                try {
                    snapshot = SnapshotGlobalFixings(historical);
                } catch (const std::exception& error) {
                    THROW2("InvalidFixingSnapshot: " + String_(error.what()) + SnapshotFailureContext(error, *requests, historical), ScriptError_);
                }
            }
            const auto environment = SnapshotFixingEnvironment(*snapshot, historical);
            Vector_<> values;
            for (auto& request : *requests) {
                if (!IsHistorical(request.key_.fixingTime_.Date(), evaluationDate, settings))
                    continue;
                request.historyValueId_ = values.size();
                values.push_back(Resolve(request, environment.get()));
            }
            return values;
        }
    } // namespace

    PreparedScript_
    PrepareScript(const ScriptProductData_& data, const ScriptValuationSettings_& settings, const Handle_<MarketFixingSnapshot_>& snapshot) {
        const Date_ evaluationDate = CaptureScriptEvaluationDate();
        REQUIRE2(settings.todayFixingPolicy_ == TodayFixingPolicy_::Value_::MODEL ||
                     settings.todayFixingPolicy_ == TodayFixingPolicy_::Value_::REQUIREHISTORICAL,
                 "InvalidTodayFixingPolicy", ScriptError_);
        auto product = std::make_unique<ScriptProduct_>(data.Product());
        REQUIRE2(!product->Events().empty(), "InvalidScriptStructure: script has no dated events", ScriptError_);
        Collector_ collector(evaluationDate, settings);
        collector.Collect(*product);
        REQUIRE2(collector.hasPayoff_, "InvalidScriptStructure: script has no PAYS payoff", ScriptError_);
        product->PartitionEvents(evaluationDate);
        product->IndexVariables();
        Vector_<> values;
        if (!product->EventDates().empty())
            values = ResolveHistory(&collector.requests_, evaluationDate, settings, snapshot);
        return PreparedScript_(std::move(product), evaluationDate, settings, ObservationPlan_(std::move(collector.requests_), std::move(values)));
    }
} // namespace Dal::Script

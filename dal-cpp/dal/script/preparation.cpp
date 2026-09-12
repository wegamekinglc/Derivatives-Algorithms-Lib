//
// Created by Codex on 2026/9/13.
//

#include <typeinfo>

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>

#include <dal/indice/detail/fixingobserver.hpp>
#include <dal/indice/detail/snapshoterror.hpp>
#include <dal/indice/index/equity.hpp>
#include <dal/indice/index/fx.hpp>
#include <dal/indice/indexparse.hpp>
#include <dal/script/preparation.hpp>

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
            Handle_<Index_> defaultIndex_;
            Vector_<Date_> unboundSpotDates_;

            void Add(NodeFix_& node, const Date_& eventDate, const ObservationUse_& use) {
                const Date_ fixingDate = node.fixingDate_.value_or(eventDate);
                REQUIRE2(fixingDate <= eventDate, "LookAheadObservation: " + node.literal_.raw_ + "; " + node.source_.Describe(), ScriptError_);
                ValidateIndex(node, IsHistorical(fixingDate, evaluationDate_, settings_));
                const ObservationKey_ key{node.index_->Name(), DateTime_(fixingDate, 0.0)};
                auto inserted = ids_.emplace(key, requests_.size());
                if (inserted.second)
                    requests_.push_back({node.index_, key, {}, std::nullopt});
                requests_[inserted.first->second].uses_.push_back(use);
                node.observationId_ = inserted.first->second;
            }

            void Collect(Node_& node, const Date_& date, size_t eventId, size_t statementId) {
                const size_t nodeId = nodeId_++;
                hasPayoff_ = hasPayoff_ || dynamic_cast<const NodePays_*>(&node);
                if (auto* fix = dynamic_cast<NodeFix_*>(&node))
                    Add(*fix, date, {fix->source_, eventId, statementId, nodeId});
                if (auto* spot = dynamic_cast<NodeSpot_*>(&node)) {
                    if (defaultIndex_) {
                        SourceLocation_ source;
                        source.eventDate_ = date;
                        NodeFix_ fix(IndexLiteral_{defaultIndex_->Name()}, defaultIndex_, date, source);
                        Add(fix, date, {source, eventId, statementId, nodeId});
                        spot->observationId_ = fix.observationId_;
                    } else {
                        unboundSpotDates_.push_back(date);
                    }
                }
                for (const auto& child : node.arguments_)
                    Collect(*child, date, eventId, statementId);
            }

        public:
            Vector_<ObservationRequest_> requests_;
            bool hasPayoff_ = false;
            Collector_(const Date_& evaluationDate, const ScriptValuationSettings_& settings, const ScriptProductSettings_& contract)
                : evaluationDate_(evaluationDate), settings_(settings) {
                if (!contract.defaultIndex_.empty()) {
                    defaultIndex_ = Handle_<Index_>(Index::Parse(contract.defaultIndex_));
                    REQUIRE2(defaultIndex_, "InvalidIndex: default index", ScriptError_);
                }
            }

            void Collect(const ScriptProduct_& product) {
                for (size_t event = 0; event < product.Events().size(); ++event)
                    for (size_t statement = 0; statement < product.Events()[event].size(); ++statement)
                        Collect(*product.Events()[event][statement], product.EventDates()[event], event, statement);
                for (const auto& date : unboundSpotDates_) {
                    REQUIRE2(!IsHistorical(date, evaluationDate_, settings_), "UnboundHistoricalSpot: SPOT() requires a default index", ScriptError_);
                    REQUIRE2(requests_.empty() && settings_.modelBindings_.empty(), "MissingDefaultIndex: SPOT() requires a default index",
                             ScriptError_);
                }
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

    class PreparedScriptBuilder_ {
        static String_ ValidateBindings(const ScriptValuationSettings_& settings) {
            String_ result;
            for (const auto& binding : settings.modelBindings_) {
                REQUIRE2(binding.assetName_ == "spot", "UnknownModelAsset: " + binding.assetName_, ScriptError_);
                REQUIRE2(result.empty(), "DuplicateModelBinding: spot", ScriptError_);
                const Handle_<Index_> index(Index::Parse(binding.indexName_));
                REQUIRE2(index, "InvalidIndex: model binding", ScriptError_);
                result = index->Name();
            }
            return result;
        }

        static void ModelPlan(ObservationPlan_* plan,
                              const ScriptProduct_& product,
                              const Date_& evaluationDate,
                              const ScriptValuationSettings_& settings,
                              const AAD::Model_<double>& model,
                              const String_& boundIndex) {
            for (const auto& binding : settings.modelBindings_) {
                const Handle_<Index_> index(Index::Parse(binding.indexName_));
                REQUIRE2(index && model.SupportsIndex(*index), "UnsupportedModelObservation: " + binding.indexName_, ScriptError_);
            }
            std::set<Date_> dates(product.EventDates().begin(), product.EventDates().end());
            for (const auto& request : plan->requests_) {
                if (IsHistorical(request.key_.fixingTime_.Date(), evaluationDate, settings))
                    continue;
                REQUIRE2(model.SupportsIndex(*request.index_), "UnsupportedModelObservation" + Context(request), ScriptError_);
                REQUIRE2(!boundIndex.empty(), "MissingModelBinding" + Context(request), ScriptError_);
                REQUIRE2(boundIndex == request.key_.canonicalIndex_, "ConflictingModelBinding" + Context(request), ScriptError_);
                dates.insert(request.key_.fixingTime_.Date());
            }
            for (const auto& date : dates) {
                plan->sampleDates_.push_back(date);
                plan->timeLine_.push_back((date - evaluationDate) / DAYS_PER_YEAR);
                AAD::SampleDef_ def;
                def.numeraire_ = false;
                plan->defLine_.push_back(def);
            }
            const auto sampleId = [&](const Date_& date) {
                return static_cast<size_t>(std::lower_bound(plan->sampleDates_.begin(), plan->sampleDates_.end(), date) - plan->sampleDates_.begin());
            };
            for (const auto& date : product.EventDates()) {
                const auto id = sampleId(date);
                plan->eventToSample_.push_back(id);
                plan->defLine_[id].numeraire_ = true;
            }
            for (auto& request : plan->requests_) {
                if (IsHistorical(request.key_.fixingTime_.Date(), evaluationDate, settings))
                    continue;
                const auto id = sampleId(request.key_.fixingTime_.Date());
                auto& outputs = plan->defLine_[id].indexNames_;
                request.modelSlot_ = ModelObservation_{id, outputs.size()};
                outputs.push_back(request.key_.canonicalIndex_);
            }
        }

    public:
        static PreparedScript_ Prepare(const ScriptProductData_& data,
                                       const ScriptValuationSettings_& settings,
                                       const Handle_<MarketFixingSnapshot_>& snapshot,
                                       AAD::Model_<double>* model,
                                       const MonteCarloSettings_& simulation,
                                       const ScriptProductSettings_& contract) {
            const Date_ evaluationDate = CaptureScriptEvaluationDate();
            REQUIRE2(settings.todayFixingPolicy_ == TodayFixingPolicy_::Value_::MODEL ||
                         settings.todayFixingPolicy_ == TodayFixingPolicy_::Value_::REQUIREHISTORICAL,
                     "InvalidTodayFixingPolicy", ScriptError_);
            auto product = std::make_unique<ScriptProduct_>(data.Product());
            REQUIRE2(!product->Events().empty(), "InvalidScriptStructure: script has no dated events", ScriptError_);
            Collector_ collector(evaluationDate, settings, contract);
            collector.Collect(*product);
            REQUIRE2(collector.hasPayoff_, "InvalidScriptStructure: script has no PAYS payoff", ScriptError_);
            product->PartitionEvents(evaluationDate);
            product->IndexVariables();
            const String_ boundIndex = ValidateBindings(settings);
            ValidateSimulationSettings(simulation);
            ObservationPlan_ plan(std::move(collector.requests_), {});
            auto* writable = product.get();
            PreparedScript_ result(std::move(product), evaluationDate, settings, std::move(plan));
            result.simulation_ = simulation;
            if (result.AllExpired())
                return result;
            if (model) {
                REQUIRE2(!simulation.enableAad_, "UnsupportedExecutionMode: prepared AAD evaluation", ScriptError_);
                REQUIRE2(!simulation.compiled_.value_or(false) || result.Plan().Requests().empty(),
                         "UnsupportedExecutionMode: named compiled evaluation", ScriptError_);
                ModelPlan(result.plan_.get(), result.Product(), evaluationDate, settings, *model, boundIndex);
                model->Allocate(result.TimeLine(), result.DefLine());
                model->Init(result.TimeLine(), result.DefLine());
            }
            result.plan_->knownValues_ = ResolveHistory(&result.plan_->requests_, evaluationDate, settings, snapshot);
            if (model) {
                if (result.Plan().Requests().empty())
                    writable->PreProcess(false, true);
                else
                    writable->InitializePastObservations(result.Plan());
                result.executable_ = true;
            }
            return result;
        }
    };

    PreparedScript_
    PrepareScript(const ScriptProductData_& data, const ScriptValuationSettings_& settings, const Handle_<MarketFixingSnapshot_>& snapshot) {
        return PreparedScriptBuilder_::Prepare(data, settings, snapshot, nullptr, {}, {});
    }

    PreparedScript_ PrepareScript(const ScriptProductData_& data,
                                  AAD::Model_<double>* model,
                                  const ScriptValuationSettings_& settings,
                                  const MonteCarloSettings_& simulation,
                                  const Handle_<MarketFixingSnapshot_>& snapshot,
                                  const ScriptProductSettings_& contract) {
        REQUIRE2(model, "InvalidModel: preparation requires a model", ScriptError_);
        return PreparedScriptBuilder_::Prepare(data, settings, snapshot, model, simulation, contract);
    }
} // namespace Dal::Script

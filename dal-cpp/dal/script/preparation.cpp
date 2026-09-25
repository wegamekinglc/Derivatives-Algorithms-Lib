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
#include <dal/script/detail/simulationobserver.hpp>
#include <dal/script/preparation.hpp>

namespace Dal::Script {
    namespace {
        ScriptProductSettings_ ResolveContract(const ScriptProductSettings_& product, const ScriptProductSettings_& legacy) {
            if (legacy.defaultIndex_.empty())
                return product;
            if (product.defaultIndex_.empty())
                return legacy;
            const auto original = ParseSettingIndex(product.defaultIndex_, "product.defaultIndex_");
            const auto overrideIndex = ParseSettingIndex(legacy.defaultIndex_, "contract.defaultIndex_");
            REQUIRE2(original->Name() == overrideIndex->Name(),
                     "InvalidSetting: product.defaultIndex_=" + product.defaultIndex_ + "; contract.defaultIndex_=" + legacy.defaultIndex_ +
                         "; expected the same canonical identity",
                     ScriptError_);
            return product;
        }

        String_ Context(const ObservationRequest_& request) {
            const auto& use = request.uses_[0];
            return ObservationErrorContext(use.indexOriginal_, request.key_.canonicalIndex_, request.key_.fixingTime_, use.source_, use.statementId_,
                                           use.nodeId_);
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
            String_ defaultOriginal_;
            Vector_<ObservationUse_> unboundSpots_;

            void Add(NodeFix_& node, const Date_& eventDate, const ObservationUse_& use) {
                const Date_ fixingDate = node.fixingDate_.value_or(eventDate);
                REQUIRE2(fixingDate <= eventDate,
                         LookAheadObservationError(node.literal_.raw_, node.index_->Name(), fixingDate, node.source_, use.statementId_, use.nodeId_),
                         ScriptError_);
                const bool historical = IsHistorical(fixingDate, evaluationDate_, settings_);
                ValidateIndex(node, historical);
                const ObservationKey_ key{node.index_->Name(), DateTime_(fixingDate, 0.0)};
                auto inserted = ids_.emplace(key, requests_.size());
                if (inserted.second)
                    requests_.push_back({node.index_, key, {}, std::nullopt, std::nullopt, historical});
                requests_[inserted.first->second].uses_.push_back(use);
                node.observationId_ = inserted.first->second;
            }

            void Collect(Node_& node, const Date_& date, size_t eventId, size_t statementId) {
                const size_t nodeId = nodeId_++;
                if (auto* fix = dynamic_cast<NodeFix_*>(&node))
                    Add(*fix, date, {fix->source_, eventId, statementId, nodeId, fix->literal_.raw_, fix->fixingDate_, false});
                if (auto* spot = dynamic_cast<NodeSpot_*>(&node)) {
                    if (defaultIndex_) {
                        NodeFix_ fix(IndexLiteral_{defaultOriginal_}, defaultIndex_, {}, spot->source_);
                        Add(fix, date, {spot->source_, eventId, statementId, nodeId, defaultOriginal_, {}, true});
                        spot->observationId_ = fix.observationId_;
                    } else {
                        unboundSpots_.push_back({spot->source_, eventId, statementId, nodeId, "SPOT()", {}, true});
                    }
                }
                for (const auto& child : node.arguments_)
                    Collect(*child, date, eventId, statementId);
            }

        public:
            Vector_<ObservationRequest_> requests_;
            Collector_(const Date_& evaluationDate, const ScriptValuationSettings_& settings, const ScriptProductSettings_& contract)
                : evaluationDate_(evaluationDate), settings_(settings), defaultOriginal_(contract.defaultIndex_) {
                if (!contract.defaultIndex_.empty()) {
                    defaultIndex_ = ParseSettingIndex(contract.defaultIndex_, "product.defaultIndex_");
                }
            }

            void Collect(const ScriptProduct_& product) {
                for (size_t event = 0; event < product.Events().size(); ++event)
                    for (size_t statement = 0; statement < product.Events()[event].size(); ++statement)
                        Collect(*product.Events()[event][statement], product.EventDates()[event], event, statement);
                for (const auto& use : unboundSpots_) {
                    const String_ context = ": SPOT(); product.defaultIndex_=empty; expected a default index; " + use.source_.Describe() +
                                            "; statement=" + String_(std::to_string(use.statementId_)) + "; node=n" +
                                            String_(std::to_string(use.nodeId_));
                    //  explicit branch, not a macro argument: keeps the guard unconditional
                    if (!use.source_.eventDate_)
                        THROW2("InvalidFixingDate: observation source has no event date" + context, ScriptError_);
                    REQUIRE2(!IsHistorical(*use.source_.eventDate_, evaluationDate_, settings_), "UnboundHistoricalSpot" + context, ScriptError_);
                    REQUIRE2(requests_.empty(), "MissingDefaultIndex" + context, ScriptError_);
                }
            }
        };

        Vector_<FixingRequest_> HistoricalRequests(const Vector_<ObservationRequest_>& requests) {
            Vector_<FixingRequest_> result;
            for (const auto& request : requests)
                if (request.historical_)
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

        double Resolve(const ObservationRequest_& request, const Environment_* environment, const ScriptValuationSettings_& settings) {
            double value;
            try {
                if (auto* observer = Dal::Detail::FixingReadObserver())
                    observer->BeforeFixing(*request.index_, environment, request.key_.fixingTime_);
                value = request.index_->Fixing(environment, request.key_.fixingTime_);
            } catch (const std::exception& error) {
                THROW2("MissingFixing: " + String_(error.what()) + Context(request) + "; source=" + FixingSourceKind(settings) +
                           "; exact historical fixing required; no model fallback",
                       ScriptError_);
            }
            REQUIRE2(std::isfinite(value), "InvalidFixing: expected a finite fixing" + Context(request), ScriptError_);
            if (dynamic_cast<const Index::Fx_*>(request.index_.get()))
                REQUIRE2(value > 0.0, "InvalidFixing: expected a positive FX fixing" + Context(request), ScriptError_);
            return value;
        }

        Vector_<> ResolveHistory(Vector_<ObservationRequest_>* requests,
                                 const ScriptValuationSettings_& settings,
                                 const Handle_<MarketFixingSnapshot_>& explicitSnapshot) {
            const auto historical = HistoricalRequests(*requests);
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
                if (!request.historical_)
                    continue;
                request.historyValueId_ = values.size();
                values.push_back(Resolve(request, environment.get(), settings));
            }
            return values;
        }

        //  First EXERCISE statement on events whose date satisfies `accept`, with the event date
        template <class F_>
        std::pair<const NodeExercise_*, Date_> FindExerciseIn(const Vector_<Event_>& events, const Vector_<Date_>& dates, const F_& accept) {
            for (size_t i = 0; i < events.size(); ++i) {
                if (!accept(dates[i]))
                    continue;
                for (const auto& statement : events[i])
                    if (const auto* exercise = dynamic_cast<const NodeExercise_*>(FindNode(
                            *statement, [](const Node_& visited) { return dynamic_cast<const NodeExercise_*>(&visited) != nullptr; })))
                        return {exercise, dates[i]};
            }
            return {nullptr, Date_()};
        }

        template <class F_> std::pair<const NodeExercise_*, Date_> FindExercise(const ScriptProduct_& product, const F_& accept) {
            const auto past = FindExerciseIn(product.PastEvents(), product.PastEventDates(), accept);
            return past.first ? past : FindExerciseIn(product.Events(), product.EventDates(), accept);
        }
    } // namespace

    class PreparedScriptBuilder_ {
        //  The model's spot output binds to the script's own model-observed index
        static Handle_<Index_> InferBinding(const Vector_<ObservationRequest_>& requests) {
            Handle_<Index_> result;
            for (const auto& request : requests) {
                if (request.historical_)
                    continue;
                if (!result) {
                    result = request.index_;
                    continue;
                }
                REQUIRE2(result->Name() == request.key_.canonicalIndex_,
                         "MultipleModelIndices: expected one model-observed EQ; first=" + result->Name() + Context(request),
                         ScriptError_);
            }
            return result;
        }

        static void BindModelObservations(ObservationPlan_* plan, const ScriptProduct_& product, const Date_& evaluationDate) {
            const auto sampleId = [&](const Date_& date) {
                return static_cast<size_t>(std::lower_bound(plan->sampleDates_.begin(), plan->sampleDates_.end(), date) - plan->sampleDates_.begin());
            };
            for (const auto& date : product.EventDates()) {
                const auto id = sampleId(date);
                plan->eventToSample_.push_back(id);
                plan->defLine_[id].numeraire_ = true;
            }
            for (size_t event = 0; event < product.ParsedEventDates().size(); ++event)
                if (product.ParsedEventDates()[event] >= evaluationDate)
                    plan->liveEventIds_.push_back(event);
            for (auto& request : plan->requests_) {
                if (request.historical_)
                    continue;
                const auto id = sampleId(request.key_.fixingTime_.Date());
                auto& outputs = plan->defLine_[id].indexNames_;
                request.modelSlot_ = ModelObservation_{id, outputs.size()};
                outputs.push_back(request.key_.canonicalIndex_);
            }
        }

        static void ModelPlan(ObservationPlan_* plan,
                              const ScriptProduct_& product,
                              const Date_& evaluationDate,
                              const AAD::Model_<double>& model) {
            std::set<Date_> dates(product.EventDates().begin(), product.EventDates().end());
            for (const auto& request : plan->requests_) {
                if (request.historical_)
                    continue;
                REQUIRE2(model.SupportsIndex(*request.index_),
                         "UnsupportedModelObservation: expected one plain EQ supported by the model" + Context(request), ScriptError_);
                dates.insert(request.key_.fixingTime_.Date());
            }
            for (const auto& date : dates) {
                plan->sampleDates_.push_back(date);
                plan->timeLine_.push_back((date - evaluationDate) / DAYS_PER_YEAR);
                AAD::SampleDef_ def;
                def.numeraire_ = false;
                plan->defLine_.push_back(def);
            }
            BindModelObservations(plan, product, evaluationDate);
        }

        static void MarkLiveObservations(const Node_& node, Vector_<char>* live) {
            const std::optional<size_t>* id = nullptr;
            if (const auto* spot = dynamic_cast<const NodeSpot_*>(&node))
                id = &spot->observationId_;
            else if (const auto* fix = dynamic_cast<const NodeFix_*>(&node))
                id = &fix->observationId_;
            if (id && *id) {
                REQUIRE2(**id < live->size(), "ObservationIdOutOfRange", ScriptError_);
                (*live)[**id] = 1;
            }
            for (const auto& child : node.arguments_)
                MarkLiveObservations(*child, live);
        }

        static Vector_<char> LiveModelObservations(const ObservationPlan_& plan, const ScriptProduct_& product) {
            Vector_<char> live(plan.requests_.size(), 0);
            for (const auto& event : product.Events())
                for (const auto& statement : event)
                    MarkLiveObservations(*statement, &live);
            return live;
        }

        static bool HasDeadModelObservations(const ObservationPlan_& plan, const Vector_<char>& live) {
            for (size_t id = 0; id < live.size(); ++id)
                if (plan.requests_[id].modelSlot_ && !live[id])
                    return true;
            return false;
        }

        static void CompactModelOutputs(ObservationPlan_* plan, const Vector_<char>& live) {
            for (auto& def : plan->defLine_)
                def.indexNames_.clear();
            for (size_t id = 0; id < live.size(); ++id) {
                auto& request = plan->requests_[id];
                if (!request.modelSlot_)
                    continue;
                if (!live[id]) {
                    request.modelSlot_.reset();
                    continue;
                }
                auto& outputs = plan->defLine_[request.modelSlot_->sampleId_].indexNames_;
                request.modelSlot_->outputId_ = outputs.size();
                outputs.push_back(request.key_.canonicalIndex_);
            }
        }

        //  Keep the timeline and all event/sample IDs intact: removing dates
        //  would change Sobol dimensions and path-dependent model evolution.
        //  Historical requests were resolved before this pass.
        static bool PruneDeadModelObservations(ObservationPlan_* plan, const ScriptProduct_& product) {
            const auto live = LiveModelObservations(*plan, product);
            if (!HasDeadModelObservations(*plan, live))
                return false;
            CompactModelOutputs(plan, live);
            return true;
        }

    public:
        static PreparedScript_ Prepare(const ScriptProductData_& data,
                                       const ScriptValuationSettings_& valuation,
                                       const Handle_<MarketFixingSnapshot_>& snapshot,
                                       AAD::Model_<double>* model,
                                       const MonteCarloSettings_& requestedSimulation,
                                       const ScriptProductSettings_& legacyContract) {
            //  Snapshot: observer callbacks during history resolution can mutate the caller's settings object
            const auto simulation = requestedSimulation;
            const auto settings = ResolveValuationSettings(valuation, snapshot);
            const Date_ evaluationDate = *settings.evaluationDate_;
            const auto contract = ResolveContract(data.Settings(), legacyContract);
            auto product = std::make_unique<ScriptProduct_>(data.Product());
            REQUIRE2(!product->Events().empty(), "InvalidScriptStructure: script has no dated events", ScriptError_);
            Collector_ collector(evaluationDate, settings, contract);
            collector.Collect(*product);
            REQUIRE2(product->HasPayoff(), "InvalidScriptStructure: dates/events has no PAYS payoff", ScriptError_);
            product->PartitionEvents(evaluationDate);
            ValidateSimulationSettings(simulation);
            //  Early-exercise gates: exercise dates must be strictly future (S1/S8) and only the
            //  sobol engine seeks each batch's normal paths exactly (S15); history-only
            //  preparation cannot value EXERCISE (S12)
            const auto expired = FindExercise(*product, [&](const Date_& date) { return date <= evaluationDate; });
            REQUIRE2(!expired.first,
                     "UnsupportedExerciseDate: event=" + Date::ToString(expired.second) +
                         "; expected an exercise date strictly after the evaluation date " + Date::ToString(evaluationDate) + "; " +
                         expired.first->source_.Describe(),
                     ScriptError_);
            const auto exercise = FindExercise(*product, [](const Date_&) { return true; });
            if (exercise.first) {
                REQUIRE2(simulation.rsg_ == "sobol",
                         "UnsupportedRsgForExercise: rsg=" + simulation.rsg_ + "; use method='sobol'; " + exercise.first->source_.Describe(),
                         ScriptError_);
                REQUIRE2(model, "UnsupportedExecutionMode: history-only preparation cannot value EXERCISE; " + exercise.first->source_.Describe(),
                         ScriptError_);
            }
            product->IndexVariables();
            if (simulation.enableAad_)
                product->ValidateFuzzyVectorMutations();
            const auto boundIndex = InferBinding(collector.requests_);
            ObservationPlan_ plan(std::move(collector.requests_), {});
            if (boundIndex)
                plan.modelBindingNames_.push_back(boundIndex->Name());
            auto* writable = product.get();
            PreparedScript_ result(std::move(product), evaluationDate, settings, std::move(plan));
            result.simulation_ = simulation;
            if (result.AllExpired())
                return result;
            if (model) {
                ModelPlan(result.plan_.get(), result.Product(), evaluationDate, *model);
                model->Allocate(result.TimeLine(), result.DefLine());
                model->Init(result.TimeLine(), result.DefLine());
            }
            result.plan_->knownValues_ = ResolveHistory(&result.plan_->requests_, settings, settings.fixings_);
            if (model) {
                writable->InitializePastObservations(result.Plan());
                ConstProcessor_ constants(writable->VarNames().size(), result.plan_.get(), true);
                writable->Visit(constants, true, false);
                // Retain parsed branches and continuous fuzzy kernels without tolerance-domain arithmetic.
                result.maxNestedIfs_ = writable->IFProcess();
                constants.StartFuture();
                writable->Visit(constants, false, true);
                if (writable->ContainsExercise()) {
                    writable->OptimizeLsmc();
                    result.maxNestedIfs_ = writable->IFProcess();
                    if (PruneDeadModelObservations(result.plan_.get(), *writable)) {
                        model->Allocate(result.TimeLine(), result.DefLine());
                        model->Init(result.TimeLine(), result.DefLine());
                    }
                }
                if (simulation.compiled_.value_or(false)) {
                    if (auto* observer = Detail::SimulationObserver())
                        observer->BeforeCompilation();
                    result.pastCompiled_ = ScriptCompiled_::Build(writable->PastEvents(), false, result.plan_, true);
                    result.compiled_ = ScriptCompiled_::Build(writable->Events(), simulation.enableAad_, result.plan_, false, writable->ContainsExercise());
                }
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

#include <dal/platform/platform.hpp>

#include <dal/calibration/validation_internal.hpp>
#include <dal/indice/index/fx.hpp>
#include <dal/indice/index/ir.hpp>
#include <dal/script/event.hpp>
#include <dal/script/xccyobservations.hpp>

namespace Dal::Script {
#include <dal/auto/MG_XccyObservationKind_enum.inc>
    namespace XccyScriptDetail {
        using namespace CalibrationInternal;
        struct Plan_ {
            struct Statement_ {
                const Node_* node_;
                Date_ date_;
                size_t sample_;
                Vector_<int> nodes_;
                Vector_<> constants_;
            };
            PreparedCalibrationInstrument_ admitted_;
            Handle_<XccyGaussianModelData_> model_;
            std::unique_ptr<const ScriptProduct_> product_;
            std::shared_ptr<const ObservationPlan_> observations_;
            Vector_<XccyObservationRequest_> requests_;
            Vector_<Date_> dates_;
            Vector_<AAD::SampleDef_> definitions_;
            Vector_<Statement_> statements_;
            String_ identity_;
            explicit Plan_(PreparedCalibrationInstrument_ admitted) : admitted_(std::move(admitted)) {}
        };
        namespace {
            void ExecutionContext(const Handle_<CalibrationInstrument_>& instrument, const CalibrationPreparationContext_& context) {
                Check(static_cast<bool>(instrument), "InvalidCalibrationPlan", "/instrument", "nonnull immutable instrument");
                CalibrationDiagnostic_ d;
                d.stage_ = CalibrationStage_::Value_::Prepare;
                d.instrumentId_ = instrument->Id();
                d.measureId_ = "Price";
                Check(context.xccy_ != nullptr, "CalibrationContextMismatch", "/context", "dedicated XCCY timestamp context", "", d);
                const auto& time = context.xccy_->valuationTime_;
                Check(time.IsValid() && time.Frac() == 0., "UnsupportedCalibrationFixingTime", "/context/valuation_time", "exact midnight valuation",
                      time.IsValid() ? DateTime::ToString(time) : String_("invalid"), d);
                Check(context.conventionState_ == instrument->Definition()->Spec().header_.conventionSet_, "CalibrationContextMismatch",
                      "/context/conventions", "frozen source convention state", context.conventionState_, d);
            }
            CalibrationDiagnostic_ Diagnostic(const Plan_& plan) {
                CalibrationDiagnostic_ d;
                d.stage_ = CalibrationStage_::Value_::Prepare;
                d.instrumentId_ = plan.admitted_.Instrument()->Id();
                d.measureId_ = "Price";
                return d;
            }
            const CalibrationXccyDescriptor_& Descriptor(const Plan_& plan) {
                const auto* descriptor = std::get_if<CalibrationXccy_>(&plan.admitted_.Descriptor());
                Check(descriptor && *descriptor, "UnsupportedCalibrationProduct", "/instrument", "admitted complete XCCY source", "",
                      Diagnostic(plan));
                return **descriptor;
            }
            void ModelContext(const Plan_& plan, const CalibrationPreparationContext_& context) {
                const auto d = Diagnostic(plan);
                Check(plan.model_ && context.xccy_, "CalibrationContextMismatch", "/model", "explicit XCCY model and timestamp context", "", d);
                const auto& market = *plan.model_->market_;
                const auto& xccy = *context.xccy_;
                Check(market.valuationTime_ == xccy.valuationTime_ && market.pair_ == xccy.pair_ &&
                          market.collateralCurrency_ == xccy.collateralCurrency_ && market.discountRoute_ == xccy.discountRoute_,
                      "CalibrationContextMismatch", "/model/market", "same valuation, pair, domestic collateral and discount route", "", d);
            }
            const XccyCouponPeriod_& Period(const Plan_& plan, const ObservationKey_& key, const String_& currency) {
                const auto& descriptor = Descriptor(plan);
                const auto& cashflows = descriptor.plan_;
                const auto& periods = currency == cashflows.config_.pair_.domestic_.String() ? cashflows.domesticPeriods_ : cashflows.foreignPeriods_;
                for (const auto& period : periods)
                    if (period.rateIndexName_ == key.canonicalIndex_ && period.rateFixingTime_ == key.fixingTime_)
                        return period;
                Fail("InvalidCalibrationPlan", "/observations", "source-derived canonical coupon geometry", key.canonicalIndex_, Diagnostic(plan));
            }
            XccyObservationRequest_ RateRequest(const Plan_& plan, const ObservationRequest_& request, const Index::Libor_& rate) {
                const String_ currency = rate.ccy_.String();
                const auto& period = Period(plan, request.key_, currency);
                const String_ tenor = rate.tenor_ == TradedRate_::Value_::LIBOR_3M_LCH ? "3M" : "6M";
                return {XccyObservationKind_::Value_::RateFixing,
                        currency,
                        tenor,
                        tenor,
                        period.schedule_.accrualStart_,
                        period.schedule_.accrualEnd_,
                        period.accrual_.dcf_};
            }
            class Collector_ {
                Plan_* plan_;
                std::map<std::tuple<String_, String_, DateTime_>, size_t> ids_;
                size_t nodeId_ = 0;

                void Resolve(ObservationRequest_* request, bool payment) {
                    const auto& descriptor = Descriptor(*plan_);
                    if (payment || request->key_.fixingTime_ > descriptor.valuationTime_)
                        return;
                    const auto value = descriptor.fixings_->Find(request->key_.canonicalIndex_, request->key_.fixingTime_);
                    Check(value || request->key_.fixingTime_ == descriptor.valuationTime_, "MissingFixing", "/context/fixings",
                          "exact strict-past fixing", request->key_.canonicalIndex_, Diagnostic(*plan_));
                    if (value) {
                        request->historyValueId_ = known_.size();
                        request->historical_ = true;
                        known_.push_back(*value);
                    }
                }
                size_t Add(const Handle_<Index_>& index, const DateTime_& time, const ObservationUse_& use, XccyObservationKind_ kind) {
                    const auto inserted = ids_.emplace(std::make_tuple(kind.String(), index->Name(), time), requests_.size());
                    if (inserted.second) {
                        ObservationRequest_ request{index, {index->Name(), time}, {}, {}, {}, false};
                        Resolve(&request, kind == XccyObservationKind_::Value_::PaymentFxSpot);
                        XccyObservationRequest_ detail{kind, plan_->model_->market_->pair_.foreign_.String(), "", "FX", {}, {}, 0.};
                        if (const auto* rate = dynamic_cast<const Index::Libor_*>(index.get()))
                            detail = RateRequest(*plan_, request, *rate);
                        plan_->requests_.push_back(std::move(detail));
                        requests_.push_back(std::move(request));
                    }
                    requests_[inserted.first->second].uses_.push_back(use);
                    return inserted.first->second;
                }

            public:
                Vector_<ObservationRequest_> requests_;
                Vector_<> known_;
                explicit Collector_(Plan_* plan) : plan_(plan) {}
                void Visit(Node_* node, const Date_& date, size_t event, size_t statement) {
                    const size_t nodeId = nodeId_++;
                    if (auto* fix = dynamic_cast<NodeFix_*>(node)) {
                        const auto kind = dynamic_cast<const Index::Libor_*>(fix->index_.get()) ? XccyObservationKind_::Value_::RateFixing
                                                                                                : XccyObservationKind_::Value_::ResetFxFixing;
                        fix->observationId_ = Add(fix->index_, DateTime_(*fix->fixingDate_),
                                                  {fix->source_, event, statement, nodeId, fix->literal_.raw_, fix->fixingDate_, false}, kind);
                    }
                    if (auto* spot = dynamic_cast<NodeSpot_*>(node)) {
                        const Handle_<Index_> fx(new Index::Fx_(plan_->model_->market_->pair_.domestic_, plan_->model_->market_->pair_.foreign_));
                        spot->observationId_ = Add(fx, DateTime_(date), {spot->source_, event, statement, nodeId, "SPOT()", {}, true},
                                                   XccyObservationKind_::Value_::PaymentFxSpot);
                    }
                    for (const auto& child : node->arguments_)
                        Visit(child.get(), date, event, statement);
                }
            };
            void Coverage(const Plan_& plan, const ObservationRequest_& request, const XccyObservationRequest_& detail) {
                if (request.historyValueId_ || detail.kind_ != XccyObservationKind_::Value_::RateFixing)
                    return;
                const auto& projection = plan.model_->market_->Projection(detail.currency_, detail.tenor_);
                const auto& market = *plan.model_->market_;
                const auto& discount = detail.currency_ == market.pair_.domestic_.String() ? market.domestic_ : market.foreign_;
                for (const auto& date : {request.key_.fixingTime_.Date(), detail.start_, detail.end_}) {
                    (void)projection.LogDF(date);
                    (void)discount->LogDF(date);
                    if (detail.currency_ == market.pair_.foreign_.String())
                        (void)market.basis_->LogDF(date);
                }
                Check(detail.end_ <= plan.model_->dynamics_->bucketEndDates_.back(), "CalibrationContextMismatch", "/model/dynamics/coverage",
                      "dynamics covering requested rate bond maturities", "", Diagnostic(plan));
            }
            void BindSamples(Plan_* plan, Collector_* collector, const ScriptProduct_& product) {
                std::set<Date_> dates;
                for (size_t event = 0; event < product.Events().size(); ++event)
                    for (const auto& node : product.Events()[event])
                        if (dynamic_cast<const NodePays_*>(node.get()))
                            dates.insert(product.EventDates()[event]);
                for (size_t id = 0; id < collector->requests_.size(); ++id) {
                    const auto& request = collector->requests_[id];
                    Coverage(*plan, request, plan->requests_[id]);
                    if (!request.historyValueId_)
                        dates.insert(request.key_.fixingTime_.Date());
                }
                auto model = NewXccyGaussianModel(*plan->model_);
                model->Prepare(Vector_<Date_>(dates.begin(), dates.end()));
                plan->dates_ = model->Grid();
                plan->definitions_.Resize(plan->dates_.size());
                for (auto& request : collector->requests_) {
                    if (request.historyValueId_)
                        continue;
                    const size_t sample = static_cast<size_t>(
                        std::lower_bound(plan->dates_.begin(), plan->dates_.end(), request.key_.fixingTime_.Date()) - plan->dates_.begin());
                    auto& names = plan->definitions_[sample].indexNames_;
                    request.modelSlot_ = ModelObservation_{sample, names.size()};
                    names.push_back(request.key_.canonicalIndex_);
                }
            }
            void CompileStatements(Plan_* plan, const ScriptProduct_& product) {
                for (size_t event = 0; event < product.Events().size(); ++event) {
                    const auto date = product.EventDates()[event];
                    for (const auto& node : product.Events()[event]) {
                        // Admission allows only leading literal-zero receiver assignments alongside PAYS.
                        if (!dynamic_cast<const NodePays_*>(node.get()))
                            continue;
                        const size_t sample =
                            static_cast<size_t>(std::lower_bound(plan->dates_.begin(), plan->dates_.end(), date) - plan->dates_.begin());
                        Compiler_ compiler(false, plan->observations_.get());
                        node->Accept(compiler);
                        plan->statements_.push_back({node.get(), date, sample, compiler.NodeStream(), compiler.ConstStream()});
                    }
                }
            }
            std::shared_ptr<const Plan_> Build(const Handle_<CalibrationInstrument_>& instrument,
                                               const CalibrationPreparationContext_& context,
                                               const Handle_<XccyGaussianModelData_>& model) {
                ExecutionContext(instrument, context);
                auto plan = std::make_shared<Plan_>(PrepareCalibrationInstrument(instrument, "Price", context));
                plan->model_ = model;
                ModelContext(*plan, context);
                auto product = instrument->Product();
                product->PartitionEvents(context.xccy_->valuationTime_.Date());
                product->IndexVariables();
                Collector_ collector(plan.get());
                for (size_t event = 0; event < product->Events().size(); ++event)
                    for (size_t statement = 0; statement < product->Events()[event].size(); ++statement)
                        collector.Visit(product->Events()[event][statement].get(), product->EventDates()[event], event, statement);
                BindSamples(plan.get(), &collector, *product);
                plan->observations_ = std::make_shared<const ObservationPlan_>(std::move(collector.requests_), std::move(collector.known_));
                CompileStatements(plan.get(), *product);
                plan->product_ = std::move(product);
                plan->identity_ = Fingerprint(Obj({{"source_plan", Str(plan->admitted_.PlanIdentity())},
                                                   {"model", Str(model->Identity())},
                                                   {"execution", Str("dal.xccy-observation-execution/1")}}));
                return plan;
            }
            template <class T_, class E_, class F_> Vector_<XccyScriptPayment_<T_>> Payments(const Plan_& plan, E_* evaluator, const F_& evaluate) {
                evaluator->Init();
                Vector_<XccyScriptPayment_<T_>> result;
                const auto payoff = plan.product_->PayOffIdx();
                for (const auto& statement : plan.statements_) {
                    const T_ before = evaluator->VarVals()[payoff];
                    evaluate(statement);
                    if (dynamic_cast<const NodePays_*>(statement.node_)) {
                        const T_ amount = evaluator->VarVals()[payoff] - before;
                        Check(std::isfinite(static_cast<double>(amount)), "CalibrationNumericalFailure", "/execution/payment",
                              "finite discounted source cashflow", "", Diagnostic(plan));
                        result.push_back({statement.date_, amount});
                    }
                }
                return result;
            }
            template <class T_> Vector_<XccyScriptPayment_<T_>> Evaluate(const Plan_& plan, const AAD::Scenario_<T_>& samples, bool compiled) {
                const auto& product = *plan.product_;
                const Vector_<> seed(product.VarNames().size(), 0.);
                const auto constants = Apply([](double value) { return T_(value); }, product.ConstVarValues());
                if (compiled) {
                    EvalState_<T_> evaluator(seed, constants, 0, 0., product.VectorCapacities());
                    evaluator.observations_ = plan.observations_.get();
                    evaluator.scenario_ = &samples;
                    return Payments<T_>(plan, &evaluator, [&](const auto& statement) {
                        Detail::EvalCompiledEvents<true>(
                            1,
                            [&](size_t) {
                                return Detail::CompiledEventView_<T_>{statement.nodes_, statement.constants_, samples[statement.sample_]};
                            },
                            &evaluator);
                    });
                }
                Evaluator_<T_> evaluator(seed, constants, product.VectorCapacities());
                evaluator.SetObservations(plan.observations_.get());
                evaluator.SetScenario(&samples);
                return Payments<T_>(plan, &evaluator, [&](const auto& statement) {
                    evaluator.SetCurEvt(statement.sample_);
                    statement.node_->Accept(evaluator);
                });
            }
            template <class T_> void ValidateStates(const Vector_<Date_>& grid, const Vector_<AAD::XccyGaussianState_<T_>>& states) {
                Check(states.size() == grid.size(), "InvalidCalibrationPlan", "/execution/states", "one state per exact prepared grid date");
                for (size_t i = 0; i < states.size(); ++i) {
                    Check(states[i].date_ == grid[i], "InvalidCalibrationPlan", "/execution/states/date", "exact prepared grid date",
                          Date::ToString(states[i].date_));
                    for (const auto& value : states[i].values_)
                        Check(std::isfinite(static_cast<double>(value)) && (i != 0 || static_cast<double>(value) == 0.), "InvalidCalibrationPlan",
                              "/execution/states/value", "finite state with zero initial factors");
                }
            }
            template <class T_>
            T_ Observation(const XccyObservationRequest_& request, const AAD::XccyGaussian_<T_>& model, const AAD::XccyGaussianState_<T_>& state) {
                if (request.kind_ == XccyObservationKind_::Value_::RateFixing)
                    return model.ForwardRate(request.currency_, request.tenor_, state, request.start_, request.end_, request.accrual_);
                return model.Spot(state);
            }
            String_ EvaluationIdentity(const PreparedXccyScript_& prepared, const Handle_<XccyGaussianModelData_>& data) {
                return Fingerprint(Obj({{"plan", Str(prepared.PlanIdentity())}, {"candidate", Str(data->Identity())}}));
            }
            [[noreturn]] void OwnFailure(const Handle_<CalibrationInstrument_>& instrument,
                                         const Handle_<XccyGaussianModelData_>& data,
                                         const CalibrationError_& error,
                                         CalibrationStage_ stage) {
                auto d = error.Failure().diagnostic_;
                d.stage_ = stage;
                if (instrument)
                    d.instrumentId_ = instrument->Id();
                d.measureId_ = "Price";
                const Handle_<CalibrationReport_> report(new CalibrationReport_(stage, {}, {d}));
                throw CalibrationError_(__FILE__, __LINE__, __func__,
                                        std::make_shared<const CalibrationFailure_>(CalibrationFailure_{d, report, Handle_<Storable_>(data)}));
            }
            Handle_<XccyGaussianModelData_> Candidate(const PreparedXccyScript_& prepared, const Handle_<XccyGaussianModelData_>& candidate) try {
                const auto data = candidate ? candidate : prepared.ModelData();
                prepared.ModelData()->RequireSameGeometry(*data);
                return data;
            } catch (const CalibrationError_& error) {
                OwnFailure(prepared.Instrument(), {}, error, CalibrationStage_::Value_::Prepare);
            }
            std::shared_ptr<const Plan_> CheckedBuild(const Handle_<CalibrationInstrument_>& instrument,
                                                      const CalibrationPreparationContext_& context,
                                                      const Handle_<XccyGaussianModelData_>& model) try {
                return Build(instrument, context, model);
            } catch (const CalibrationError_& error) {
                OwnFailure(instrument, {}, error, CalibrationStage_::Value_::Prepare);
            }
        } // namespace
    } // namespace XccyScriptDetail

    PreparedXccyScript_::PreparedXccyScript_(const Handle_<CalibrationInstrument_>& instrument,
                                             const CalibrationPreparationContext_& context,
                                             const Handle_<XccyGaussianModelData_>& model)
        : plan_(XccyScriptDetail::CheckedBuild(instrument, context, model)) {}
    const Vector_<Date_>& PreparedXccyScript_::SampleDates() const { return plan_->dates_; }
    const Vector_<AAD::SampleDef_>& PreparedXccyScript_::DefLine() const { return plan_->definitions_; }
    const Vector_<XccyObservationRequest_>& PreparedXccyScript_::Requests() const { return plan_->requests_; }
    const ObservationPlan_& PreparedXccyScript_::Observations() const { return *plan_->observations_; }
    const String_& PreparedXccyScript_::PlanIdentity() const { return plan_->identity_; }
    const Handle_<XccyGaussianModelData_>& PreparedXccyScript_::ModelData() const { return plan_->model_; }
    const Handle_<CalibrationInstrument_>& PreparedXccyScript_::Instrument() const { return plan_->admitted_.Instrument(); }
    void PreparedXccyScript_::RequireCompatible(const Handle_<CalibrationInstrument_>& instrument,
                                                const CalibrationPreparationContext_& context) const {
        XccyScriptDetail::ExecutionContext(instrument, context);
        plan_->admitted_.RequireCompatible(instrument, context);
    }
    template <class T_>
    Vector_<XccyScriptPayment_<T_>> PreparedXccyScript_::EvaluatePayments(const XccyScriptPath_<T_>& path, bool compiled) const try {
        CalibrationInternal::Check(path.planIdentity_ == PlanIdentity(), "InvalidCalibrationPlan", "/execution/plan",
                                   "path of this sealed source plan");
        return XccyScriptDetail::Evaluate(*plan_, path.samples_, compiled);
    } catch (const CalibrationError_& error) {
        XccyScriptDetail::OwnFailure(Instrument(), path.data_, error, CalibrationStage_::Value_::Evaluate);
    }
    template <class T_>
    XccyScriptPathProvider_<T_>::XccyScriptPathProvider_(const PreparedXccyScript_& prepared, const Handle_<XccyGaussianModelData_>& candidate)
        : prepared_(prepared), data_(XccyScriptDetail::Candidate(prepared, candidate)),
          evaluationIdentity_(XccyScriptDetail::EvaluationIdentity(prepared, data_)), model_(NewXccyGaussianModel<T_>(*data_)) {
        try {
            model_->Prepare(prepared.SampleDates());
        } catch (const CalibrationError_& error) {
            XccyScriptDetail::OwnFailure(prepared.Instrument(), data_, error, CalibrationStage_::Value_::Prepare);
        }
    }
    template <class T_> XccyScriptPath_<T_> XccyScriptPathProvider_<T_>::Observe(const Vector_<AAD::XccyGaussianState_<T_>>& states) const try {
        XccyScriptDetail::ValidateStates(model_->Grid(), states);
        AAD::Scenario_<T_> samples;
        AAD::AllocatePath(prepared_.DefLine(), samples);
        for (size_t sample = 0; sample < states.size(); ++sample) {
            samples[sample].numeraire_ = model_->DomesticNumeraire(states[sample]);
            samples[sample].spot_ = model_->Spot(states[sample]);
        }
        const auto& requests = prepared_.Observations().Requests();
        for (size_t id = 0; id < requests.size(); ++id) {
            if (!requests[id].modelSlot_)
                continue;
            const auto slot = *requests[id].modelSlot_;
            samples[slot.sampleId_].observations_[slot.outputId_] =
                XccyScriptDetail::Observation(prepared_.Requests()[id], *model_, states[slot.sampleId_]);
        }
        return XccyScriptPath_<T_>(std::move(samples), prepared_.PlanIdentity(), evaluationIdentity_, data_);
    } catch (const CalibrationError_& error) {
        XccyScriptDetail::OwnFailure(prepared_.Instrument(), data_, error, CalibrationStage_::Value_::Evaluate);
    }
    template <class T_> XccyScriptPath_<T_> XccyScriptPathProvider_<T_>::GeneratePath(const Vector_<>& normals) const try {
        return Observe(model_->GenerateStates(normals));
    } catch (const CalibrationError_& error) {
        XccyScriptDetail::OwnFailure(prepared_.Instrument(), data_, error, CalibrationStage_::Value_::Evaluate);
    }
    template Vector_<XccyScriptPayment_<double>> PreparedXccyScript_::EvaluatePayments(const XccyScriptPath_<double>&, bool) const;
    template Vector_<XccyScriptPayment_<AAD::Number_>> PreparedXccyScript_::EvaluatePayments(const XccyScriptPath_<AAD::Number_>&, bool) const;
    template class XccyScriptPathProvider_<double>;
    template class XccyScriptPathProvider_<AAD::Number_>;
} // namespace Dal::Script

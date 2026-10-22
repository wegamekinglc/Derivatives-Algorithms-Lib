#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>

#include <dal/calibration/validation_internal.hpp>
#include <dal/currency/currencydata.hpp>
#include <dal/indice/index/ir.hpp>
#include <dal/model/factory.hpp>
#include <dal/model/gsrmultidata.hpp>
#include <dal/model/gsrrateoption_internal.hpp>
#include <dal/script/calibrationpreparation.hpp>
#include <dal/script/calibrationrateoption_internal.hpp>
#include <dal/script/event.hpp>
#include <dal/script/instrument_internal.hpp>
#include <dal/script/preparation.hpp>
#include <dal/storage/json.hpp>

namespace Dal {
#include <dal/auto/MG_CalibrationBackend_enum.inc>
#include <dal/auto/MG_CalibrationDerivative_enum.inc>
#include <dal/auto/MG_CalibrationEvaluation_enum.inc>

    using namespace CalibrationInternal;
    namespace {
        using Script::Evaluator_;
        using Script::FindNode;
        using Script::Node_;
        using Script::NodeAdd_;
        using Script::NodeAssign_;
        using Script::NodeConst_;
        using Script::NodeConstVar_;
        using Script::NodeDiv_;
        using Script::NodeFix_;
        using Script::NodeIf_;
        using Script::NodeMax_;
        using Script::NodeMulti_;
        using Script::NodePays_;
        using Script::NodeSub_;
        using Script::NodeUMinus_;
        using Script::NodeUPlus_;
        using Script::NodeVar_;
        using Script::NodeVectorAssign_;
        using Script::NodeVectorEntry_;
        using Script::ScriptProduct_;

        CalibrationDiagnostic_ Diagnostic(const CalibrationInstrument_& instrument, const String_& measure) {
            CalibrationDiagnostic_ d;
            d.stage_ = CalibrationStage_::Value_::Prepare;
            d.instrumentId_ = instrument.Id();
            d.measureId_ = measure;
            return d;
        }
        Json_ ObjectData(const Storable_& object) { return Parse(JSON::WriteString(object)); }
        const GSRCurveData_* GaussianCurve(const Handle_<ModelData_>& model);
        void ValidateContextModel(const CalibrationPreparationContext_& c, const CalibrationDiagnostic_& d) {
            Check(!c.model_ || GaussianCurve(c.model_) != nullptr, "UnsupportedFastEvaluator", "/context/model",
                  "native one- or multi-factor Gaussian model; type names are not capability proofs", "", d);
        }
        String_ ContextIdentity(const CalibrationPreparationContext_& c, const CalibrationDiagnostic_& d) {
            const String_ code = "CalibrationContextMismatch";
            Check(c.valuation_.evaluationDate_ && c.valuation_.evaluationDate_->IsValid(), code, "/context/valuation_date",
                  "explicit valid valuation date", "", d);
            Check(c.valuation_.fixings_ && c.curve_, code, "/context", "explicit curve and fixing snapshot", "", d);
            Check(Token(c.currency_, code, "/context/currency", d) == c.curve_->currency_, code, "/context/currency", "curve currency",
                  c.curve_->currency_, d);
            Check(*c.valuation_.evaluationDate_ == c.curve_->evaluationDate_, code, "/context/valuation_date", "curve evaluation date", "", d);
            Token(c.valuation_.todayFixingPolicy_, code, "/context/today_fixing_policy", d);
            Check(!c.discountRoute_.empty() && !c.conventionState_.empty(), code, "/context", "explicit route and convention state", "", d);
            Check(std::isfinite(c.smoothing_) && c.smoothing_ >= 0., code, "/context/smoothing", "finite nonnegative smoothing", "", d);
            ValidateContextModel(c, d);
            auto fixings = Json_::Array();
            for (const auto& index : c.valuation_.fixings_->Values())
                for (const auto& value : index.second)
                    fixings.array_.push_back(Obj({{"index", Str(index.first)},
                                                  {"date", Str(Date::ToString(value.first.Date()))},
                                                  {"time", Num(value.first.Frac())},
                                                  {"value", Num(value.second)}}));
            return Fingerprint(Obj({{"valuation", Str(Date::ToString(*c.valuation_.evaluationDate_))},
                                    {"today", Str(c.valuation_.todayFixingPolicy_.String())},
                                    {"fixings", fixings},
                                    {"currency", Str(c.currency_.String())},
                                    {"route", Str(c.discountRoute_)},
                                    {"curve", ObjectData(*c.curve_)},
                                    {"model", c.model_ ? ObjectData(*c.model_) : Json_()},
                                    {"conventions", Str(c.conventionState_)},
                                    {"smoothing", Num(c.smoothing_)}}));
        }
        String_ RateContextIdentity(const CalibrationPreparationContext_& c, const String_& identity, const CalibrationDiagnostic_& d) {
            return Fingerprint(Obj({{"context", Str(identity)},
                                    {"libor_day_basis", Str(Token(Ccy::Conventions::LiborDayBasis()(c.currency_), "CalibrationContextMismatch",
                                                                  "/context/conventions/libor_day_basis", d))}}));
        }
        String_ DescriptorContextIdentity(const CalibrationPreparationContext_& c,
                                          const CalibrationFastDescriptor_& descriptor,
                                          const CalibrationDiagnostic_& d) {
            const auto identity = ContextIdentity(c, d);
            return std::holds_alternative<CalibrationRateOptionDescriptor_>(descriptor) ||
                           std::holds_alternative<CalibrationScriptMcDescriptor_>(descriptor)
                       ? RateContextIdentity(c, identity, d)
                       : identity;
        }
        bool Slot(const Node_& node, const String_& name) {
            const auto* slot = dynamic_cast<const NodeConstVar_*>(&node);
            return slot && slot->name_ == name;
        }
        bool Literal(const Node_& node, double value) {
            const auto* number = dynamic_cast<const NodeConst_*>(&node);
            return number && number->constVal_ == value;
        }
        bool Receiver(const Node_& node, const String_& name) {
            const auto* var = dynamic_cast<const NodeVar_*>(&node);
            return var && var->name_ == name;
        }
        const Script::SourceLocation_* NodeSource(const Node_& node);
        const Script::SourceLocation_* NestedSource(const Node_& node) {
            for (const auto& child : node.arguments_)
                if (const auto* source = NodeSource(*child))
                    return source;
            return nullptr;
        }
        const Script::SourceLocation_* AdditionalNodeSource(const Node_& node) {
            if (const auto* exercise = dynamic_cast<const Script::NodeExercise_*>(&node))
                return &exercise->source_;
            if (const auto* fix = dynamic_cast<const Script::NodeFix_*>(&node))
                return &fix->source_;
            if (const auto* spot = dynamic_cast<const Script::NodeSpot_*>(&node))
                return &spot->source_;
            if (const auto* append = dynamic_cast<const Script::NodeVectorAppend_*>(&node))
                return &append->source_;
            if (const auto* reduce = dynamic_cast<const Script::NodeVectorReduce_*>(&node))
                return &reduce->source_;
            return dynamic_cast<const NodeIf_*>(&node) ? NestedSource(node) : nullptr;
        }
        const Script::SourceLocation_* NodeSource(const Node_& node) {
            if (const auto* payment = dynamic_cast<const NodePays_*>(&node))
                return &payment->source_;
            if (const auto* assignment = dynamic_cast<const NodeAssign_*>(&node))
                return &assignment->source_;
            if (dynamic_cast<const NodeVectorAssign_*>(&node))
                return NodeSource(*node.arguments_[0]);
            if (const auto* entry = dynamic_cast<const NodeVectorEntry_*>(&node))
                return &entry->source_;
            return nullptr;
        }
        CalibrationDiagnostic_ WithSource(const Script::SourceLocation_* source, CalibrationDiagnostic_ d) {
            if (source && source->row_ != 0 && source->row_ <= INT_MAX) {
                d.sourceRow_ = static_cast<int>(source->row_);
                d.eventDate_ = source->eventDate_;
            }
            return d;
        }
        CalibrationDiagnostic_ WithNodeSource(const Node_& node, CalibrationDiagnostic_ d) { return WithSource(NodeSource(node), std::move(d)); }
        CalibrationDiagnostic_ GeneralSource(const Node_& node, CalibrationDiagnostic_ d) {
            const auto* source = NodeSource(node);
            return WithSource(source ? source : AdditionalNodeSource(node), std::move(d));
        }
        String_ SourceField(const CalibrationDiagnostic_& d) {
            return d.sourceRow_ && *d.sourceRow_ > 0 ? "/source/rows/" + OrdinalText(*d.sourceRow_ - 1) + "/text" : String_("/source/rows");
        }
        void GuardAssignment(const Node_& node, bool paid, const String_& receiver, const CalibrationDiagnostic_& d) {
            if (dynamic_cast<const NodeAssign_*>(&node) && Receiver(*node.arguments_[0], receiver))
                Check(!paid && Literal(*node.arguments_[1], 0.), "InvalidCalibrationOutputBinding", SourceField(d),
                      "literal-zero receiver initialization before every historical or live PAYS", "", d);
            if (const auto* vector = dynamic_cast<const NodeVectorAssign_*>(&node)) {
                const auto* entry = dynamic_cast<const NodeVectorEntry_*>(vector->arguments_[0].get());
                Check(!entry || entry->name_ != receiver, "InvalidCalibrationOutputBinding", SourceField(d), "scalar Price receiver", "", d);
            }
        }
        bool Guard(const Node_& node, bool paid, const String_& receiver, const CalibrationDiagnostic_& d) {
            const auto source = WithNodeSource(node, d);
            const auto visit = [&](size_t first, size_t last, bool state) {
                for (size_t i = first; i < last; ++i)
                    state = Guard(*node.arguments_[i], state, receiver, source);
                return state;
            };
            if (const auto* branch = dynamic_cast<const NodeIf_*>(&node)) {
                const auto firstElse = branch->LastTrueIndex() + 1;
                const bool thenPaid = visit(1, firstElse, paid);
                const bool elsePaid = visit(firstElse, node.arguments_.size(), paid);
                return thenPaid || elsePaid;
            }
            if (dynamic_cast<const NodePays_*>(&node)) {
                Check(Receiver(*node.arguments_[0], receiver), "InvalidCalibrationOutputBinding", SourceField(source), "one declared PAYS receiver",
                      "", source);
                return true;
            }
            GuardAssignment(node, paid, receiver, source);
            return visit(0, node.arguments_.size(), paid);
        }
        using CalibrationRateOptionInternal::Payment_;
        void ValidateAffineControls(const Node_& node, const CalibrationDiagnostic_& d) {
            if (const auto* branch = dynamic_cast<const NodeIf_*>(&node)) {
                const auto* parameter = FindNode(*branch->arguments_[0], [](const Node_& n) { return Slot(n, "K"); });
                Check(parameter == nullptr, "UnsupportedCalibrationMeasure", "/source/rows", "K-independent branch conditions", "", d);
            }
            for (const auto& child : node.arguments_)
                ValidateAffineControls(*child, d);
        }
        template <class F_> void ForStatements(const ScriptProduct_& product, CalibrationDiagnostic_ d, const F_& action) {
            for (size_t e = 0; e < product.Events().size(); ++e) {
                d.eventDate_ = product.EventDates()[e];
                d.sourceRow_ = static_cast<int>(product.ParsedEventSources()[e][0].row_);
                for (size_t s = 0; s < product.Events()[e].size(); ++s) {
                    d.statement_ = static_cast<int>(s);
                    const auto& node = *product.Events()[e][s];
                    action(node, WithNodeSource(node, d));
                }
            }
        }
        bool GuardProduct(const ScriptProduct_& product, const String_& receiver, const CalibrationDiagnostic_& d) {
            bool paid = false;
            ForStatements(product, d, [&](const Node_& node, const auto& source) { paid = Guard(node, paid, receiver, source); });
            return paid;
        }
        Vector_<Payment_> Payments(const ScriptProduct_& product, const String_& receiver, const CalibrationDiagnostic_& d, bool affine) {
            Vector_<Payment_> result;
            ForStatements(product, d, [&](const Node_& node, CalibrationDiagnostic_ source) {
                if (affine)
                    ValidateAffineControls(node, source);
                if (const auto* payment = dynamic_cast<const NodePays_*>(&node)) {
                    source.sourceRow_ = static_cast<int>(payment->source_.row_);
                    result.push_back({payment, *source.eventDate_, source});
                } else {
                    Check(dynamic_cast<const NodeAssign_*>(&node) && Receiver(*node.arguments_[0], receiver) && Literal(*node.arguments_[1], 0.),
                          "UnsupportedCalibrationProduct", "/source/rows", "P0 payments with optional leading receiver initialization", "", source);
                }
            });
            Check(!result.empty(), "NoSharedPriceDefinition", "/source/rows", "shared PAYS cashflows", "", d);
            return result;
        }
        struct Affine_ {
            double a_ = 0., b_ = 0.;
            bool dependent_ = false;
        };
        Affine_ AffineBinary(const Node_& node, const Affine_& lhs, const Affine_& rhs, const CalibrationDiagnostic_& d) {
            const bool dependent = lhs.dependent_ || rhs.dependent_;
            if (dynamic_cast<const NodeAdd_*>(&node))
                return {lhs.a_ + rhs.a_, lhs.b_ + rhs.b_, dependent};
            if (dynamic_cast<const NodeSub_*>(&node))
                return {lhs.a_ - rhs.a_, lhs.b_ - rhs.b_, dependent};
            if (dynamic_cast<const NodeMulti_*>(&node)) {
                Check(!(lhs.dependent_ && rhs.dependent_), "UnsupportedCalibrationMeasure", "/source/rows", "K-independent multiplier", "", d);
                return {lhs.a_ * rhs.b_ + lhs.b_ * rhs.a_, lhs.b_ * rhs.b_, dependent};
            }
            if (dynamic_cast<const NodeDiv_*>(&node)) {
                Check(!rhs.dependent_ && rhs.b_ != 0., "UnsupportedCalibrationMeasure", "/source/rows", "nonzero K-independent divisor", "", d);
                return {lhs.a_ / rhs.b_, lhs.b_ / rhs.b_, dependent};
            }
            Fail("UnsupportedCalibrationMeasure", "/source/rows", "structurally affine K arithmetic", "", d);
        }
        Affine_ Affine(const Node_& node, const CalibrationDiagnostic_& d) {
            if (const auto* slot = dynamic_cast<const NodeConstVar_*>(&node))
                return slot->name_ == "K" ? Affine_{1., 0., true} : Affine_{0., slot->constVal_, false};
            if (const auto* number = dynamic_cast<const NodeConst_*>(&node))
                return {0., number->constVal_, false};
            Check(!node.arguments_.empty(), "UnsupportedCalibrationMeasure", "/source/rows", "structurally affine K expression", "", d);
            const auto lhs = Affine(*node.arguments_[0], d);
            if (dynamic_cast<const NodeUPlus_*>(&node))
                return lhs;
            if (dynamic_cast<const NodeUMinus_*>(&node))
                return {-lhs.a_, -lhs.b_, lhs.dependent_};
            Check(node.arguments_.size() == 2, "UnsupportedCalibrationMeasure", "/source/rows", "affine arithmetic", "", d);
            return AffineBinary(node, lhs, Affine(*node.arguments_[1], d), d);
        }
        bool DepositStart(const Node_& node) { return dynamic_cast<const NodeUMinus_*>(&node) && Slot(*node.arguments_[0], "N"); }
        bool DepositEnd(const Node_& node) {
            if (!dynamic_cast<const NodeMulti_*>(&node) || !Slot(*node.arguments_[0], "N"))
                return false;
            const auto& sum = *node.arguments_[1];
            if (!dynamic_cast<const NodeAdd_*>(&sum) || !Literal(*sum.arguments_[0], 1.))
                return false;
            const auto& product = *sum.arguments_[1];
            return dynamic_cast<const NodeMulti_*>(&product) && Slot(*product.arguments_[0], "K") && Slot(*product.arguments_[1], "ALPHA");
        }
        double Bound(const CalibrationInstrument_& instrument, const String_& name) {
            for (const auto& value : instrument.Binding())
                if (value.key_ == name)
                    return value.value_;
            Fail("UnsupportedCalibrationProduct", "/parameters", "required P0 parameter", name);
        }
        void BondOptionParameter(
            const CalibrationInstrument_& instrument, const String_& key, ScriptParameterRole_ role, const String_& unit, CalibrationDiagnostic_ d) {
            const auto& declarations = instrument.Definition()->Spec().parameters_;
            const auto found = std::find_if(declarations.begin(), declarations.end(), [&](const auto& p) { return Bytes(p.key_) == Bytes(key); });
            const String_ code = "UnsupportedFastEvaluator";
            d.parameterKey_ = key;
            Check(found != declarations.end(), code, "/parameters", "BondOption parameter " + key, "", d);
            const auto index = static_cast<size_t>(found - declarations.begin());
            d.sourceRow_ = static_cast<int>(index + 1);
            const String_ path = "/parameters/" + OrdinalText(index);
            Check(found->type_ == ScriptParameterType_::Value_::Real && found->bindable_, code, path, "bindable Real parameter", "", d);
            Check(found->role_ == role, code, path + "/role", role.String(), found->role_.String(), d);
            Check(Bytes(found->unit_.Token()) == Bytes(unit), code, path + "/unit", unit, found->unit_.Token(), d);
            ValidateParameter(Bound(instrument, key), *found, "/binding/values/" + OrdinalText(index) + "/value", d, code);
        }
        void BondOptionParameters(const CalibrationInstrument_& instrument, const CalibrationDiagnostic_& d) {
            Check(instrument.Definition()->Spec().parameters_.size() == 2, "UnsupportedFastEvaluator", "/parameters",
                  "exactly N/Notional and K/Strike for BondOption; no ALPHA", "", d);
            BondOptionParameter(instrument, "N", ScriptParameterRole_::Value_::Notional,
                                "Price:" + String_(instrument.Definition()->Spec().header_.payCurrency_.String()), d);
            BondOptionParameter(instrument, "K", ScriptParameterRole_::Value_::Strike, "BondPrice", d);
        }
        double PaymentAmount(const ScriptProduct_& product, const NodePays_& payment) {
            Evaluator_<double> evaluator(Vector_<>(product.VarNames().size(), 0.), product.ConstVarValues(), product.VectorCapacities());
            AAD::Scenario_<> scenario(1);
            scenario[0].numeraire_ = 1.;
            evaluator.SetScenario(&scenario);
            evaluator.Init();
            evaluator.SetCurEvt(0);
            payment.Accept(evaluator);
            return evaluator.VarVals()[product.PayOffIdx()];
        }
        const Node_* OptionDifference(const Node_& amount) {
            if (!dynamic_cast<const NodeMulti_*>(&amount) || !Slot(*amount.arguments_[0], "N"))
                return nullptr;
            const auto& maximum = *amount.arguments_[1];
            if (!dynamic_cast<const NodeMax_*>(&maximum) || maximum.arguments_.size() != 2 || !Literal(*maximum.arguments_[1], 0.))
                return nullptr;
            const auto& difference = *maximum.arguments_[0];
            if (!dynamic_cast<const NodeSub_*>(&difference))
                return nullptr;
            return &difference;
        }
        std::pair<const NodeFix_*, bool> OptionFix(const Node_& difference) {
            const bool call = Slot(*difference.arguments_[1], "K");
            if (!call && !Slot(*difference.arguments_[0], "K"))
                return {nullptr, call};
            const auto* fix = dynamic_cast<const NodeFix_*>(difference.arguments_[call ? 0 : 1].get());
            if (!fix || fix->fixingDate_)
                return {nullptr, call};
            return {fix, call};
        }
        std::optional<CalibrationBondOptionDescriptor_> BondOption(const Vector_<Payment_>& payments,
                                                                   const CalibrationInstrument_& instrument,
                                                                   const CalibrationPreparationContext_& c,
                                                                   const CalibrationDiagnostic_& d) {
            if (payments.size() != 1)
                return {};
            const auto* difference = OptionDifference(*payments[0].node_->arguments_[1]);
            if (!difference)
                return {};
            const auto [fix, call] = OptionFix(*difference);
            if (!fix)
                return {};
            const auto* df = dynamic_cast<const Index::DF_*>(fix->index_.get());
            if (!df || !Cell::IsDate(df->maturity_) || !Cell::IsEmpty(df->start_))
                return {};
            BondOptionParameters(instrument, d);
            Check(df->ccy_ == c.currency_, "CalibrationContextMismatch", "/source/rows", "DF currency matching contract and context", "", d);
            const Date_ expiry = payments[0].date_, maturity = Cell::ToDate(df->maturity_);
            Check(*c.valuation_.evaluationDate_ < expiry && expiry < maturity, "UnsupportedFastEvaluator", "/source/rows",
                  "valuation < expiry < observed bond maturity", "", d);
            return CalibrationBondOptionDescriptor_{expiry, maturity, Bound(instrument, "N"), Bound(instrument, "K"), call, df->Name()};
        }
        const GSRCurveData_* GaussianCurve(const Handle_<ModelData_>& model) {
            if (model && typeid(*model) == typeid(MultiFactorGSRModelData_))
                return static_cast<const MultiFactorGSRModelData_&>(*model).curve_.get();
            if (model && typeid(*model) == typeid(GSRModelData_))
                return static_cast<const GSRModelData_&>(*model).curve_.get();
            return nullptr;
        }
        void GaussianContext(const CalibrationPreparationContext_& c, const CalibrationDiagnostic_& d) {
            const auto* modelCurve = GaussianCurve(c.model_);
            Check(modelCurve != nullptr, "UnsupportedFastEvaluator", "/context/model", "native one- or multi-factor Gaussian model", "", d);
            Check(Bytes(Fingerprint(ObjectData(*modelCurve))) == Bytes(Fingerprint(ObjectData(*c.curve_))), "CalibrationContextMismatch",
                  "/context/model/curve", "same discount curve in model and context", "", d);
            Check(c.smoothing_ == 0., "UnsupportedFastEvaluator", "/context/smoothing", "hard Gaussian payoff with zero smoothing", "", d);
        }
        void McMeasure(bool deposit, const CalibrationMeasureSpec_& measure, bool executable, const CalibrationDiagnostic_& d) {
            Check(measure.kind_ == CalibrationMeasureKind_::Value_::Price ||
                      (executable && deposit && measure.kind_ == CalibrationMeasureKind_::Value_::ParAffineRate),
                  "UnsupportedCalibrationMeasure", "/measure_id", "shared Price or proven Deposit affine rate", "", d);
        }
        void McSampling(const CalibrationEvaluationOptions_& options, const CalibrationDiagnostic_& d) {
            Check(options.sampling_ && options.sampling_->paths_ >= 2 && options.sampling_->seed_ >= 0, "InvalidCalibrationPlan", "/options/sampling",
                  "explicit paths >= 2 and nonnegative seed", "", d);
            Check(Bytes(options.sampling_->generator_) == "irn" || Bytes(options.sampling_->generator_) == "mrg32", "InvalidCalibrationPlan",
                  "/options/sampling/generator", "irn or mrg32", "", d);
        }
        CalibrationDerivative_ McDerivative(const CalibrationEvaluationOptions_& options, bool executable, const CalibrationDiagnostic_& d) {
            Check(options.derivativeMethod_ == CalibrationDerivative_::Value_::Auto ||
                      options.derivativeMethod_ == CalibrationDerivative_::Value_::None ||
                      (executable && options.derivativeMethod_ == CalibrationDerivative_::Value_::AAD),
                  "DerivativeUnavailable", "/options/derivative_method", "AAD or None for MC", "", d);
            return options.derivativeMethod_ == CalibrationDerivative_::Value_::AAD ? CalibrationDerivative_::Value_::AAD
                                                                                    : CalibrationDerivative_::Value_::None;
        }
        CalibrationPreparationCapability_ AdmitMonteCarlo(bool deposit,
                                                          const CalibrationMeasureSpec_& measure,
                                                          const CalibrationPreparationContext_& context,
                                                          const CalibrationEvaluationOptions_& options,
                                                          const CalibrationDiagnostic_& d) {
            Check(options.allowMonteCarloCalibration_, "UnsupportedFastEvaluator", "/options/allow_monte_carlo_calibration",
                  "explicit MC calibration opt-in", "false", d);
            const bool executable = options.sampling_ && Bytes(options.sampling_->generator_) == "mrg32";
            McMeasure(deposit, measure, executable, d);
            GaussianContext(context, d);
            McSampling(options, d);
            return {deposit ? "dal.deposit-origination/1" : "dal.gaussian-bond-option/1", CalibrationBackend_::Value_::ScriptMC,
                    McDerivative(options, executable, d), true, deposit};
        }
        CalibrationPreparationCapability_
        AdmitDeterministic(bool deposit, const CalibrationEvaluationOptions_& options, const CalibrationDiagnostic_& d) {
            Check(deposit || options.evaluation_ != CalibrationEvaluation_::Value_::GenericDeterministic, "UnsupportedFastEvaluator",
                  "/options/evaluation", "Gaussian expectation; forward substitution is not deterministic pricing", "", d);
            const auto backend = options.evaluation_ == CalibrationEvaluation_::Value_::GenericDeterministic
                                     ? CalibrationBackend_::Value_::GenericDeterministic
                                     : (deposit ? CalibrationBackend_::Value_::FastDeterministic : CalibrationBackend_::Value_::FastGaussian);
            const auto supported = deposit ? CalibrationDerivative_::Value_::Analytic : CalibrationDerivative_::Value_::FiniteDifference;
            const auto derivative =
                options.derivativeMethod_ == CalibrationDerivative_::Value_::Auto ? CalibrationDerivative_(supported) : options.derivativeMethod_;
            Check(derivative == supported || derivative == CalibrationDerivative_::Value_::None, "DerivativeUnavailable",
                  "/options/derivative_method", "supported native derivative or None", "", d);
            return {deposit ? "dal.deposit-origination/1" : "dal.gaussian-bond-option/1", backend, derivative, true, deposit};
        }
        CalibrationPreparationCapability_ Admit(bool deposit,
                                                const CalibrationMeasureSpec_& measure,
                                                const CalibrationPreparationContext_& context,
                                                const CalibrationEvaluationOptions_& options,
                                                const CalibrationDiagnostic_& d) {
            Token(options.evaluation_, "UnsupportedFastEvaluator", "/options/evaluation", d);
            Token(options.derivativeMethod_, "DerivativeUnavailable", "/options/derivative_method", d);
            if (options.evaluation_ == CalibrationEvaluation_::Value_::ScriptMC)
                return AdmitMonteCarlo(deposit, measure, context, options, d);
            return AdmitDeterministic(deposit, options, d);
        }
        bool IsDeposit(const Vector_<Payment_>& payments) {
            return payments.size() == 2 && DepositStart(*payments[0].node_->arguments_[1]) && DepositEnd(*payments[1].node_->arguments_[1]) &&
                   payments[0].date_ < payments[1].date_;
        }
        struct Payoff_ {
            CalibrationFastDescriptor_ descriptor_;
            Vector_<CalibrationCashflow_> cashflows_;
        };
        Payoff_ OptionPayoff(const Vector_<Payment_>& payments,
                             const CalibrationInstrument_& instrument,
                             const CalibrationPreparationContext_& context,
                             const CalibrationEvaluationOptions_& options,
                             const CalibrationMeasureSpec_& measure,
                             const CalibrationDiagnostic_& d) {
            const auto bond = BondOption(payments, instrument, context, d);
            if (bond) {
                GaussianContext(context, d);
                return {*bond,
                        {{bond->expiry_, {}, 0., 0., *payments[0].source_.sourceRow_, *payments[0].source_.statement_, bond->index_, bond->expiry_}}};
            }
            const auto rate = CalibrationRateOptionInternal::Match(payments, instrument, context);
            Check(static_cast<bool>(rate), "UnsupportedFastEvaluator", "/source/rows", "complete Deposit, BondOption, Caplet or Swaption AST", "",
                  payments[0].source_);
            GaussianContext(context, d);
            Check(measure.kind_ == CalibrationMeasureKind_::Value_::Price, "UnsupportedCalibrationMeasure", "/measure_id", "shared rate option Price",
                  "", d);
            GaussianRateOptionInternal::RequireDomain(*rate, context, options.derivativeMethod_ == CalibrationDerivative_::Value_::AAD,
                                                      payments[0].source_);
            return {*rate,
                    {{payments[0].date_,
                      {},
                      0.,
                      0.,
                      *payments[0].source_.sourceRow_,
                      *payments[0].source_.statement_,
                      {},
                      std::visit([](const auto& o) { return o.expiry_; }, rate->option_)}}};
        }
        Payoff_ Payoff(bool deposit,
                       const Vector_<Payment_>& payments,
                       ScriptProduct_* product,
                       const CalibrationInstrument_& instrument,
                       const CalibrationPreparationContext_& context,
                       const CalibrationEvaluationOptions_& options,
                       const CalibrationMeasureSpec_& measure,
                       const CalibrationDiagnostic_& d) {
            if (!deposit)
                return OptionPayoff(payments, instrument, context, options, measure, d);
            Check(*context.valuation_.evaluationDate_ == payments[0].date_, "UnsupportedCalibrationMeasure", "/context/valuation_date",
                  "Deposit origination valuation", "", d);
            Payoff_ result{CalibrationDepositDescriptor_{payments[0].date_, payments[1].date_, Bound(instrument, "N"), Bound(instrument, "K"),
                                                         Bound(instrument, "ALPHA")},
                           {}};
            product->IndexVariables();
            for (const auto& payment : payments) {
                const auto coefficients = Affine(*payment.node_->arguments_[1], payment.source_);
                result.cashflows_.push_back({payment.date_, PaymentAmount(*product, *payment.node_), coefficients.a_, coefficients.b_,
                                             *payment.source_.sourceRow_, *payment.source_.statement_});
            }
            return result;
        }
        bool RateTemplateShape(const Vector_<Payment_>& payments) {
            if (payments.size() != 1)
                return false;
            const auto& amount = *payments[0].node_->arguments_[1];
            if (!dynamic_cast<const NodeMulti_*>(&amount) || !Slot(*amount.arguments_[0], "N"))
                return false;
            const auto& option = *amount.arguments_[1];
            if (dynamic_cast<const NodeMulti_*>(&option))
                return dynamic_cast<const NodeMax_*>(option.arguments_[1].get()) != nullptr;
            const auto* difference = OptionDifference(amount);
            if (!difference)
                return false;
            const auto strike = [](const Node_& node) { return dynamic_cast<const NodeMulti_*>(&node) && Slot(*node.arguments_[0], "K"); };
            return strike(*difference->arguments_[0]) || strike(*difference->arguments_[1]);
        }
        bool NativeTemplate(const ScriptProduct_& product,
                            const CalibrationInstrument_& instrument,
                            const CalibrationPreparationContext_& context,
                            const CalibrationDiagnostic_& d) {
            Vector_<Payment_> payments;
            bool plain = true;
            ForStatements(product, d, [&](const Node_& node, const auto& source) {
                if (const auto* pay = dynamic_cast<const NodePays_*>(&node))
                    payments.push_back({pay, *source.eventDate_, source});
                else if (!(dynamic_cast<const NodeAssign_*>(&node) &&
                           Receiver(*node.arguments_[0], instrument.Definition()->Spec().header_.receiver_) && Literal(*node.arguments_[1], 0.)))
                    plain = false;
            });
            return plain && (IsDeposit(payments) || BondOption(payments, instrument, context, d).has_value() || RateTemplateShape(payments));
        }
        bool VectorNode(const Node_& node) {
            return dynamic_cast<const NodeVectorAssign_*>(&node) || dynamic_cast<const NodeVectorEntry_*>(&node) ||
                   dynamic_cast<const Script::NodeVectorReduce_*>(&node) || dynamic_cast<const Script::NodeVectorAppend_*>(&node) ||
                   dynamic_cast<const Script::NodeCollect_*>(&node);
        }
        void GeneralFixing(const Script::NodeFix_& fix, const CalibrationPreparationContext_& c, const CalibrationDiagnostic_& d) {
            const auto* df = dynamic_cast<const Index::DF_*>(fix.index_.get());
            const auto* libor = dynamic_cast<const Index::Libor_*>(fix.index_.get());
            Check(df || libor, "UnsupportedCalibrationProduct", SourceField(d), "Gaussian DF or Libor FIX", "", d);
            Check((df ? df->ccy_ : libor->ccy_) == c.currency_, "CalibrationContextMismatch", SourceField(d), "same-currency FIX", "", d);
            const auto date = fix.fixingDate_.value_or(*d.eventDate_);
            const bool historical =
                date < *c.valuation_.evaluationDate_ ||
                (date == *c.valuation_.evaluationDate_ && c.valuation_.todayFixingPolicy_ == TodayFixingPolicy_::Value_::REQUIREHISTORICAL);
            Check(!historical || c.valuation_.fixings_->Find(fix.index_->Name(), DateTime_(date, 0.)).has_value(), "InvalidCalibrationPlan",
                  SourceField(d), "explicit historical fixing snapshot", fix.index_->Name(), d);
        }
        void GeneralNode(const Node_& node, const CalibrationPreparationContext_& c, const CalibrationDiagnostic_& d) {
            const auto source = GeneralSource(node, d);
            Check(!dynamic_cast<const Script::NodeExercise_*>(&node), "UnsupportedCalibrationProduct", SourceField(source), "no early exercise", "",
                  source);
            Check(!dynamic_cast<const NodeIf_*>(&node) && !dynamic_cast<const Script::BoolNode_*>(&node), "UnsupportedCalibrationProduct",
                  SourceField(source), "hard scalar arithmetic without IF/comparison control", "", source);
            Check(!VectorNode(node) && !dynamic_cast<const Script::NodeSpot_*>(&node), "UnsupportedCalibrationProduct", SourceField(source),
                  "scalar Gaussian FIX arithmetic", "", source);
            if (const auto* pay = dynamic_cast<const NodePays_*>(&node))
                Check(!pay->paymentDate_, "UnsupportedFastEvaluator", SourceField(source), "AtEvent payment without ON clause", "", source);
            if (const auto* fix = dynamic_cast<const Script::NodeFix_*>(&node))
                GeneralFixing(*fix, c, source);
            for (const auto& child : node.arguments_)
                GeneralNode(*child, c, source);
        }
        void GeneralProduct(const ScriptProduct_& product,
                            const CalibrationInstrument_& instrument,
                            bool paid,
                            const CalibrationPreparationContext_& c,
                            const CalibrationDiagnostic_& d) {
            ForStatements(product, d, [&](const Node_& node, const auto& source) { GeneralNode(node, c, source); });
            Check(paid, "NoSharedPriceDefinition", "/source/rows", "shared PAYS cashflows", "", d);
            try {
                auto model = CreateModel<double>(c.model_);
                Script::MonteCarloSettings_ simulation;
                simulation.rsg_ = "mrg32";
                simulation.compiled_ = false;
                static_cast<void>(Script::Detail::PrepareBoundScript(instrument.Product(), model.get(), c.valuation_, simulation,
                                                                     instrument.Definition()->Spec().source_.settings_));
            } catch (const CalibrationError_&) {
                throw;
            } catch (const Exception_& error) {
                Fail("InvalidCalibrationPlan", "/mc/preparation", "executable bound Gaussian Price script", String_(error.what()), d);
            }
        }
        struct Preparation_ {
            Payoff_ payoff_;
            CalibrationPreparationCapability_ capability_;
        };
        Preparation_ NativePreparation(ScriptProduct_* product,
                                       const CalibrationInstrument_& instrument,
                                       const CalibrationMeasureSpec_& measure,
                                       const CalibrationPreparationContext_& context,
                                       const CalibrationEvaluationOptions_& options,
                                       const CalibrationDiagnostic_& d) {
            const auto payments =
                Payments(*product, instrument.Definition()->Spec().header_.receiver_, d, measure.kind_ != CalibrationMeasureKind_::Value_::Price);
            if (measure.kind_ != CalibrationMeasureKind_::Value_::Price)
                for (const auto& payment : payments)
                    static_cast<void>(Affine(*payment.node_->arguments_[1], payment.source_));
            const bool deposit = IsDeposit(payments);
            for (const auto& payment : payments)
                Check(!payment.node_->paymentDate_, "UnsupportedFastEvaluator", "/source/rows", "AtEvent payment without ON clause", "",
                      payment.source_);
            return {Payoff(deposit, payments, product, instrument, context, options, measure, d), Admit(deposit, measure, context, options, d)};
        }
        Preparation_ LowerForExecution(ScriptProduct_* product,
                                       const CalibrationInstrument_& instrument,
                                       const CalibrationMeasureSpec_& measure,
                                       const CalibrationPreparationContext_& context,
                                       const CalibrationEvaluationOptions_& options,
                                       const CalibrationDiagnostic_& d) {
            const bool paid = GuardProduct(*product, instrument.Definition()->Spec().header_.receiver_, d);
            if (options.evaluation_ != CalibrationEvaluation_::Value_::ScriptMC || measure.kind_ != CalibrationMeasureKind_::Value_::Price)
                return NativePreparation(product, instrument, measure, context, options, d);
            if (NativeTemplate(*product, instrument, context, d))
                return NativePreparation(product, instrument, measure, context, options, d);
            auto capability = Admit(false, measure, context, options, d);
            Check(Bytes(options.sampling_->generator_) == "mrg32", "InvalidCalibrationPlan", "/options/sampling/generator",
                  "executable mrg32 for general Price", "", d);
            GeneralProduct(*product, instrument, paid, context, d);
            capability.ruleId_ = "dal.gaussian-script-price/1";
            capability.affine_ = false;
            return {{CalibrationScriptMcDescriptor_{}, {}}, capability};
        }
        String_ GenericCompilerIdentity() {
            return Fingerprint(Obj({{"build", Str("dal.core/1")},
                                    {"parser", Str("typed-slots/1")},
                                    {"lowering", Str("script-price/1")},
                                    {"matcher", Str("whole-scalar-ast/1")},
                                    {"constant_binding", Str("effective-slots/1")},
                                    {"receiver_guard", Str("whole-history/2")}}));
        }
        String_ CompilerIdentity(bool rate) {
            return Fingerprint(Obj({{"build", Str("dal.core/1")},
                                    {"parser", Str("typed-slots/1")},
                                    {"lowering", Str(rate ? "rate-option-cashflows/1" : "p0-cashflows/1")},
                                    {"matcher", Str(rate ? "rate-option-ast/1" : "p0-ast/2")},
                                    {"constant_binding", Str("effective-slots/1")},
                                    {"receiver_guard", Str("whole-history/2")}}));
        }
        void Discounts(const Vector_<>& discounts, size_t size, const CalibrationDiagnostic_& d) {
            Check(discounts.size() == size, "InvalidCalibrationPlan", "/discounts", "one discount per live cashflow", "", d);
            for (double value : discounts)
                Check(std::isfinite(value) && value > 0., "CalibrationNumericalFailure", "/discounts", "finite positive discounts", "", d);
        }
    } // namespace

    PreparedCalibrationInstrument_ PrepareCalibrationInstrument(const Handle_<CalibrationInstrument_>& instrument,
                                                                const String_& measureId,
                                                                const CalibrationPreparationContext_& context,
                                                                const CalibrationEvaluationOptions_& options) {
        Check(static_cast<bool>(instrument), "InvalidCalibrationPlan", "/instrument", "nonnull instrument");
        auto d = Diagnostic(*instrument, measureId);
        PreparedCalibrationInstrument_ result;
        result.instrument_ = instrument;
        const auto& spec = instrument->Definition()->Spec();
        const auto found =
            std::find_if(spec.measures_.begin(), spec.measures_.end(), [&](const auto& m) { return Bytes(m.id_) == Bytes(measureId); });
        Check(found != spec.measures_.end(), "UnsupportedCalibrationMeasure", "/measure_id", "declared measure ID", measureId, d);
        result.measure_ = *found;
        result.contextIdentity_ = ContextIdentity(context, d);
        Check(context.currency_ == spec.header_.payCurrency_ && Bytes(context.discountRoute_) == Bytes(spec.header_.discountRoute_),
              "CalibrationContextMismatch", "/context", "matching currency and explicit discount route", "", d);
        auto product = instrument->Product();
        auto lowered = LowerForExecution(product.get(), *instrument, result.measure_, context, options, d);
        result.descriptor_ = std::move(lowered.payoff_.descriptor_);
        result.cashflows_ = std::move(lowered.payoff_.cashflows_);
        result.capability_ = std::move(lowered.capability_);
        const auto* rate = std::get_if<CalibrationRateOptionDescriptor_>(&result.descriptor_);
        const bool generic = std::holds_alternative<CalibrationScriptMcDescriptor_>(result.descriptor_);
        if (rate)
            result.capability_.ruleId_ = CalibrationRateOptionInternal::Rule(*rate);
        if (rate || generic)
            result.contextIdentity_ = RateContextIdentity(context, result.contextIdentity_, d);
        if (result.capability_.backend_ == CalibrationBackend_::Value_::ScriptMC)
            result.sampling_ = options.sampling_;
        result.compilerIdentity_ = generic ? GenericCompilerIdentity() : CompilerIdentity(rate != nullptr);
        result.planIdentity_ = Fingerprint(Obj({{"instrument", Str(instrument->InstrumentIdentity())},
                                                {"measure", Str(measureId)},
                                                {"compiler", Str(result.compilerIdentity_)},
                                                {"context", Str(result.contextIdentity_)},
                                                {"rule", Str(result.capability_.ruleId_)},
                                                {"backend", Str(result.capability_.backend_.String())},
                                                {"derivative", Str(result.capability_.derivativeMethod_.String())},
                                                {"sampling", Optional(result.sampling_, [](const auto& s) {
                                                     return Obj({{"paths", Num(s.paths_)}, {"seed", Num(s.seed_)}, {"generator", Str(s.generator_)}});
                                                 })}}));
        return result;
    }
    void PreparedCalibrationInstrument_::RequireCompatible(const Handle_<CalibrationInstrument_>& instrument,
                                                           const CalibrationPreparationContext_& context) const {
        const auto d = Diagnostic(*instrument_, measure_.id_);
        Check(instrument && Bytes(instrument->InstrumentIdentity()) == Bytes(instrument_->InstrumentIdentity()), "InvalidCalibrationPlan",
              "/instrument", "prepared instrument identity", "", d);
        Check(Bytes(DescriptorContextIdentity(context, descriptor_, d)) == Bytes(contextIdentity_), "CalibrationContextMismatch", "/context",
              "prepared context identity; re-prepare after changes", "", d);
    }
    CalibrationAffineCoefficients_ PreparedCalibrationInstrument_::Coefficients(const Vector_<>& discounts,
                                                                                const Matrix_<>& discountDerivatives) const {
        auto d = Diagnostic(*instrument_, measure_.id_);
        d.stage_ = CalibrationStage_::Value_::Evaluate;
        Check(capability_.affine_, "UnsupportedCalibrationMeasure", "/measure_id", "affine contract", "", d);
        Discounts(discounts, cashflows_.size(), d);
        const int columns = discountDerivatives.Cols();
        Check(columns == 0 || discountDerivatives.Rows() == static_cast<int>(cashflows_.size()), "InvalidCalibrationPlan", "/discount_derivatives",
              "cashflow rows by native parameter columns", "", d);
        CalibrationAffineCoefficients_ result{0., 0., Vector_<>(columns, 0.), Vector_<>(columns, 0.)};
        for (size_t i = 0; i < cashflows_.size(); ++i) {
            result.a_ += cashflows_[i].a_ * discounts[i];
            result.b_ += cashflows_[i].b_ * discounts[i];
            for (int j = 0; j < columns; ++j) {
                const double derivative = discountDerivatives(static_cast<int>(i), j);
                Check(std::isfinite(derivative), "CalibrationNumericalFailure", "/discount_derivatives", "finite native partials", "", d);
                result.da_[j] += cashflows_[i].a_ * derivative;
                result.db_[j] += cashflows_[i].b_ * derivative;
            }
        }
        Check(std::isfinite(result.a_) && std::isfinite(result.b_), "CalibrationNumericalFailure", "/coefficients", "finite A and B", "", d);
        for (size_t i = 0; i < result.da_.size(); ++i)
            Check(std::isfinite(result.da_[i]) && std::isfinite(result.db_[i]), "CalibrationNumericalFailure", "/coefficient_derivatives",
                  "finite independent dA and dB", "", d);
        return result;
    }
    CalibrationMeasureValue_ PreparedCalibrationInstrument_::AffineMeasure(const CalibrationAffineCoefficients_& coefficients) const {
        auto d = Diagnostic(*instrument_, measure_.id_);
        d.stage_ = CalibrationStage_::Value_::Evaluate;
        Check(measure_.annuitySign_ && measure_.minAbsAnnuity_, "UnsupportedCalibrationMeasure", "/measure_id", "affine Scalar measure", "", d);
        const double a = coefficients.a_, b = coefficients.b_;
        Check(std::isfinite(a) && std::isfinite(b) && a * *measure_.annuitySign_ >= *measure_.minAbsAnnuity_, "InvalidCalibrationAnnuity", "/annuity",
              "finite annuity with declared sign and minimum magnitude", NumberText(a), d);
        Check(coefficients.da_.size() == coefficients.db_.size(), "InvalidCalibrationPlan", "/coefficient_derivatives",
              "independent dA and dB in the same native coordinates", "", d);
        CalibrationMeasureValue_ result{-b / a, Vector_<>(coefficients.da_.size())};
        for (size_t i = 0; i < result.derivatives_.size(); ++i)
            result.derivatives_[i] = -coefficients.db_[i] / a + (b / a) * (coefficients.da_[i] / a);
        Check(std::isfinite(result.value_), "CalibrationNumericalFailure", "/measure", "finite Scalar value", "", d);
        for (double derivative : result.derivatives_)
            Check(std::isfinite(derivative), "CalibrationNumericalFailure", "/derivatives", "finite quotient partials", "", d);
        return result;
    }
    double PreparedCalibrationInstrument_::Price(const Vector_<>& discounts) const {
        auto d = Diagnostic(*instrument_, measure_.id_);
        d.stage_ = CalibrationStage_::Value_::Evaluate;
        Check(capability_.affine_, "UnsupportedCalibrationMeasure", "/measure_id", "deterministic cashflow Price", "", d);
        Discounts(discounts, cashflows_.size(), d);
        double result = 0.;
        for (size_t i = 0; i < cashflows_.size(); ++i)
            result += *cashflows_[i].amount_ * discounts[i];
        Check(std::isfinite(result), "CalibrationNumericalFailure", "/price", "finite cashflow Price", "", d);
        return result;
    }
} // namespace Dal

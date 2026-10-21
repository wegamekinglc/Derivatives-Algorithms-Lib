#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>

#include <dal/calibration/validation_internal.hpp>
#include <dal/indice/index/ir.hpp>
#include <dal/model/gsrmultidata.hpp>
#include <dal/script/calibrationpreparation.hpp>
#include <dal/script/event.hpp>
#include <dal/storage/json.hpp>

namespace Dal {
#include <dal/auto/MG_CalibrationBackend_enum.inc>
#include <dal/auto/MG_CalibrationDerivative_enum.inc>
#include <dal/auto/MG_CalibrationEvaluation_enum.inc>

    using namespace CalibrationInternal;
    using namespace Script;
    namespace {
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
        void GuardAssignment(const Node_& node, bool paid, const String_& receiver, const CalibrationDiagnostic_& d) {
            if (dynamic_cast<const NodeAssign_*>(&node) && Receiver(*node.arguments_[0], receiver))
                Check(!paid && Literal(*node.arguments_[1], 0.), "InvalidCalibrationOutputBinding", "/source/rows",
                      "literal-zero receiver initialization before every historical or live PAYS", "", d);
            if (const auto* vector = dynamic_cast<const NodeVectorAssign_*>(&node)) {
                const auto* entry = dynamic_cast<const NodeVectorEntry_*>(vector->arguments_[0].get());
                Check(!entry || entry->name_ != receiver, "InvalidCalibrationOutputBinding", "/source/rows", "scalar Price receiver", "", d);
            }
        }
        bool Guard(const Node_& node, bool paid, const String_& receiver, const CalibrationDiagnostic_& d) {
            const auto visit = [&](size_t first, size_t last, bool state) {
                for (size_t i = first; i < last; ++i)
                    state = Guard(*node.arguments_[i], state, receiver, d);
                return state;
            };
            if (const auto* branch = dynamic_cast<const NodeIf_*>(&node)) {
                const auto firstElse = branch->LastTrueIndex() + 1;
                const bool thenPaid = visit(1, firstElse, paid);
                const bool elsePaid = visit(firstElse, node.arguments_.size(), paid);
                return thenPaid || elsePaid;
            }
            if (dynamic_cast<const NodePays_*>(&node)) {
                Check(Receiver(*node.arguments_[0], receiver), "InvalidCalibrationOutputBinding", "/source/rows", "one declared PAYS receiver", "",
                      d);
                return true;
            }
            GuardAssignment(node, paid, receiver, d);
            return visit(0, node.arguments_.size(), paid);
        }
        struct Payment_ {
            const NodePays_* node_;
            Date_ date_;
            CalibrationDiagnostic_ source_;
        };
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
                    action(*product.Events()[e][s], d);
                }
            }
        }
        Vector_<Payment_> Payments(const ScriptProduct_& product, const String_& receiver, const CalibrationDiagnostic_& d, bool affine) {
            bool paid = false;
            ForStatements(product, d, [&](const Node_& node, const auto& source) { paid = Guard(node, paid, receiver, source); });
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
        CalibrationPreparationCapability_ AdmitMonteCarlo(bool deposit,
                                                          const CalibrationMeasureSpec_& measure,
                                                          const CalibrationPreparationContext_& context,
                                                          const CalibrationEvaluationOptions_& options,
                                                          const CalibrationDiagnostic_& d) {
            Check(options.allowMonteCarloCalibration_, "UnsupportedFastEvaluator", "/options/allow_monte_carlo_calibration",
                  "explicit MC calibration opt-in", "false", d);
            Check(measure.kind_ == CalibrationMeasureKind_::Value_::Price, "UnsupportedCalibrationMeasure", "/measure_id",
                  "shared Price; pathwise par ratios are not supported", "", d);
            GaussianContext(context, d);
            Check(options.sampling_ && options.sampling_->paths_ >= 2 && options.sampling_->seed_ >= 0, "InvalidCalibrationPlan", "/options/sampling",
                  "explicit paths >= 2 and nonnegative seed", "", d);
            Check(Bytes(options.sampling_->generator_) == "irn", "InvalidCalibrationPlan", "/options/sampling/generator", "irn", "", d);
            Check(options.derivativeMethod_ == CalibrationDerivative_::Value_::Auto ||
                      options.derivativeMethod_ == CalibrationDerivative_::Value_::None,
                  "DerivativeUnavailable", "/options/derivative_method", "None for MC preparation admission", "", d);
            return {deposit ? "dal.deposit-origination/1" : "dal.gaussian-bond-option/1", CalibrationBackend_::Value_::ScriptMC,
                    CalibrationDerivative_::Value_::None, true, deposit};
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
        const auto payments = Payments(*product, spec.header_.receiver_, d, found->kind_ != CalibrationMeasureKind_::Value_::Price);
        if (found->kind_ != CalibrationMeasureKind_::Value_::Price)
            for (const auto& payment : payments)
                static_cast<void>(Affine(*payment.node_->arguments_[1], payment.source_));
        const bool deposit = IsDeposit(payments);
        for (const auto& payment : payments)
            Check(!payment.node_->paymentDate_, "UnsupportedFastEvaluator", "/source/rows", "AtEvent payment without ON clause", "", payment.source_);
        if (deposit) {
            Check(*context.valuation_.evaluationDate_ == payments[0].date_, "UnsupportedCalibrationMeasure", "/context/valuation_date",
                  "Deposit origination valuation", "", d);
            const CalibrationDepositDescriptor_ descriptor{payments[0].date_, payments[1].date_, Bound(*instrument, "N"), Bound(*instrument, "K"),
                                                           Bound(*instrument, "ALPHA")};
            result.descriptor_ = descriptor;
            product->IndexVariables();
            for (const auto& payment : payments) {
                const auto coefficients = Affine(*payment.node_->arguments_[1], payment.source_);
                result.cashflows_.push_back({payment.date_, PaymentAmount(*product, *payment.node_), coefficients.a_, coefficients.b_,
                                             *payment.source_.sourceRow_, *payment.source_.statement_});
            }
        } else {
            const auto option = BondOption(payments, *instrument, context, d);
            Check(static_cast<bool>(option), "UnsupportedFastEvaluator", "/source/rows", "complete Deposit or BondOption P0 AST", "", d);
            GaussianContext(context, d);
            result.descriptor_ = *option;
            result.cashflows_.push_back(
                {option->expiry_, {}, 0., 0., *payments[0].source_.sourceRow_, *payments[0].source_.statement_, option->index_, option->expiry_});
        }
        result.capability_ = Admit(deposit, result.measure_, context, options, d);
        if (result.capability_.backend_ == CalibrationBackend_::Value_::ScriptMC)
            result.sampling_ = options.sampling_;
        result.compilerIdentity_ = Fingerprint(Obj({{"build", Str("dal.core/1")},
                                                    {"parser", Str("typed-slots/1")},
                                                    {"lowering", Str("p0-cashflows/1")},
                                                    {"matcher", Str("p0-ast/1")},
                                                    {"constant_binding", Str("effective-slots/1")},
                                                    {"receiver_guard", Str("whole-history/1")}}));
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
        Check(Bytes(Dal::ContextIdentity(context, d)) == Bytes(contextIdentity_), "CalibrationContextMismatch", "/context",
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

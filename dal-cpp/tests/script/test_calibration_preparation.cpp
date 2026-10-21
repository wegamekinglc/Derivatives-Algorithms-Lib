#include <gtest/gtest.h>

#include <limits>

#include <dal/platform/platform.hpp>

#include <dal/math/distribution/black.hpp>
#include <dal/model/gsreuropean.hpp>
#include <dal/model/gsrmultidata.hpp>
#include <dal/script/calibrationpreparation.hpp>
#include <dal/script/event.hpp>

using namespace Dal;

namespace {
    struct NamedImpostorModel_ final : ModelData_ {
        NamedImpostorModel_() : ModelData_("MultiFactorGSRModelData", "model") {}
        void Write(Archive::Store_&) const override { THROW("impostor serializer must not be called"); }
        std::unique_ptr<ModelData_> MutantModel(const String_*, const Slide_*) const override { THROW("unsupported model"); }
    };
    ScriptInstrumentDefinitionSpec_ DepositSpec() {
        const auto amount = CalibrationValueUnit_::Parse("Price:USD");
        ScriptInstrumentDefinitionSpec_ spec;
        spec.source_.rows_ = {{ScriptSourceRowKind_::Value_::Definition, "N", "100"},
                              {ScriptSourceRowKind_::Value_::Definition, "K", "0.025"},
                              {ScriptSourceRowKind_::Value_::Definition, "ALPHA", "1.2"},
                              {ScriptSourceRowKind_::Value_::Event, "2026-10-21", "pay PAYS -N"},
                              {ScriptSourceRowKind_::Value_::Event, "2027-10-21", "pay PAYS N*(1+K*ALPHA)"}};
        spec.header_.payCurrency_ = spec.header_.resultCurrency_ = spec.header_.collateralCurrency_ = Ccy_("USD");
        spec.header_.discountRoute_ = "USD.OIS";
        spec.parameters_ = {
            {"N", ScriptParameterType_::Value_::Real, amount, ScriptParameterRole_::Value_::Notional, true, 0., {}, false, true},
            {"K", ScriptParameterType_::Value_::Real, CalibrationValueUnit_::Parse("Rate"), ScriptParameterRole_::Value_::Coupon, true},
            {"ALPHA",
             ScriptParameterType_::Value_::Real,
             CalibrationValueUnit_::Parse("YearFraction"),
             ScriptParameterRole_::Value_::Accrual,
             false,
             0.,
             {},
             false,
             true}};
        spec.measures_ = {
            {"Price", CalibrationMeasureKind_::Value_::Price, "PAY", {}, amount, {}, {}},
            {"ParRate", CalibrationMeasureKind_::Value_::ParAffineRate, "PAY", String_("K"), CalibrationValueUnit_::Parse("Rate"), 1, 1e-12}};
        return spec;
    }
    Handle_<CalibrationInstrument_> Instrument(const ScriptInstrumentDefinitionSpec_& spec = DepositSpec()) {
        return NewScriptCalibrationInstrument({"deposit", NewScriptInstrumentDefinition(spec)});
    }
    CalibrationPreparationContext_ Context() {
        CalibrationPreparationContext_ result;
        result.valuation_.evaluationDate_ = Date_(2026, 10, 21);
        result.valuation_.fixings_.reset(new MarketFixingSnapshot_());
        result.currency_ = Ccy_("USD");
        result.discountRoute_ = "USD.OIS";
        result.curve_.reset(new GSRCurveData_("curve", *result.valuation_.evaluationDate_, "USD",
                                              {Date_(2026, 10, 21), Date_(2027, 10, 21), Date_(2028, 10, 21)}, {0., -0.04, -0.08}, {}, {}));
        return result;
    }
    template <class F_> void Failure(const F_& action, const String_& code) {
        try {
            action();
            FAIL() << "expected " << code;
        } catch (const CalibrationError_& error) {
            ASSERT_EQ(error.Failure().diagnostic_.code_, code) << error.what();
            ASSERT_TRUE(error.Failure().report_);
            ASSERT_EQ(error.Failure().report_->Errors().size(), 1);
        }
    }
    ScriptInstrumentDefinitionSpec_ BondOptionSpec(bool call = true) {
        auto spec = DepositSpec();
        spec.source_.rows_ = {{ScriptSourceRowKind_::Value_::Definition, "N", "100"},
                              {ScriptSourceRowKind_::Value_::Definition, "K", "0.97"},
                              {ScriptSourceRowKind_::Value_::Event, "2027-10-21",
                               call ? "pay PAYS N*MAX(FIX(IR[USD,DF,2028-10-21])-K,0)" : "pay PAYS N*MAX(K-FIX(IR[USD,DF,2028-10-21]),0)"}};
        spec.parameters_.pop_back();
        spec.parameters_[1] = {"K",
                               ScriptParameterType_::Value_::Real,
                               CalibrationValueUnit_::Parse("BondPrice"),
                               ScriptParameterRole_::Value_::Strike,
                               true,
                               0.,
                               {},
                               false,
                               true};
        spec.measures_.pop_back();
        return spec;
    }
    CalibrationPreparationContext_ GaussianContext() {
        auto result = Context();
        const Date_ v = *result.valuation_.evaluationDate_;
        Handle_<MultiFactorGSRVolData_> vol(
            new MultiFactorGSRVolData_("vol", {"f"}, {v}, Matrix_<>(1, 1, 0.02), {v}, Matrix_<>(1, 1, 1.), Matrix_<>(1, 1, 1.)));
        result.model_.reset(new MultiFactorGSRModelData_("model", result.curve_, vol));
        return result;
    }
} // namespace

TEST(CalibrationPreparationTest, TestAffineCoefficientsAndIndependentQuotientOracle) {
    const auto plan = PrepareCalibrationInstrument(Instrument(), "ParRate", Context());
    ASSERT_EQ(plan.Capability().backend_, CalibrationBackend_::Value_::FastDeterministic);
    ASSERT_EQ(plan.Cashflows().size(), 2);
    const double p = 0.94;
    Matrix_<> derivatives(2, 1, 0.);
    derivatives(1, 0) = p;
    const auto coefficients = plan.Coefficients({1., p}, derivatives);
    ASSERT_NEAR(coefficients.a_, 120. * p, 1e-10);
    ASSERT_NEAR(coefficients.b_, 100. * (p - 1.), 1e-10);
    ASSERT_NEAR(coefficients.da_[0], 120. * p, 1e-10);
    ASSERT_NEAR(coefficients.db_[0], 100. * p, 1e-10);
    const auto par = plan.AffineMeasure(coefficients);
    ASSERT_NEAR(par.value_, (1. / p - 1.) / 1.2, 1e-10);
    ASSERT_NEAR(par.derivatives_[0], -1. / (1.2 * p), 1e-10);
    const auto price = plan.Price({1., p});
    ASSERT_NEAR(price, -100. + 100. * (1. + 0.025 * 1.2) * p, 1e-10);
    ASSERT_NEAR(price, coefficients.b_ + 0.025 * coefficients.a_, 1e-10);
}

TEST(CalibrationPreparationTest, TestReceiverGuardPrecedesTemplateMatchingAcrossHistoryAndBranches) {
    auto spec = DepositSpec();
    spec.source_.rows_[3].text_ = "IF K > 0 THEN pay PAYS -N ELSE pay = 0 END";
    spec.source_.rows_[4].text_ = "pay = 0 pay PAYS N*(1+K*ALPHA)";
    auto context = Context();
    Failure([&] { PrepareCalibrationInstrument(Instrument(spec), "Price", context); }, "InvalidCalibrationOutputBinding");
    spec.source_.rows_[3].text_ = "pay PAYS -N";
    spec.source_.rows_[3].key_ = "2026-10-20";
    Failure([&] { PrepareCalibrationInstrument(Instrument(spec), "Price", context); }, "InvalidCalibrationOutputBinding");
    spec.source_.rows_[3].text_ = "pay = N pay PAYS -N";
    Failure([&] { PrepareCalibrationInstrument(Instrument(spec), "Price", context); }, "InvalidCalibrationOutputBinding");
}

TEST(CalibrationPreparationTest, TestBondOptionDescriptorUsesSameSourceForCallAndPut) {
    for (bool call : {true, false}) {
        const auto instrument = Instrument(BondOptionSpec(call));
        const auto plan = PrepareCalibrationInstrument(instrument, "Price", GaussianContext());
        ASSERT_EQ(plan.Capability().backend_, CalibrationBackend_::Value_::FastGaussian);
        ASSERT_EQ(plan.Capability().derivativeMethod_, CalibrationDerivative_::Value_::FiniteDifference);
        ASSERT_FALSE(plan.Capability().affine_);
        const auto& descriptor = std::get<CalibrationBondOptionDescriptor_>(plan.Descriptor());
        ASSERT_EQ(descriptor.expiry_, Date_(2027, 10, 21));
        ASSERT_EQ(descriptor.maturity_, Date_(2028, 10, 21));
        ASSERT_EQ(descriptor.notional_, 100.);
        ASSERT_EQ(descriptor.strike_, 0.97);
        ASSERT_EQ(descriptor.call_, call);
    }
}

TEST(CalibrationPreparationTest, TestExplicitMcRequiresOptInSamplingAndSharedPrice) {
    const auto instrument = Instrument(BondOptionSpec());
    const auto context = GaussianContext();
    CalibrationEvaluationOptions_ options;
    options.evaluation_ = CalibrationEvaluation_::Value_::ScriptMC;
    options.derivativeMethod_ = CalibrationDerivative_::Value_::None;
    Failure([&] { PrepareCalibrationInstrument(instrument, "Price", context, options); }, "UnsupportedFastEvaluator");
    options.allowMonteCarloCalibration_ = true;
    Failure([&] { PrepareCalibrationInstrument(instrument, "Price", context, options); }, "InvalidCalibrationPlan");
    options.sampling_ = CalibrationSamplingSpec_{64, 42, "irn"};
    const auto first = PrepareCalibrationInstrument(instrument, "Price", context, options);
    ASSERT_EQ(first.Capability().backend_, CalibrationBackend_::Value_::ScriptMC);
    options.sampling_->seed_ = 43;
    const auto second = PrepareCalibrationInstrument(instrument, "Price", context, options);
    ASSERT_NE(first.PlanIdentity(), second.PlanIdentity());
    Failure([&] { PrepareCalibrationInstrument(Instrument(), "ParRate", GaussianContext(), options); }, "UnsupportedCalibrationMeasure");
    options.sampling_->paths_ = 0;
    Failure([&] { PrepareCalibrationInstrument(instrument, "Price", context, options); }, "InvalidCalibrationPlan");
}

TEST(CalibrationPreparationTest, TestCoefficientOverflowFailsWithOwnedEvaluateReport) {
    const auto plan = PrepareCalibrationInstrument(Instrument(), "ParRate", Context());
    Failure([&] { static_cast<void>(plan.Coefficients({1., std::numeric_limits<double>::max()})); }, "CalibrationNumericalFailure");
}

TEST(CalibrationPreparationTest, TestVectorReceiverIsRejectedBeforeLowering) {
    auto spec = DepositSpec();
    spec.source_.rows_[3].text_ = "pay[0] = 0 pay PAYS -N";
    Failure([&] { PrepareCalibrationInstrument(Instrument(spec), "Price", Context()); }, "InvalidCalibrationOutputBinding");
}

TEST(CalibrationPreparationTest, TestBondOptionCashflowIrDistinguishesContingentAmount) {
    const auto plan = PrepareCalibrationInstrument(Instrument(BondOptionSpec()), "Price", GaussianContext());
    ASSERT_EQ(plan.Cashflows().size(), 1);
    ASSERT_EQ(plan.Cashflows()[0].eventDate_, Date_(2027, 10, 21));
    ASSERT_FALSE(plan.Cashflows()[0].amount_);
    ASSERT_EQ(plan.Cashflows()[0].fixingDate_, std::optional<Date_>(Date_(2027, 10, 21)));
    ASSERT_FALSE(plan.Cashflows()[0].observationIndex_.empty());
}

TEST(CalibrationPreparationTest, TestNonlinearCouponAndParameterBranchAreRejected) {
    for (const String_ expression : {"N*(1+K*K*ALPHA)", "N/(1+K)", "N*(1+(K-K)*K*ALPHA)"}) {
        auto spec = DepositSpec();
        spec.source_.rows_[4].text_ = "pay PAYS " + expression;
        Failure([&] { PrepareCalibrationInstrument(Instrument(spec), "ParRate", Context()); }, "UnsupportedCalibrationMeasure");
    }
    auto spec = DepositSpec();
    spec.source_.rows_[4].text_ = "IF K > 0 THEN pay PAYS N*(1+K*ALPHA) ELSE pay PAYS N END";
    Failure([&] { PrepareCalibrationInstrument(Instrument(spec), "ParRate", Context()); }, "UnsupportedCalibrationMeasure");
}

TEST(CalibrationPreparationTest, TestUnstableAnnuityAndCoefficientPartialValidation) {
    const auto plan = PrepareCalibrationInstrument(Instrument(), "ParRate", Context());
    for (double a : {0., 1e-13, -1., std::numeric_limits<double>::infinity()})
        Failure([&] { static_cast<void>(plan.AffineMeasure({a, 1., {}, {}})); }, "InvalidCalibrationAnnuity");
    Failure([&] { static_cast<void>(plan.AffineMeasure({1., 1., {1.}, {}})); }, "InvalidCalibrationPlan");
    Failure([&] { static_cast<void>(plan.Coefficients({1.})); }, "InvalidCalibrationPlan");
    Failure([&] { static_cast<void>(plan.Coefficients({1., -0.5})); }, "CalibrationNumericalFailure");
    auto c = plan.Coefficients({1., 0.95});
    c.da_ = {std::numeric_limits<double>::quiet_NaN()};
    c.db_ = {0.};
    Failure([&] { static_cast<void>(plan.AffineMeasure(c)); }, "CalibrationNumericalFailure");
}

TEST(CalibrationPreparationTest, TestOriginalTreeAndCompiledPriceMatchCashflowIrAfterRebinding) {
    auto spec = DepositSpec();
    spec.source_.rows_[3].text_ = "pay = 0 pay PAYS -N";
    const auto original = Instrument(spec);
    for (double coupon : {-0.01, 0.025, 0.08}) {
        const auto instrument = BindCalibrationInstrumentParameters(
            original, {{{"K", coupon, CalibrationValueUnit_::Parse("Rate")}, {"N", 250., CalibrationValueUnit_::Parse("Price:USD")}}, {}});
        const auto plan = PrepareCalibrationInstrument(instrument, "ParRate", Context());
        auto product = instrument->Product();
        product->PartitionEvents(*Context().valuation_.evaluationDate_);
        product->PreProcess(false, true);
        AAD::Scenario_<> scenario(2);
        scenario[0].numeraire_ = 1.;
        scenario[1].numeraire_ = 1. / 0.93;
        auto evaluator = product->BuildEvaluator<double>();
        product->Evaluate(scenario, evaluator);
        auto state = product->BuildEvalState<double>();
        product->Compile().Evaluate(scenario, state);
        const double oracle = 250. * ((1. + coupon * 1.2) * 0.93 - 1.);
        ASSERT_NEAR(evaluator.VarVals()[product->PayOffIdx()], oracle, 1e-10);
        ASSERT_NEAR(state.VarVals()[product->PayOffIdx()], oracle, 1e-10);
        ASSERT_NEAR(plan.Price({1., 0.93}), oracle, 1e-10);
        ASSERT_NEAR(plan.AffineMeasure(plan.Coefficients({1., 0.93})).value_, (1. / 0.93 - 1.) / 1.2, 1e-10);
    }
    ASSERT_EQ(original->Binding()[0].value_, 100.);
    ASSERT_EQ(original->Binding()[1].value_, 0.025);
}

TEST(CalibrationPreparationTest, TestFullAstMatcherRejectsFeeLagAndExtraReceiver) {
    for (const String_ suffix : {" pay PAYS 1", " pay PAYS 0", " other PAYS 1"}) {
        auto spec = DepositSpec();
        spec.source_.rows_[4].text_ += suffix;
        const auto code = suffix == " other PAYS 1" ? "InvalidCalibrationOutputBinding" : "UnsupportedFastEvaluator";
        Failure([&] { PrepareCalibrationInstrument(Instrument(spec), "Price", Context()); }, code);
    }
    auto spec = DepositSpec();
    spec.source_.rows_[4].text_ += " ON 2027-10-22";
    Failure([&] { PrepareCalibrationInstrument(Instrument(spec), "Price", Context()); }, "UnsupportedFastEvaluator");
}

TEST(CalibrationPreparationTest, TestMethodsAndDerivativeAdmissionAreIndependent) {
    CalibrationEvaluationOptions_ options;
    options.evaluation_ = CalibrationEvaluation_::Value_::GenericDeterministic;
    const auto generic = PrepareCalibrationInstrument(Instrument(), "Price", Context(), options);
    ASSERT_EQ(generic.Capability().backend_, CalibrationBackend_::Value_::GenericDeterministic);
    Failure([&] { PrepareCalibrationInstrument(Instrument(BondOptionSpec()), "Price", GaussianContext(), options); }, "UnsupportedFastEvaluator");
    options.evaluation_ = CalibrationEvaluation_::Value_::RequireFast;
    options.derivativeMethod_ = CalibrationDerivative_::Value_::AAD;
    Failure([&] { PrepareCalibrationInstrument(Instrument(), "Price", Context(), options); }, "DerivativeUnavailable");
    Failure([&] { PrepareCalibrationInstrument(Instrument(BondOptionSpec()), "Price", GaussianContext(), options); }, "DerivativeUnavailable");
    options.derivativeMethod_ = CalibrationDerivative_::Value_::None;
    ASSERT_EQ(PrepareCalibrationInstrument(Instrument(), "Price", Context(), options).Capability().derivativeMethod_,
              CalibrationDerivative_::Value_::None);
}

TEST(CalibrationPreparationTest, TestGaussianDomainRejectsCurrencyFixingModelAndSmoothingChanges) {
    Failure([&] { PrepareCalibrationInstrument(Instrument(BondOptionSpec()), "Price", Context()); }, "UnsupportedFastEvaluator");
    auto context = GaussianContext();
    context.smoothing_ = 0.01;
    Failure([&] { PrepareCalibrationInstrument(Instrument(BondOptionSpec()), "Price", context); }, "UnsupportedFastEvaluator");
    context = GaussianContext();
    context.curve_.reset(
        new GSRCurveData_("other", *context.valuation_.evaluationDate_, "USD", context.curve_->nodeDates_, {0., -0.03, -0.06}, {}, {}));
    Failure([&] { PrepareCalibrationInstrument(Instrument(BondOptionSpec()), "Price", context); }, "CalibrationContextMismatch");
    auto spec = BondOptionSpec();
    spec.source_.rows_[2].text_ = "pay PAYS N*MAX(FIX(IR[EUR,DF,2028-10-21])-K,0)";
    Failure([&] { PrepareCalibrationInstrument(Instrument(spec), "Price", GaussianContext()); }, "CalibrationContextMismatch");
    spec.source_.rows_[2].text_ = "pay PAYS N*MAX(FIX(IR[USD,DF,2028-10-21],2027-10-20)-K,0)";
    Failure([&] { PrepareCalibrationInstrument(Instrument(spec), "Price", GaussianContext()); }, "UnsupportedFastEvaluator");
    spec.source_.rows_[2].text_ = "pay PAYS N*MAX(FIX(IR[USD,DF,1Y])-K,0)";
    Failure([&] { PrepareCalibrationInstrument(Instrument(spec), "Price", GaussianContext()); }, "UnsupportedFastEvaluator");
}

TEST(CalibrationPreparationTest, TestGaussianDescriptorMatchesIndependentBlackOracle) {
    const auto context = GaussianContext();
    const auto plan = PrepareCalibrationInstrument(Instrument(BondOptionSpec()), "Price", context);
    const auto& d = std::get<CalibrationBondOptionDescriptor_>(plan.Descriptor());
    const auto& model = static_cast<const MultiFactorGSRModelData_&>(*context.model_);
    const double value = d.notional_ * PriceGSREuropeanOption(model, BondOption_{d.expiry_, d.maturity_, d.strike_, OptionType_("CALL")}).price_;
    const double volatility = 0.02 * (Date_(2028, 10, 21) - Date_(2027, 10, 21)) / 365.;
    const double oracle = 100. * std::exp(-0.04) * Distribution::BlackOpt(std::exp(-0.04), volatility, 0.97, OptionType_("CALL"));
    ASSERT_NEAR(value, oracle, 1e-10);
}

TEST(CalibrationPreparationTest, TestBindingSourceHeaderAndContextInvalidatePreparedIdentity) {
    const auto instrument = Instrument();
    const auto context = Context();
    const auto plan = PrepareCalibrationInstrument(instrument, "Price", context);
    ASSERT_NO_THROW(plan.RequireCompatible(instrument, Context()));
    const auto rebound = BindCalibrationInstrumentParameters(instrument, {{{"N", 500., CalibrationValueUnit_::Parse("Price:USD")}}, {}});
    Failure([&] { plan.RequireCompatible(rebound, context); }, "InvalidCalibrationPlan");
    auto spec = DepositSpec();
    spec.source_.rows_[4].text_ += " ";
    Failure([&] { plan.RequireCompatible(Instrument(spec), context); }, "InvalidCalibrationPlan");
    ASSERT_NE(plan.PlanIdentity(), PrepareCalibrationInstrument(Instrument(spec), "Price", context).PlanIdentity());
    spec = DepositSpec();
    spec.header_.discountRoute_ = "USD.OTHER";
    auto changed = context;
    changed.discountRoute_ = "USD.OTHER";
    ASSERT_NE(plan.PlanIdentity(), PrepareCalibrationInstrument(Instrument(spec), "Price", changed).PlanIdentity());
    Failure([&] { plan.RequireCompatible(instrument, changed); }, "CalibrationContextMismatch");
    changed = context;
    changed.smoothing_ = 0.1;
    Failure([&] { plan.RequireCompatible(instrument, changed); }, "CalibrationContextMismatch");
    changed = context;
    changed.conventionState_ = "frozen-conventions/2";
    Failure([&] { plan.RequireCompatible(instrument, changed); }, "CalibrationContextMismatch");
}

TEST(CalibrationPreparationTest, TestFixingsAndMutableModelContentInvalidateContext) {
    auto context = GaussianContext();
    const auto instrument = Instrument(BondOptionSpec());
    const auto plan = PrepareCalibrationInstrument(instrument, "Price", context);
    context.valuation_.fixings_.reset(new MarketFixingSnapshot_({{"FX:USD/EUR", {{DateTime_(Date_(2026, 10, 20), 0.25), 1.1}}}}));
    Failure([&] { plan.RequireCompatible(instrument, context); }, "CalibrationContextMismatch");
    context = GaussianContext();
    const auto& previous = static_cast<const MultiFactorGSRModelData_&>(*context.model_);
    const auto& v = *previous.vol_;
    auto vol = std::make_shared<MultiFactorGSRVolData_>("vol", v.factorNames_, v.gKnotDates_, v.gValues_, v.hKnotDates_, v.hValues_, v.correlations_);
    auto model = std::make_shared<MultiFactorGSRModelData_>("model", previous.curve_, Handle_<MultiFactorGSRVolData_>(vol));
    context.model_ = Handle_<ModelData_>(model);
    ASSERT_NO_THROW(plan.RequireCompatible(instrument, context));
    vol->gValues_(0, 0) = 0.03;
    Failure([&] { plan.RequireCompatible(instrument, context); }, "CalibrationContextMismatch");
    ASSERT_NE(plan.ContextIdentity(), PrepareCalibrationInstrument(instrument, "Price", context).ContextIdentity());
}

TEST(CalibrationPreparationTest, TestPureScalarAndUnknownScriptsNeverAcquireMcCapability) {
    auto spec = DepositSpec();
    spec.source_.rows_[3].text_ = "x = -N";
    spec.source_.rows_[4].text_ = "pay = (1+K*ALPHA)";
    Failure([&] { PrepareCalibrationInstrument(Instrument(spec), "Price", Context()); }, "NoSharedPriceDefinition");
    spec = DepositSpec();
    spec.source_.rows_[4].text_ = "pay PAYS N*EXP(K*ALPHA)";
    CalibrationEvaluationOptions_ options;
    options.allowMonteCarloCalibration_ = true;
    options.sampling_ = CalibrationSamplingSpec_{100, 1, "irn"};
    Failure([&] { PrepareCalibrationInstrument(Instrument(spec), "Price", GaussianContext(), options); }, "UnsupportedFastEvaluator");
}

TEST(CalibrationPreparationTest, TestModelNameCannotBypassStructuralAdmissionOrLeakLegacyError) {
    auto context = Context();
    context.model_.reset(new NamedImpostorModel_());
    Failure([&] { PrepareCalibrationInstrument(Instrument(BondOptionSpec()), "Price", context); }, "UnsupportedFastEvaluator");
}

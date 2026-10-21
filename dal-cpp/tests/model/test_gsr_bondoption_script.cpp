#include <gtest/gtest.h>

#include <cmath>
#include <limits>

#include <dal/model/gsr.hpp>
#include <dal/model/gsrbondoption.hpp>
#include <dal/model/gsrslvdata.hpp>
#include <dal/script/calibrationmc.hpp>
#include <dal/script/preparation.hpp>
#include <dal/time/dateutils.hpp>

using namespace Dal;

namespace {
    const Date_ V(2026, 10, 21), E(2027, 10, 21), T(2028, 10, 21);

    CalibrationPreparationContext_ Context(double g = 0.02) {
        CalibrationPreparationContext_ c;
        c.currency_ = Ccy_("USD");
        c.discountRoute_ = "USD.OIS";
        c.valuation_.evaluationDate_ = V;
        c.valuation_.fixings_.reset(new MarketFixingSnapshot_());
        c.curve_.reset(new GSRCurveData_("curve", V, "USD", {V, E, T}, {0., -0.04, -0.08}, {}, {}));
        Handle_<MultiFactorGSRVolData_> vol(
            new MultiFactorGSRVolData_("vol", {"level"}, {V}, Matrix_<>(1, 1, g), {V}, Matrix_<>(1, 1, 1.), Matrix_<>(1, 1, 1.)));
        c.model_.reset(new MultiFactorGSRModelData_("model", c.curve_, vol));
        return c;
    }
    Handle_<CalibrationInstrument_> Instrument(bool call = true, double strike = 0.97, double notional = 100.) {
        return NewBondOptionScriptInstrument({E, T, strike, OptionType_(call ? "CALL" : "PUT")}, {"bond", Ccy_("USD"), "USD.OIS", notional});
    }
    double Oracle(double g, bool call = true, double strike = 0.97, double notional = 100.) {
        const double sigma = g * (366. / 365.);
        const double direction = call ? 1. : -1.;
        const double forward = std::exp(-0.04);
        if (sigma == 0.)
            return notional * std::exp(-0.04) * std::max(direction * (forward - strike), 0.);
        const double d1 = std::log(forward / strike) / sigma + 0.5 * sigma, d2 = d1 - sigma;
        const auto cdf = [](double x) { return 0.5 * std::erfc(-x / std::sqrt(2.)); };
        return notional * direction * (std::exp(-0.08) * cdf(direction * d1) - strike * std::exp(-0.04) * cdf(direction * d2));
    }
    double Fast(const Handle_<CalibrationInstrument_>& instrument, const CalibrationPreparationContext_& context) {
        const auto plan = PrepareCalibrationInstrument(instrument, "Price", context);
        return PriceGaussianBondOption(plan, instrument, context).price_;
    }
    Handle_<CalibrationObservation_> Observation(const Handle_<CalibrationInstrument_>& instrument, double target, double scale = 1e-6) {
        return NewCalibrationObservation("fit", instrument, "Price",
                                         NewCalibrationMarketQuote("market", target, CalibrationValueUnit_::Parse("Price:USD")), scale);
    }
    CalibrationMcSettings_ McSettings(bool aad = true, bool compiled = true) {
        CalibrationMcSettings_ s;
        s.allowMonteCarloCalibration_ = true;
        s.aad_ = aad;
        s.compiled_ = compiled;
        s.seed_ = 1024;
        s.fitBatches_ = 8;
        s.fitPathsPerBatch_ = 8192;
        s.validationBatches_ = 16;
        s.validationPathsPerBatch_ = 16384;
        s.maxPathsPerView_ = 327680;
        if (aad)
            s.parameterLabels_ = {"g:level:2026-10-21"};
        return s;
    }
    CalibrationMcPrecision_ Precision() {
        CalibrationMcPrecision_ p;
        p.errorScale_ = 0.1;
        p.parameterScales_ = {0.01};
        p.epsilonGradient_ = {1e-8};
        return p;
    }
    double Vega(double g) {
        const double tau = 366. / 365., sigma = g * tau;
        const double d1 = std::log(std::exp(-0.04) / 0.97) / sigma + 0.5 * sigma;
        return 100. * std::exp(-0.08) * std::exp(-0.5 * d1 * d1) / std::sqrt(2. * std::acos(-1.)) * tau;
    }
    CalibrationMcReference_ Reference(const Handle_<CalibrationInstrument_>& instrument, double g) {
        CalibrationMcReference_ r;
        r.value_ = Fast(instrument, Context(g));
        const auto fd = [&](double h) { return (Fast(instrument, Context(g + h)) - Fast(instrument, Context(g - h))) / (2. * h); };
        const double coarse = fd(1e-4), fine = fd(5e-5);
        r.gradient_ = {fine};
        r.gradientStandardError_ = {0.};
        r.finiteDifferenceError_ = {std::abs(fine - coarse)};
        return r;
    }
} // namespace

TEST(GSRBondOptionScriptTest, TestExporterOwnsSourceAndRebinding) {
    BondOption_ native{Date_(2027, 10, 21), Date_(2028, 10, 21), 0.97, OptionType_("PUT")};
    const auto instrument = NewBondOptionScriptInstrument(native, {"bond", Ccy_("USD"), "USD.OIS", 100.});
    native.strike_ = 0.1;
    native.maturity_ = Date_(2030, 1, 1);
    ASSERT_EQ(instrument->Definition()->Spec().source_.rows_.size(), 3);
    ASSERT_EQ(instrument->Definition()->Spec().source_.rows_[2].text_, "PAY PAYS N*MAX(K-FIX(IR[USD,DF,2028-10-21]),0)");
    ASSERT_DOUBLE_EQ(instrument->Binding()[1].value_, 0.97);
    const auto rebound = BindCalibrationInstrumentParameters(instrument, {{{"K", 0.95, CalibrationValueUnit_::Parse("BondPrice")}}});
    ASSERT_DOUBLE_EQ(rebound->Binding()[1].value_, 0.95);
    ASSERT_DOUBLE_EQ(instrument->Binding()[1].value_, 0.97);
    const auto loaded = LoadCalibrationInstrumentJson(DumpCalibrationInstrumentJson(rebound));
    ASSERT_EQ(loaded->InstrumentIdentity(), rebound->InstrumentIdentity());
}

TEST(GSRBondOptionScriptTest, TestFastPriceUsesCompleteBoundDescriptor) {
    for (bool call : {false, true}) {
        const auto instrument = Instrument(call);
        for (double g : {0., 0.012, 0.016, 0.02, 0.0201, 0.0199})
            ASSERT_NEAR(Fast(instrument, Context(g)), Oracle(g, call), 1e-10);
        const auto rebound = BindCalibrationInstrumentParameters(
            instrument, {{{"K", 0.95, CalibrationValueUnit_::Parse("BondPrice")}, {"N", 200., CalibrationValueUnit_::Parse("Price:USD")}}});
        ASSERT_NEAR(Fast(rebound, Context()), Oracle(0.02, call, 0.95, 200.), 1e-10);
        ASSERT_NEAR(Fast(instrument, Context()), Oracle(0.02, call), 1e-10);
    }
}

TEST(GSRBondOptionScriptTest, TestFastCalibrationKeepsInstrumentAndNativeJacobian) {
    const auto instrument = Instrument();
    const auto observation = Observation(instrument, Oracle(0.02));
    const auto result = CalibrateGaussianBondOptions(Context(0.012), {observation}, {{0, 0, 0., 0.1}});
    ASSERT_TRUE(result.converged_) << result.terminationReason_ << " g=" << result.parameters_[0] << " residual=" << result.residuals_[0];
    ASSERT_TRUE(result.fitWithinTolerance_);
    ASSERT_TRUE(result.numericalValidationPassed_);
    ASSERT_NEAR(result.parameters_[0], 0.02, 1e-8);
    ASSERT_EQ(result.axes_.observations_[0]->Instrument().get(), instrument.get());
    ASSERT_TRUE(result.report_);
    ASSERT_EQ(result.report_->Stage(), CalibrationStage_::Value_::Validate);
    ASSERT_EQ(result.report_->Axes().observations_[0]->Instrument().get(), instrument.get());
    ASSERT_EQ(result.parameterLabels_, (Vector_<String_>{"g:level:2026-10-21"}));
    ASSERT_NEAR(result.modelPrices_[0], Oracle(0.02), 1e-6);
    ASSERT_EQ(result.quoteJacobian_.Rows(), 1);
    const double h = 1e-5;
    ASSERT_NEAR(result.quoteJacobian_(0, 0), (Oracle(result.parameters_[0] + h) - Oracle(result.parameters_[0] - h)) / (2. * h), 2e-4);
    ASSERT_GT(result.costs_.fastPrices_, 3);
    ASSERT_EQ(result.costs_.fastPrices_, result.costs_.preparations_);
    ASSERT_EQ(result.costs_.monteCarloPaths_, 0);
}

TEST(GSRBondOptionScriptTest, TestInvalidTargetsKeepOwnedObservationReport) {
    std::optional<CalibrationError_> saved;
    {
        const auto instrument = Instrument();
        try {
            static_cast<void>(CalibrateGaussianBondOptions(Context(), {Observation(instrument, -1.)}, {{0, 0, 0., 0.1}}));
        } catch (const CalibrationError_& error) {
            saved = error;
        }
    }
    ASSERT_TRUE(saved);
    ASSERT_EQ(saved->Failure().diagnostic_.code_, "InvalidCalibrationQuote");
    ASSERT_EQ(saved->Failure().diagnostic_.observationId_, "fit");
    ASSERT_EQ(saved->Failure().report_->Axes().observations_.size(), 1);
    ASSERT_EQ(saved->Failure().report_->Axes().observations_[0]->Quote()->Value(), -1.);
}

TEST(GSRBondOptionScriptTest, TestEmptyRowsFailBeforeSolving) {
    ASSERT_THROW(CalibrateGaussianBondOptions(Context(), {}, {{0, 0, 0., 0.1}}), CalibrationError_);
}

TEST(GSRBondOptionScriptTest, TestNumericalOverflowKeepsOwnedSolverFailure) {
    const auto row = Observation(Instrument(), 1., 1e-300);
    try {
        static_cast<void>(CalibrateGaussianBondOptions(Context(), {row}, {{0, 0, 0., 0.1}}));
        FAIL() << "expected overflow rejection";
    } catch (const CalibrationError_& error) {
        ASSERT_EQ(error.Failure().diagnostic_.code_, "CalibrationNumericalFailure");
        ASSERT_EQ(error.Failure().report_->Stage(), CalibrationStage_::Value_::Solve);
        ASSERT_EQ(error.Failure().report_->Axes().observations_.size(), 1);
    }
}

TEST(GSRBondOptionScriptTest, TestImporterRejectsInvalidFinancialTerms) {
    const BondOption_ valid{E, T, 0.97, OptionType_("CALL")};
    for (double n : {0., -1., std::numeric_limits<double>::infinity()})
        ASSERT_THROW(NewBondOptionScriptInstrument(valid, {"bad", Ccy_("USD"), "USD.OIS", n}), CalibrationError_);
    for (double k : {0., -1., std::numeric_limits<double>::quiet_NaN()}) {
        auto option = valid;
        option.strike_ = k;
        ASSERT_THROW(NewBondOptionScriptInstrument(option, {"bad", Ccy_("USD"), "USD.OIS", 1.}), CalibrationError_);
    }
    ASSERT_THROW(NewBondOptionScriptInstrument({T, E, 0.97, OptionType_("CALL")}, {"bad", Ccy_("USD"), "USD.OIS", 1.}), CalibrationError_);
    ASSERT_THROW(NewBondOptionScriptInstrument({E, T, 0.97, OptionType_("STRADDLE")}, {"bad", Ccy_("USD"), "USD.OIS", 1.}), CalibrationError_);
}

TEST(GSRBondOptionScriptTest, TestImporterRequiresExplicitNotional) {
    ASSERT_THROW(NewBondOptionScriptInstrument({E, T, 0.97, OptionType_("CALL")}, {"missing-n", Ccy_("USD"), "USD.OIS"}), CalibrationError_);
}

TEST(GSRBondOptionScriptTest, TestWholePayoffAndSettlementAreRequired) {
    const auto instrument = Instrument();
    const auto original = instrument->Definition()->Spec();
    for (const String_& text :
         Vector_<String_>{original.source_.rows_.back().text_ + " PAY PAYS 1", original.source_.rows_.back().text_ + " ON 2028-10-21",
                          String_("PAY PAYS N*MAX(FIX(IR[USD,DF,2028-10-21])-K*FIX(IR[USD,DF,2028-10-21]),0)")}) {
        auto changed = original;
        changed.source_.rows_.back().text_ = text;
        const auto invalid = NewScriptCalibrationInstrument({"changed", NewScriptInstrumentDefinition(changed)});
        ASSERT_THROW(Fast(invalid, Context()), CalibrationError_);
    }
}

TEST(GSRBondOptionScriptTest, TestPlansInvalidateBindingCurveModelAndSmoothing) {
    const auto instrument = Instrument();
    const auto c = Context();
    const auto plan = PrepareCalibrationInstrument(instrument, "Price", c);
    auto changed = c;
    changed.smoothing_ = 0.001;
    ASSERT_THROW(PriceGaussianBondOption(plan, instrument, changed), CalibrationError_);
    ASSERT_THROW(Fast(instrument, changed), CalibrationError_);
    changed = Context(0.03);
    ASSERT_THROW(PriceGaussianBondOption(plan, instrument, changed), CalibrationError_);
    changed = c;
    changed.curve_.reset(new GSRCurveData_("different", V, "USD", {V, E, T}, {0., -0.05, -0.08}, {}, {}));
    ASSERT_THROW(Fast(instrument, changed), CalibrationError_);
    changed = c;
    Handle_<GSRLeverageData_> leverage(new GSRLeverageData_("flat", {0.}, {0.}, Matrix_<>(1, 1, 1.)));
    changed.model_.reset(new GSRSLVModelData_("slv", handle_cast<MultiFactorGSRModelData_>(c.model_), leverage, {}));
    ASSERT_THROW(Fast(instrument, changed), CalibrationError_);
    ASSERT_THROW(PriceGaussianBondOption(plan, instrument, changed), CalibrationError_);
    const auto rebound = BindCalibrationInstrumentParameters(instrument, {{{"K", 0.95, CalibrationValueUnit_::Parse("BondPrice")}}});
    ASSERT_THROW(PriceGaussianBondOption(plan, rebound, c), CalibrationError_);
    ASSERT_NEAR(PriceGaussianBondOption(plan, instrument, c).price_, Oracle(0.02), 1e-10);
}

TEST(GSRBondOptionScriptTest, TestLegacyOneFactorUsesSameSource) {
    auto c = Context();
    Handle_<GSRVolData_> vol(new GSRVolData_("vol", {V}, {0.02}, {V}, {1.}));
    c.model_.reset(new GSRModelData_("legacy", c.curve_, vol));
    ASSERT_NEAR(Fast(Instrument(), c), Oracle(0.02), 1e-10);
}

TEST(GSRBondOptionScriptTest, TestBoundsFailureAndNativePenaltyConvention) {
    const auto instrument = Instrument();
    const auto observation = Observation(instrument, Oracle(0.04));
    const auto bounded = CalibrateGaussianBondOptions(Context(0.012), {observation}, {{0, 0, 0., 0.015}});
    ASSERT_TRUE(bounded.converged_);
    ASSERT_FALSE(bounded.fitWithinTolerance_);
    ASSERT_TRUE(bounded.activeBounds_[0]);
    ASSERT_TRUE(bounded.oneSidedColumns_[0]);
    ASSERT_NEAR(bounded.parameters_[0], 0.015, 1e-12);
    GSRCalibrationSettings_ shortRun;
    shortRun.maxIterations_ = 1;
    ASSERT_FALSE(CalibrateGaussianBondOptions(Context(0.012), {observation}, {{0, 0, 0., 0.1}}, shortRun).converged_);
    GSRCalibrationSettings_ prior;
    prior.priorWeight_ = 0.1;
    const auto c = Context(0.012);
    const auto row = Observation(instrument, Oracle(0.02), 0.01);
    const auto script = CalibrateGaussianBondOptions(c, {row}, {{0, 0, 0., 0.1}}, prior);
    const BondOption_ native{E, T, 0.97, OptionType_("CALL")};
    const auto legacy = CalibrateGSRVolatility(static_cast<const MultiFactorGSRModelData_&>(*c.model_),
                                               {{"bond", native, row->Quote()->Value() / 100., row->ErrorScale() / 100.}}, {{0, 0, 0., 0.1}}, prior);
    ASSERT_NEAR(script.parameters_[0], legacy.parameters_[0], 1e-10);
    ASSERT_NEAR(script.quoteJacobian_(0, 0), 100. * legacy.quoteJacobian_(0, 0), 1e-7);
}

TEST(GSRBondOptionScriptTest, TestSelectedKnotFreezesOthersAndPreservesSmoothing) {
    auto c = Context();
    Matrix_<> g(1, 2);
    g(0, 0) = 0.012;
    g(0, 1) = 0.01;
    Handle_<MultiFactorGSRVolData_> vol(
        new MultiFactorGSRVolData_("vol", {"level"}, {V, V.AddDays(180)}, g, {V}, Matrix_<>(1, 1, 1.), Matrix_<>(1, 1, 1.)));
    c.model_.reset(new MultiFactorGSRModelData_("model", c.curve_, vol));
    const double effectiveG = std::sqrt((180. * 0.012 * 0.012 + 185. * 0.025 * 0.025) / 365.);
    const auto row = Observation(Instrument(), Oracle(effectiveG), 0.01);
    const Vector_<GSRCalibrationParameter_> selected{{0, 1, 0., 0.1}};
    const auto plain = CalibrateGaussianBondOptions(c, {row}, selected);
    ASSERT_TRUE(plain.fitWithinTolerance_);
    ASSERT_DOUBLE_EQ(plain.model_->vol_->gValues_(0, 0), 0.012);
    ASSERT_NEAR(plain.parameters_[0], 0.025, 1e-8);
    GSRCalibrationSettings_ smooth;
    smooth.smoothingWeight_ = 0.5;
    const auto script = CalibrateGaussianBondOptions(c, {row}, selected, smooth);
    const auto native =
        CalibrateGSRVolatility(static_cast<const MultiFactorGSRModelData_&>(*c.model_),
                               {{"bond", BondOption_{E, T, 0.97, OptionType_("CALL")}, row->Quote()->Value() / 100., 0.0001}}, selected, smooth);
    ASSERT_NEAR(script.parameters_[0], native.parameters_[0], 1e-9);
    ASSERT_DOUBLE_EQ(script.model_->vol_->gValues_(0, 0), 0.012);
    ASSERT_LT(script.parameters_[0], plain.parameters_[0]);
    ASSERT_EQ(script.quoteJacobian_.Rows(), 1);
    ASSERT_EQ(script.quoteJacobian_.Cols(), 1);
}

TEST(GSRBondOptionScriptTest, TestInvalidSelectionAndSettingsRejectBeforeSolve) {
    const auto row = Observation(Instrument(), Oracle(0.02));
    for (const auto& selection :
         Vector_<Vector_<GSRCalibrationParameter_>>{{}, {{1, 0}}, {{0, 1}}, {{0, 0}, {0, 0}}, {{0, 0, 0.03, 0.1}}, {{0, 0, -1., 0.1}}})
        ASSERT_THROW(CalibrateGaussianBondOptions(Context(), {row}, selection), CalibrationError_);
    GSRCalibrationSettings_ bad;
    bad.parameterScale_ = 0.;
    ASSERT_THROW(CalibrateGaussianBondOptions(Context(), {row}, {{0, 0}}, bad), CalibrationError_);
    bad = {};
    bad.numericalErrorFraction_ = 1.;
    ASSERT_THROW(CalibrateGaussianBondOptions(Context(), {row}, {{0, 0}}, bad), CalibrationError_);
}

TEST(GSRBondOptionScriptTest, TestSharedQuoteAxesAndRestoredContractRebuildPlans) {
    const auto instrument = BindCalibrationInstrumentParameters(Instrument(), {{{"K", 0.95, CalibrationValueUnit_::Parse("BondPrice")}}});
    const auto restored = LoadCalibrationInstrumentJson(DumpCalibrationInstrumentJson(instrument));
    const auto q = NewCalibrationMarketQuote("shared", Oracle(0.02, true, 0.95), CalibrationValueUnit_::Parse("Price:USD"));
    const auto row1 = NewCalibrationObservation("first", restored, "Price", q, 0.001);
    const auto row2 = NewCalibrationObservation("second", restored, "Price", q, 0.002);
    const auto result = CalibrateGaussianBondOptions(Context(0.012), {row1, row2}, {{0, 0, 0., 0.1}});
    ASSERT_TRUE(result.fitWithinTolerance_);
    ASSERT_EQ(result.axes_.quotes_.size(), 1);
    ASSERT_EQ(result.axes_.quoteIndex_, (Vector_<int>{0, 0}));
    ASSERT_EQ(result.quoteJacobian_.Rows(), 2);
    ASSERT_NEAR(result.quoteJacobian_(0, 0), result.quoteJacobian_(1, 0), 1e-8);
    const auto c = Context();
    const auto a = PrepareCalibrationMc(instrument, "Price", c, McSettings(false)).Evaluate(instrument, c, false);
    const auto b = PrepareCalibrationMc(restored, "Price", c, McSettings(false)).Evaluate(restored, c, false);
    ASSERT_DOUBLE_EQ(a.mean_, b.mean_);
    ASSERT_DOUBLE_EQ(Fast(instrument, c), Fast(restored, c));
}

TEST(GSRBondOptionScriptTest, TestZeroVolatilityPriceAndUnavailablePathwiseDerivative) {
    const auto zero = Context(0.);
    for (double k : {0.95, std::exp(-0.04), 0.97}) {
        const auto instrument = Instrument(true, k);
        ASSERT_NEAR(Fast(instrument, zero), Oracle(0., true, k), 1e-10);
        ASSERT_THROW(PrepareCalibrationMc(instrument, "Price", zero, McSettings()), CalibrationError_);
        const auto plan = PrepareCalibrationMc(instrument, "Price", zero, McSettings(false));
        const auto mc = plan.Evaluate(instrument, zero, true);
        ASSERT_NEAR(mc.mean_, Oracle(0., true, k), 1e-10);
    }
    ASSERT_THROW(Fast(NewBondOptionScriptInstrument({V, T, 0.97, OptionType_("CALL")}, {"expired", Ccy_("USD"), "USD.OIS", 100.}), zero),
                 CalibrationError_);
    ASSERT_THROW(Fast(NewBondOptionScriptInstrument({E, T.AddDays(1), 0.97, OptionType_("CALL")}, {"outside", Ccy_("USD"), "USD.OIS", 100.}), zero),
                 CalibrationError_);
}

TEST(GSRBondOptionScriptTest, TestZeroBoundaryDoesNotPublishTwoSidedNativeJacobian) {
    const auto fit = CalibrateGaussianBondOptions(Context(0.), {Observation(Instrument(), 0.)}, {{0, 0, 0., 0.1}});
    ASSERT_TRUE(fit.fitWithinTolerance_);
    ASSERT_FALSE(fit.nativeJacobianAvailable_);
    ASSERT_EQ(fit.quoteJacobian_.Rows(), 0);
    ASSERT_TRUE(fit.oneSidedColumns_[0]);
}

TEST(GSRBondOptionScriptTest, TestPayoutOnIdenticalScenarioIncludesNumeraireAndDelayDiscount) {
    const auto immediate = Instrument();
    auto spec = immediate->Definition()->Spec();
    spec.source_.rows_.back().text_ += " ON 2028-10-21";
    const auto delayed = NewScriptCalibrationInstrument({"delay", NewScriptInstrumentDefinition(spec)});
    const auto c = Context();
    AAD::GSR_<double> model(static_cast<const MultiFactorGSRModelData_&>(*c.model_));
    Script::MonteCarloSettings_ simulation;
    simulation.compiled_ = true;
    const auto plain = Script::Detail::PrepareBoundScript(immediate->Product(), &model, c.valuation_, simulation, spec.source_.settings_);
    const auto lagged = Script::Detail::PrepareBoundScript(delayed->Product(), &model, c.valuation_, simulation, spec.source_.settings_);
    AAD::Scenario_<> path;
    AAD::AllocatePath(lagged.DefLine(), path);
    ASSERT_EQ(path.size(), 1);
    ASSERT_EQ(path[0].observations_.size(), 1);
    ASSERT_EQ(path[0].discounts_.size(), 1);
    path[0].observations_[0] = 1.1;
    path[0].discounts_[0] = 0.87;
    for (double numeraire : {1.7, 3.4}) {
        path[0].numeraire_ = numeraire;
        for (const auto* p : {&plain, &lagged}) {
            const double expected = 100. * (1.1 - 0.97) * (p == &lagged ? 0.87 : 1.) / numeraire;
            auto tree = p->BuildEvaluator<double>();
            p->Evaluate(path, tree);
            auto state = p->BuildEvalState<double>();
            p->CompiledProgram().Evaluate(path, state);
            ASSERT_NEAR(tree.VarVals()[p->PayOffIdx()], expected, 1e-12);
            ASSERT_NEAR(state.VarVals()[p->PayOffIdx()], expected, 1e-12);
        }
    }
    ASSERT_THROW(Fast(delayed, c), CalibrationError_);
}

TEST(GSRBondOptionScriptTest, TestExportedTreeCompiledAndCrnPathwiseAad) {
    const auto instrument = Instrument();
    const auto c = Context();
    auto s = McSettings();
    s.fitBatches_ = s.validationBatches_ = 2;
    s.fitPathsPerBatch_ = 8;
    s.validationPathsPerBatch_ = 16;
    s.maxPathsPerView_ = 48;
    s.retainPaths_ = true;
    const auto compiled = PrepareCalibrationMc(instrument, "Price", c, s).Evaluate(instrument, c, false);
    s.compiled_ = false;
    const auto tree = PrepareCalibrationMc(instrument, "Price", c, s).Evaluate(instrument, c, false);
    ASSERT_EQ(compiled.pathValues_, tree.pathValues_);
    ASSERT_NEAR(compiled.gradient_[0], tree.gradient_[0], 1e-10);
    s.aad_ = false;
    s.parameterLabels_.clear();
    for (double h : {1e-5, 5e-6}) {
        const auto u = Context(0.02 + h), d = Context(0.02 - h);
        const auto up = PrepareCalibrationMc(instrument, "Price", u, s).Evaluate(instrument, u, false);
        const auto down = PrepareCalibrationMc(instrument, "Price", d, s).Evaluate(instrument, d, false);
        for (int i = 0; i < 16; ++i)
            ASSERT_NEAR(compiled.pathDerivatives_(i, 0), (up.pathValues_[i] - down.pathValues_[i]) / (2. * h), 1e-5);
    }
}

TEST(GSRBondOptionNumericalTest, TestFrozenOracleFastAndSameBindingMcAtInitialNonrootAndFit) {
    for (bool call : {true, false}) {
        const auto instrument = Instrument(call);
        for (double g : {0.012, 0.016, 0.02}) {
            const auto c = Context(g);
            const auto reference = Reference(instrument, g);
            ASSERT_NEAR(reference.value_, Oracle(g, call), 1e-10);
            ASSERT_NEAR(reference.gradient_[0], Vega(g), 2e-4);
            const auto plan = PrepareCalibrationMc(instrument, "Price", c, McSettings());
            ASSERT_EQ(plan.Mapping().numeraire_, "dal.native-gsr/1");
            const auto result = plan.Evaluate(instrument, c, true);
            const auto acceptance = ValidateCalibrationMc(result, reference, Precision());
            ASSERT_TRUE(acceptance.numericalValidationPassed_)
                << "g=" << g << " call=" << call << " value=" << result.mean_ << " SE=" << result.standardError_;
            ASSERT_EQ(result.pathsPerView_, 262144);
            ASSERT_EQ(result.engineRuns_, 16);
            const auto key = std::string(call ? "call_" : "put_") + std::to_string(g);
            RecordProperty(key, std::to_string(result.mean_) + "," + std::to_string(result.standardError_) + "," +
                                    std::to_string(result.gradient_[0]) + "," + std::to_string(result.gradientStandardError_[0]));
        }
    }
}

TEST(GSRBondOptionNumericalTest, TestFastFitToFixedContractMcAndBudgetRefinement) {
    const auto instrument = Instrument();
    const auto fit = CalibrateGaussianBondOptions(Context(0.012), {Observation(instrument, Oracle(0.02))}, {{0, 0, 0., 0.1}});
    ASSERT_TRUE(fit.fitWithinTolerance_);
    auto c = Context();
    c.model_ = Handle_<ModelData_>(fit.model_);
    const auto reference = Reference(instrument, fit.parameters_[0]);
    const auto plan = PrepareCalibrationMc(instrument, "Price", c, McSettings());
    const auto result = plan.Evaluate(instrument, c, true);
    ASSERT_EQ(result.instrumentIdentity_, instrument->InstrumentIdentity());
    ASSERT_TRUE(ValidateCalibrationMc(result, reference, Precision()).numericalValidationPassed_);
    auto prefix = McSettings();
    prefix.validationBatches_ = 8;
    const auto finer = PrepareCalibrationMc(instrument, "Price", c, prefix).Evaluate(instrument, c, true);
    ASSERT_TRUE(ValidateCalibrationMc(finer, reference, Precision()).numericalValidationPassed_);
    auto small = McSettings();
    small.fitBatches_ = small.validationBatches_ = 2;
    small.fitPathsPerBatch_ = 8;
    small.validationPathsPerBatch_ = 16;
    small.maxPathsPerView_ = 48;
    const auto exhausted = PrepareCalibrationMc(instrument, "Price", c, small).Evaluate(instrument, c, true);
    const auto rejected = ValidateCalibrationMc(exhausted, reference, Precision());
    ASSERT_FALSE(rejected.numericalValidationPassed_);
    ASSERT_TRUE(rejected.budgetExhausted_);
    RecordProperty("fast_prices", static_cast<int>(fit.costs_.fastPrices_));
    RecordProperty("fast_models", static_cast<int>(fit.costs_.models_));
    RecordProperty("mc_validation_paths", static_cast<int>(result.pathsPerView_));
    RecordProperty("mc_engine_runs", static_cast<int>(result.engineRuns_));
    RecordProperty("peak_tape_nodes", static_cast<int>(result.peakTapeNodes_));
    RecordProperty("peak_tape_bytes", static_cast<int>(result.peakTapeBytes_));
    RecordProperty("validation_prefix_delta", std::to_string(std::abs(finer.mean_ - result.mean_)));
}

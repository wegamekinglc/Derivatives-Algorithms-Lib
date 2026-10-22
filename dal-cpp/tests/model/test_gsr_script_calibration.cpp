#include <gtest/gtest.h>

#include <dal/indice/index/ir.hpp>
#include <dal/math/aad/recording.hpp>
#include <dal/math/random/pseudorandom.hpp>
#include <dal/model/gsrbondoption.hpp>
#include <dal/model/gsrrateoption.hpp>
#include <dal/model/gsrscriptcalibration.hpp>
#include <dal/time/dateutils.hpp>

using namespace Dal;

namespace {
    const Date_ AVG_V(2026, 10, 21), AVG_F1(2027, 10, 21), AVG_F2(2028, 4, 21), AVG_PAY(2028, 10, 21), AVG_END(2029, 10, 21);
    double AverageTime(const Date_& date) { return (date - AVG_V) / 365.; }
    CalibrationPreparationContext_ AverageContext(double g = .02) {
        CalibrationPreparationContext_ c;
        c.currency_ = Ccy_("USD");
        c.discountRoute_ = "USD.OIS";
        c.valuation_.evaluationDate_ = AVG_V;
        c.valuation_.fixings_.reset(new MarketFixingSnapshot_());
        c.curve_.reset(new GSRCurveData_("average-curve", AVG_V, "USD", {AVG_V, AVG_END}, {0., -.12}, {}, {}));
        Handle_<MultiFactorGSRVolData_> vol(
            new MultiFactorGSRVolData_("average-vol", {"level"}, {AVG_V}, Matrix_<>(1, 1, g), {AVG_V}, Matrix_<>(1, 1, 1.), Matrix_<>(1, 1, 1.)));
        c.model_.reset(new MultiFactorGSRModelData_("average-model", c.curve_, vol));
        return c;
    }
    GSRScriptCalibrationSettings_ AverageSettings() {
        GSRScriptCalibrationSettings_ s;
        s.sampling_.allowMonteCarloCalibration_ = true;
        s.sampling_.fitBatches_ = 8;
        s.sampling_.fitPathsPerBatch_ = 8192;
        s.sampling_.validationBatches_ = s.sampling_.heldOutBatches_ = 16;
        s.sampling_.validationPathsPerBatch_ = s.sampling_.heldOutPathsPerBatch_ = 16384;
        s.sampling_.maxPathsPerView_ = 589824;
        s.optimizer_.gradientTolerance_ = 1e-10;
        return s;
    }
    ScriptInstrumentDefinitionSpec_ AverageDefinition() {
        const auto price = CalibrationValueUnit_::Parse("Price:USD");
        ScriptInstrumentDefinitionSpec_ s;
        s.header_.payCurrency_ = s.header_.resultCurrency_ = s.header_.collateralCurrency_ = Ccy_("USD");
        s.header_.discountRoute_ = "USD.OIS";
        s.source_.rows_ = {{ScriptSourceRowKind_::Value_::Definition, "N", "100"},
                           {ScriptSourceRowKind_::Value_::Definition, "K", ".04"},
                           {ScriptSourceRowKind_::Value_::Event, "2027-10-21", "avg = .5*FIX(IR[USD,LIBOR_6M_LCH,2027-10-21])"},
                           {ScriptSourceRowKind_::Value_::Event, "2028-04-21", "avg = avg+.5*FIX(IR[USD,LIBOR_6M_LCH,2028-04-21])"},
                           {ScriptSourceRowKind_::Value_::Event, "2028-10-21", "pay PAYS N*MAX(avg-K,0)"}};
        s.parameters_ = {
            {"N", ScriptParameterType_::Value_::Real, price, ScriptParameterRole_::Value_::Notional, true, 0., {}, false, true},
            {"K", ScriptParameterType_::Value_::Real, CalibrationValueUnit_::Parse("Rate"), ScriptParameterRole_::Value_::RateStrike, true}};
        s.measures_ = {{"Price", CalibrationMeasureKind_::Value_::Price, "PAY", {}, price}};
        return s;
    }
    Handle_<CalibrationInstrument_> AverageInstrument(const String_& id = "average", double notional = 100.) {
        const auto instrument = NewScriptCalibrationInstrument({id, NewScriptInstrumentDefinition(AverageDefinition())});
        return BindCalibrationInstrumentParameters(instrument, {{{"N", notional, CalibrationValueUnit_::Parse("Price:USD")}}, {}});
    }
    std::pair<double, double> AveragePathOracle(double g, const Vector_<>& z) {
        const Vector_<Date_> dates = {AVG_F1, AVG_F2, AVG_PAY};
        double w = 0., logN = 0., dLogN = 0., average = 0., dAverage = 0., previous = 0.;
        for (size_t i = 0; i < dates.size(); ++i) {
            const double t = AverageTime(dates[i]), dt = t - previous;
            const double c = .5 * dt * previous * previous + .5 * dt * dt * previous + .125 * dt * dt * dt;
            const double dw = std::sqrt(dt) * z[i];
            logN += .12 * dt / AverageTime(AVG_END) + g * g * c + g * dt * w + .5 * g * dt * dw;
            dLogN += 2. * g * c + dt * w + .5 * dt * dw;
            w += dw;
            if (i < 2) {
                const Date_ end = Date::AddMonths(dates[i], 6);
                const double tau = (end - dates[i]) / 365., alpha = (end - dates[i]) / 360.;
                const double drift = tau * t * t + tau * tau * t;
                const double e = .12 * tau / AverageTime(AVG_END) + .5 * g * g * drift + tau * g * w;
                average += .5 * std::expm1(e) / alpha;
                dAverage += .5 * std::exp(e) * (g * drift + tau * w) / alpha;
            }
            previous = t;
        }
        const double payoff = std::max(average - .04, 0.), discount = std::exp(-logN);
        return {100. * payoff * discount, 100. * discount * ((average > .04 ? dAverage : 0.) - payoff * dLogN)};
    }
    double AverageFitOracle(double g) {
        auto random = New(RNGType_("MRG32"), 1024, 3, true);
        Vector_<> z(3);
        double mean = 0.;
        for (int i = 0; i < 65536; ++i) {
            random->FillNormal(&z);
            mean += AveragePathOracle(g, z).first / 65536.;
        }
        return mean;
    }
    double AverageConditional(double z, double g) {
        const double t1 = AverageTime(AVG_F1), t2 = AverageTime(AVG_F2), payment = AverageTime(AVG_PAY);
        const double tau1 = (Date::AddMonths(AVG_F1, 6) - AVG_F1) / 365., tau2 = (Date::AddMonths(AVG_F2, 6) - AVG_F2) / 365.;
        const double alpha1 = (Date::AddMonths(AVG_F1, 6) - AVG_F1) / 360., alpha2 = (Date::AddMonths(AVG_F2, 6) - AVG_F2) / 360.;
        const auto exponent = [&](double t, double tau) {
            return .12 * tau / AverageTime(AVG_END) + g * g * tau * t * (t - payment) + .5 * g * g * tau * tau * t + tau * g * std::sqrt(t1) * z;
        };
        const double a = .5 * std::exp(exponent(t2, tau2)) / alpha2;
        const double b = .04 + .5 / alpha1 + .5 / alpha2 - .5 * std::exp(exponent(t1, tau1)) / alpha1;
        const double sigma = tau2 * g * std::sqrt(t2 - t1), forward = a * std::exp(.5 * sigma * sigma);
        if (b <= 0.)
            return forward - b;
        const double d2 = std::log(a / b) / sigma;
        const auto cdf = [](double x) { return .5 * std::erfc(-x / std::sqrt(2.)); };
        return forward * cdf(d2 + sigma) - b * cdf(d2);
    }
    double AverageExpectation(double g, int intervals = 8192) {
        const double step = 20. / intervals, pi = std::acos(-1.);
        double sum = 0.;
        for (int i = 0; i <= intervals; ++i) {
            const double z = -10. + i * step;
            const double weight = i == 0 || i == intervals ? 1. : (i % 2 == 0 ? 2. : 4.);
            sum += weight * AverageConditional(z, g) * std::exp(-.5 * z * z) / std::sqrt(2. * pi);
        }
        return 100. * std::exp(-.12 * AverageTime(AVG_PAY) / AverageTime(AVG_END)) * sum * step / 3.;
    }
    CalibrationMcSettings_ AverageSampling(bool aad = true) {
        auto mc = AverageSettings().sampling_;
        mc.aad_ = aad;
        mc.retainPaths_ = true;
        mc.parameterLabels_ = aad ? Vector_<String_>{"g:level:2026-10-21"} : Vector_<String_>{};
        return mc;
    }
    Vector_<Handle_<CalibrationObservation_>> AverageRows(double target, const String_& prefix, double scale = .1) {
        Vector_<Handle_<CalibrationObservation_>> rows;
        for (int i = 0; i < 2; ++i) {
            const String_ id = prefix + String::FromInt(i);
            rows.push_back(NewCalibrationObservation(id, AverageInstrument(id, 100. * (i + 1)), "Price",
                                                     NewCalibrationMarketQuote(id, target * (i + 1), CalibrationValueUnit_::Parse("Price:USD")),
                                                     scale));
        }
        return rows;
    }
    void RecordAverageValidation(const GSRScriptCalibrationResult_& result) {
        ::testing::Test::RecordProperty("converged", result.converged_ ? "true" : "false");
        ::testing::Test::RecordProperty("termination", std::string(result.terminationReason_.begin(), result.terminationReason_.end()));
        ::testing::Test::RecordProperty("fitted_g", std::to_string(result.parameters_[0]));
        ::testing::Test::RecordProperty("basket_evaluations", static_cast<int>(result.costs_.basketEvaluations_));
        ::testing::Test::RecordProperty("path_executions", static_cast<int>(result.costs_.paths_));
        for (int i = 0; i < result.validation_.size(); ++i) {
            const std::string id = std::to_string(i);
            ::testing::Test::RecordProperty("fit_" + id, std::to_string(result.fit_[i].mean_));
            ::testing::Test::RecordProperty("validation_" + id, std::to_string(result.validation_[i].mean_));
            ::testing::Test::RecordProperty("se_" + id, std::to_string(result.validation_[i].standardError_));
            ::testing::Test::RecordProperty("error_indicator_" + id, std::to_string(result.validationIndicators_[i].errorIndicator_));
            ::testing::Test::RecordProperty("gradient_indicator_" + id,
                                            std::to_string(result.validationIndicators_[i].gradientPrecisionIndicators_[0]));
        }
    }
} // namespace

TEST(GSRAverageCalibrationTest, TestOriginalBoundTreeCompiledAndAadMatchIndependentPathOracle) {
    const auto context = AverageContext();
    const auto instrument = AverageInstrument();
    for (bool compiled : {false, true}) {
        auto mc = AverageSampling();
        mc.compiled_ = compiled;
        const auto plan = PrepareCalibrationMc(instrument, "Price", context, mc);
        ASSERT_EQ(plan.Capability().ruleId_, "dal.gaussian-script-price/1");
        ASSERT_EQ(plan.Mapping().timeline_, (Vector_<>{AverageTime(AVG_F1), AverageTime(AVG_F2), AverageTime(AVG_PAY)}));
        ASSERT_EQ(plan.Mapping().dimension_, 3);
        const auto fit = plan.Evaluate(instrument, context, false);
        auto random = New(RNGType_("MRG32"), 1024, 3, true);
        Vector_<> z(3);
        for (size_t i = 0; i < fit.pathValues_.size(); ++i) {
            random->FillNormal(&z);
            const auto oracle = AveragePathOracle(.02, z);
            ASSERT_NEAR(fit.pathValues_[i], oracle.first, 1e-10);
            ASSERT_NEAR(fit.pathDerivatives_(static_cast<int>(i), 0), oracle.second, 1e-8);
        }
        ASSERT_NEAR(fit.mean_, AverageFitOracle(.02), 1e-11);
    }
}

TEST(GSRAverageCalibrationTest, TestNonRootRawGradientAndRefreshMatchCrnStepRefinement) {
    const auto instrument = AverageInstrument();
    const auto context = AverageContext(.016);
    const auto plan = PrepareCalibrationMc(instrument, "Price", context, AverageSampling());
    const auto fit = plan.Evaluate(instrument, context, false);
    ASSERT_GT(std::abs(fit.mean_ - AverageFitOracle(.02)), .01);
    Vector_<> gradients;
    for (double h : {1e-6, 5e-7}) {
        const auto high = AverageContext(.016 + h), low = AverageContext(.016 - h);
        const auto up = plan.RefreshG(high), down = plan.RefreshG(low);
        ASSERT_EQ(up.Mapping().identity_, plan.Mapping().identity_);
        ASSERT_EQ(up.PlanIdentity(), PrepareCalibrationMc(instrument, "Price", high, AverageSampling()).PlanIdentity());
        const double fd = (up.Evaluate(instrument, high, false).mean_ - down.Evaluate(instrument, low, false).mean_) / (2. * h);
        ASSERT_NEAR(fit.gradient_[0], fd, 2e-6);
        gradients.push_back(fd);
    }
    ASSERT_NEAR(gradients[0], gradients[1], 2e-6);
    ASSERT_EQ(AAD::Tape()->nodes_.Size(), 0);
}

TEST(GSRAverageCalibrationTest, TestFrozenMultiObservationFitValidationAndIndependentHeldOut) {
    const double oracle = AverageExpectation(.02);
    ASSERT_NEAR(oracle, AverageExpectation(.02, 16384), 1e-10);
    const auto rows = AverageRows(AverageFitOracle(.02), "fit-average-");
    const auto held = AverageRows(oracle, "held-average-");
    const auto result = CalibrateGSRScriptPrices(AverageContext(.012), rows, {{0, 0, .001, .06}}, AverageSettings(), held);
    RecordAverageValidation(result);
    ASSERT_TRUE(result.converged_ || result.terminationReason_ == "StepStalled");
    ASSERT_TRUE(result.fitWithinTolerance_);
    ASSERT_FALSE(result.numericalValidationPassed_);
    ASSERT_TRUE(result.validationIndicators_[0].numericalValidationPassed_);
    ASSERT_FALSE(result.validationIndicators_[1].numericalValidationPassed_);
    ASSERT_TRUE(result.validationIndicators_[1].budgetExhausted_);
    ASSERT_GT(result.validationIndicators_[1].errorIndicator_, .025);
    ASSERT_TRUE(result.heldOutRequested_);
    ASSERT_TRUE(result.heldOutPassed_);
    ASSERT_NEAR(result.parameters_[0], .02, 1e-8);
    ASSERT_EQ(result.quoteJacobian_.Rows(), 2);
    ASSERT_EQ(result.quoteJacobian_.Cols(), 1);
    ASSERT_EQ(result.parameterLabels_[0], "g:level:2026-10-21");
    ASSERT_EQ(result.jacobianRank_, 1);
    ASSERT_NEAR(result.quoteJacobian_(1, 0), 2. * result.quoteJacobian_(0, 0), 1e-10);
    for (size_t i = 0; i < rows.size(); ++i) {
        ASSERT_EQ(result.fit_[i].pathsPerView_, 65536);
        ASSERT_EQ(result.validation_[i].ranges_[0].firstPath_, 65536);
        ASSERT_EQ(result.heldOut_[i].ranges_[0].firstPath_, 327680);
        ASSERT_EQ(result.validation_[i].pathsPerView_, 262144);
        ASSERT_TRUE(result.validationIndicators_[i].gradientParityPassed_);
        ASSERT_TRUE(result.validationIndicators_[i].gradientPrecisionPassed_);
        ASSERT_LE(std::abs(result.heldOut_[i].mean_ - held[i]->Quote()->Value()) + 3. * result.heldOut_[i].standardError_, .1);
    }
    ASSERT_FALSE(result.quoteRiskAvailable_);
    ASSERT_GT(result.costs_.peakTapeNodes_, 0);
    ASSERT_EQ(AAD::Tape()->nodes_.Size(), 0);
}

TEST(GSRAverageCalibrationTest, TestFrozenPrimaryCandidateClosesIndependentNumericsAndHeldOut) {
    const auto rows = AverageRows(AverageFitOracle(.02), "primary-");
    const auto held = AverageRows(AverageExpectation(.02), "primary-held-");
    const auto result = CalibrateGSRScriptPrices(AverageContext(.012), {rows[0]}, {{0, 0, .001, .06}}, AverageSettings(), {held[0]});
    RecordAverageValidation(result);
    ASSERT_TRUE(result.fitWithinTolerance_);
    ASSERT_TRUE(result.numericalValidationPassed_);
    ASSERT_TRUE(result.heldOutPassed_);
    ASSERT_NEAR(result.parameters_[0], .02, 1e-8);
    ASSERT_LE(result.validationIndicators_[0].errorIndicator_, .025);
}

TEST(GSRAverageCalibrationTest, TestIndependentExpectationGradientAndFrozenPrecisionBudget) {
    const auto instrument = AverageInstrument();
    auto mc = AverageSampling();
    mc.retainPaths_ = false;
    const auto context = AverageContext();
    const auto plan = PrepareCalibrationMc(instrument, "Price", context, mc);
    const auto fit = plan.Evaluate(instrument, context, false), val = plan.Evaluate(instrument, context, true);
    Vector_<> differences;
    for (double h : {1e-6, 5e-7})
        differences.push_back((AverageExpectation(.02 + h) - AverageExpectation(.02 - h)) / (2. * h));
    ASSERT_NEAR(differences[0], differences[1], 2e-6);
    CalibrationMcReference_ reference;
    reference.value_ = AverageExpectation(.02);
    reference.numericalError_ = std::abs(reference.value_ - AverageExpectation(.02, 16384));
    reference.gradient_ = {differences[1]};
    reference.gradientStandardError_ = {0.};
    reference.finiteDifferenceError_ = {std::abs(differences[0] - differences[1])};
    CalibrationMcPrecision_ precision;
    precision.errorScale_ = .1;
    precision.parameterScales_ = {.01};
    precision.epsilonGradient_ = {1e-8};
    const auto validation = ValidateCalibrationMc(val, reference, precision);
    ASSERT_TRUE(validation.numericalValidationPassed_);
    ASSERT_TRUE(validation.gradientParityPassed_);
    ASSERT_TRUE(validation.gradientPrecisionPassed_);
    precision.errorScale_ = 1e-5;
    const auto exhausted = ValidateCalibrationMc(fit, val, precision);
    ASSERT_FALSE(exhausted.numericalValidationPassed_);
    ASSERT_TRUE(exhausted.budgetExhausted_);
    ASSERT_FALSE(exhausted.precisionPassed_);
}

TEST(GSRAverageCalibrationTest, TestSingleFixingDegeneracyPreservesAccrualAndDelayedNumeraire) {
    auto source = AverageDefinition();
    source.source_.rows_[2].text_ = "avg = FIX(IR[USD,LIBOR_6M_LCH,2027-10-21])";
    source.source_.rows_.erase(source.source_.rows_.begin() + 3);
    const auto instrument = NewScriptCalibrationInstrument({"single-average", NewScriptInstrumentDefinition(source)});
    const auto context = AverageContext();
    const auto mc = AverageSampling();
    const auto plan = PrepareCalibrationMc(instrument, "Price", context, mc);
    const Caplet_ caplet{AVG_F1, AVG_F1, AVG_F2, AVG_PAY, (AVG_F2 - AVG_F1) / 360., 1., "6M", .04, OptionType_("CALL")};
    const auto native = NewCapletScriptInstrument(caplet, {"single-native", Ccy_("USD"), "USD.OIS", 100.});
    const auto nativePlan = PrepareCalibrationMc(native, "Price", context, mc);
    ASSERT_EQ(nativePlan.Capability().ruleId_, "dal.gaussian-caplet/1");
    ASSERT_EQ(plan.Mapping().identity_, nativePlan.Mapping().identity_);
    const auto fit = plan.Evaluate(instrument, context, false), nativeFit = nativePlan.Evaluate(native, context, false);
    ASSERT_EQ(fit.pathValues_, nativeFit.pathValues_);
    ASSERT_NEAR(fit.gradient_[0], nativeFit.gradient_[0], 1e-10);
    CalibrationMcReference_ reference;
    const auto& model = static_cast<const MultiFactorGSRModelData_&>(*context.model_);
    reference.value_ = 100. * PriceGSREuropeanOption(model, caplet).price_;
    const auto nativePrice = [&](double g) {
        const auto c = AverageContext(g);
        return 100. * PriceGSREuropeanOption(static_cast<const MultiFactorGSRModelData_&>(*c.model_), caplet).price_;
    };
    const double coarse = (nativePrice(.02 + 1e-6) - nativePrice(.02 - 1e-6)) / 2e-6;
    const double fine = (nativePrice(.02 + 5e-7) - nativePrice(.02 - 5e-7)) / 1e-6;
    reference.gradient_ = {fine};
    reference.gradientStandardError_ = {0.};
    reference.finiteDifferenceError_ = {std::abs(coarse - fine)};
    CalibrationMcPrecision_ precision;
    precision.errorScale_ = .1;
    precision.parameterScales_ = {.01};
    precision.epsilonGradient_ = {1e-8};
    ASSERT_TRUE(ValidateCalibrationMc(plan.Evaluate(instrument, context, true), reference, precision).numericalValidationPassed_);
}

TEST(GSRAverageCalibrationTest, TestEventGridRefinementRetainsIndependentExpectationAndGradient) {
    auto source = AverageDefinition();
    const auto rows = source.source_.rows_;
    source.source_.rows_ = {rows[0],
                            rows[1],
                            {ScriptSourceRowKind_::Value_::Event, "2027-04-21", "clock = 0"},
                            rows[2],
                            {ScriptSourceRowKind_::Value_::Event, "2028-01-21", "clock = 0"},
                            rows[3],
                            {ScriptSourceRowKind_::Value_::Event, "2028-07-21", "clock = 0"},
                            rows[4]};
    const auto instrument = NewScriptCalibrationInstrument({"refined-average", NewScriptInstrumentDefinition(source)});
    const auto context = AverageContext();
    auto mc = AverageSampling();
    mc.retainPaths_ = false;
    const auto plan = PrepareCalibrationMc(instrument, "Price", context, mc);
    ASSERT_EQ(plan.Mapping().dimension_, 6);
    ASSERT_NE(plan.Mapping().identity_, PrepareCalibrationMc(AverageInstrument(), "Price", context, mc).Mapping().identity_);
    CalibrationMcReference_ reference;
    reference.value_ = AverageExpectation(.02);
    const double coarse = (AverageExpectation(.02 + 1e-6) - AverageExpectation(.02 - 1e-6)) / 2e-6;
    const double fine = (AverageExpectation(.02 + 5e-7) - AverageExpectation(.02 - 5e-7)) / 1e-6;
    reference.gradient_ = {fine};
    reference.gradientStandardError_ = {0.};
    reference.finiteDifferenceError_ = {std::abs(coarse - fine)};
    CalibrationMcPrecision_ precision;
    precision.errorScale_ = .1;
    precision.parameterScales_ = {.01};
    precision.epsilonGradient_ = {1e-8};
    ASSERT_TRUE(ValidateCalibrationMc(plan.Evaluate(instrument, context, true), reference, precision).numericalValidationPassed_);
}

TEST(GSRAverageCalibrationTest, TestDriverNonRootRawRowsRemainUnscaledAndMatchCrnDifferences) {
    auto settings = AverageSettings();
    settings.optimizer_.priorWeight_ = 1e8;
    const auto rows = AverageRows(AverageFitOracle(.02), "nonroot-average-");
    const auto result = CalibrateGSRScriptPrices(AverageContext(.016), rows, {{0, 0, .001, .06}}, settings);
    ASSERT_FALSE(result.fitWithinTolerance_);
    ASSERT_GT(std::abs(result.residuals_[0]), .01);
    ASSERT_EQ(result.penaltyResiduals_.size(), 1);
    for (double h : {1e-6, 5e-7}) {
        const auto high = AverageContext(result.parameters_[0] + h), low = AverageContext(result.parameters_[0] - h);
        const auto instrument = rows[0]->Instrument();
        const auto value = [&](const CalibrationPreparationContext_& c) {
            return PrepareCalibrationMc(instrument, "Price", c, AverageSampling(false)).Evaluate(instrument, c, false).mean_;
        };
        const double fd = (value(high) - value(low)) / (2. * h);
        ASSERT_NEAR(result.quoteJacobian_(0, 0), fd, 2e-6);
        ASSERT_NEAR(result.quoteJacobian_(1, 0), 2. * fd, 2e-6);
    }
    ASSERT_EQ(result.quoteJacobian_(0, 0), result.fit_[0].gradient_[0]);
}

TEST(GSRAverageCalibrationTest, TestHistoricalAverageSnapshotAndRebindingCannotReuseStalePlan) {
    auto source = AverageDefinition();
    source.source_.rows_[2].key_ = "2026-10-20";
    const auto instrument = NewScriptCalibrationInstrument({"historical-average", NewScriptInstrumentDefinition(source)});
    auto context = AverageContext();
    const String_ index = Index::Parse("IR:USD,LIBOR_6M_LCH,2027-10-21")->Name();
    context.valuation_.fixings_.reset(new MarketFixingSnapshot_({{index, {{DateTime_(Date_(2026, 10, 20), 0.), .035}}}}));
    const auto mc = AverageSampling();
    const auto plan = PrepareCalibrationMc(instrument, "Price", context, mc);
    const auto fit = plan.Evaluate(instrument, context, false);
    source.source_.rows_[2].text_ = "avg = .5*.035";
    const auto literal = NewScriptCalibrationInstrument({"literal-history", NewScriptInstrumentDefinition(source)});
    const auto expected = PrepareCalibrationMc(literal, "Price", context, mc).Evaluate(literal, context, false);
    ASSERT_EQ(fit.pathValues_, expected.pathValues_);
    ASSERT_EQ(fit.pathDerivatives_.Rows(), expected.pathDerivatives_.Rows());
    ASSERT_EQ(fit.pathDerivatives_.Cols(), expected.pathDerivatives_.Cols());
    for (int i = 0; i < fit.pathDerivatives_.Rows(); ++i)
        ASSERT_EQ(fit.pathDerivatives_(i, 0), expected.pathDerivatives_(i, 0));
    auto changed = context;
    changed.valuation_.fixings_.reset(new MarketFixingSnapshot_({{index, {{DateTime_(Date_(2026, 10, 20), 0.), .036}}}}));
    ASSERT_THROW(static_cast<void>(plan.Evaluate(instrument, changed, false)), CalibrationError_);
    ASSERT_NE(plan.PlanIdentity(), PrepareCalibrationMc(instrument, "Price", changed, mc).PlanIdentity());
    const auto rebound = BindCalibrationInstrumentParameters(instrument, {{{"N", 200., CalibrationValueUnit_::Parse("Price:USD")}}, {}});
    ASSERT_THROW(static_cast<void>(plan.Evaluate(rebound, context, false)), CalibrationError_);
    const auto reboundFit = PrepareCalibrationMc(rebound, "Price", context, mc).Evaluate(rebound, context, false);
    ASSERT_NEAR(reboundFit.mean_, 2. * fit.mean_, 1e-12);
    ASSERT_EQ(plan.Evaluate(instrument, context, false).pathValues_, fit.pathValues_);
}

TEST(GSRAverageCalibrationTest, TestBudgetAndNumericalFailureOwnAxesAndLeaveTapeReusable) {
    auto settings = AverageSettings();
    settings.maxBasketEvaluations_ = 1;
    const auto rows = AverageRows(AverageFitOracle(.02), "budget-average-");
    try {
        static_cast<void>(CalibrateGSRScriptPrices(AverageContext(.012), rows, {{0, 0, .001, .06}}, settings));
        FAIL();
    } catch (const CalibrationError_& e) {
        ASSERT_EQ(e.Failure().diagnostic_.code_, "CalibrationBudgetExceeded");
        ASSERT_EQ(e.Failure().diagnostic_.stage_, CalibrationStage_::Value_::Solve);
        ASSERT_TRUE(e.Failure().candidate_);
        ASSERT_EQ(e.Failure().report_->Axes().observations_.size(), 2);
    }
    auto source = AverageDefinition();
    source.source_.rows_.back().text_ = "pay PAYS N/(avg-avg)";
    const auto invalid = NewScriptCalibrationInstrument({"undefined-average", NewScriptInstrumentDefinition(source)});
    const auto context = AverageContext();
    const auto mc = AverageSampling();
    const auto plan = PrepareCalibrationMc(invalid, "Price", context, mc);
    try {
        static_cast<void>(plan.Evaluate(invalid, context, false));
        FAIL();
    } catch (const CalibrationError_& e) {
        ASSERT_EQ(e.Failure().diagnostic_.code_, "CalibrationNumericalFailure");
        ASSERT_TRUE(e.Failure().report_);
    }
    ASSERT_EQ(AAD::Tape()->nodes_.Size(), 0);
    const auto valid = AverageInstrument();
    ASSERT_NEAR(PrepareCalibrationMc(valid, "Price", context, mc).Evaluate(valid, context, false).mean_, AverageFitOracle(.02), 1e-11);
}

namespace {
    CalibrationPreparationContext_ FitContext(double first = 0.012, double second = 0.016) {
        CalibrationPreparationContext_ c;
        const Date_ v(2026, 10, 21), k(2027, 10, 21);
        c.currency_ = Ccy_("USD");
        c.discountRoute_ = "USD.OIS";
        c.valuation_.evaluationDate_ = v;
        c.valuation_.fixings_.reset(new MarketFixingSnapshot_());
        c.curve_.reset(new GSRCurveData_("curve", v, "USD", {v, k, Date_(2028, 10, 21), Date_(2029, 10, 21)}, {0., -.04, -.08, -.12}, {}, {}));
        Matrix_<> g(1, 2);
        g(0, 0) = first;
        g(0, 1) = second;
        Handle_<MultiFactorGSRVolData_> vol(new MultiFactorGSRVolData_("vol", {"level"}, {v, k}, g, {v}, Matrix_<>(1, 1, 1.), Matrix_<>(1, 1, 1.)));
        c.model_.reset(new MultiFactorGSRModelData_("model", c.curve_, vol));
        return c;
    }
    GSRScriptCalibrationSettings_ FitSettings() {
        GSRScriptCalibrationSettings_ s;
        s.sampling_.allowMonteCarloCalibration_ = true;
        s.sampling_.fitBatches_ = 2;
        s.sampling_.fitPathsPerBatch_ = 64;
        s.sampling_.validationBatches_ = 4;
        s.sampling_.validationPathsPerBatch_ = 128;
        s.sampling_.heldOutBatches_ = 4;
        s.sampling_.heldOutPathsPerBatch_ = 128;
        s.sampling_.maxPathsPerView_ = 1152;
        s.optimizer_.gradientTolerance_ = 1e-10;
        return s;
    }
    Vector_<GSRCalibrationParameter_> FitParameters() { return {{0, 1, .001, .06}, {0, 0, .001, .06}}; }
    Vector_<Handle_<CalibrationObservation_>> FitRows(const GSRScriptCalibrationSettings_& settings, double scale = .1) {
        const auto targetContext = FitContext(.02, .025);
        Vector_<Handle_<CalibrationObservation_>> rows;
        for (int i = 0; i < 2; ++i) {
            const String_ id = i == 0 ? "short" : "long";
            const auto instrument = NewBondOptionScriptInstrument(
                BondOption_{Date_(2027 + i, 10, 21), Date_(2028 + i, 10, 21), .97, OptionType_("CALL")}, {id, Ccy_("USD"), "USD.OIS", 100.});
            auto mc = settings.sampling_;
            mc.aad_ = false;
            const auto target = PrepareCalibrationMc(instrument, "Price", targetContext, mc).Evaluate(instrument, targetContext, false).mean_;
            rows.push_back(NewCalibrationObservation(id, instrument, "Price",
                                                     NewCalibrationMarketQuote(id, target, CalibrationValueUnit_::Parse("Price:USD")), scale));
        }
        return rows;
    }
} // namespace

TEST(GSRScriptCalibrationTest, TestSelectedGRecoveryAndFullRawJacobian) {
    const auto settings = FitSettings();
    const auto rows = FitRows(settings);
    const auto result = CalibrateGSRScriptPrices(FitContext(), rows, FitParameters(), settings);
    ASSERT_TRUE(result.converged_);
    ASSERT_TRUE(result.fitWithinTolerance_);
    ASSERT_NEAR(result.parameters_[0], .025, 1e-8);
    ASSERT_NEAR(result.parameters_[1], .02, 1e-8);
    ASSERT_EQ(result.quoteJacobian_.Rows(), 2);
    ASSERT_EQ(result.quoteJacobian_.Cols(), 2);
    ASSERT_EQ(result.jacobianRank_, 2);
    ASSERT_EQ(result.parameterLabels_[0], "g:level:2027-10-21");
    ASSERT_DOUBLE_EQ(result.quoteJacobian_(0, 0), 0.);
    ASSERT_GT(result.quoteJacobian_(0, 1), 1.);
    ASSERT_EQ(result.derivativeMethod_, CalibrationDerivative_::Value_::AAD);
    ASSERT_GT(result.costs_.aadRows_, 0);
    ASSERT_FALSE(result.quoteRiskAvailable_);
    ASSERT_EQ(result.fit_[0].ranges_[0].firstPath_, 0);
    ASSERT_EQ(result.validation_[0].ranges_[0].firstPath_, 128);
}

TEST(GSRScriptCalibrationTest, TestNonRootRowsMatchIndependentCrnDifferences) {
    auto settings = FitSettings();
    settings.optimizer_.priorWeight_ = 1e8;
    settings.optimizer_.smoothingWeight_ = 2.;
    const auto rows = FitRows(settings);
    const auto parameters = FitParameters();
    const auto result = CalibrateGSRScriptPrices(FitContext(), rows, parameters, settings);
    ASSERT_GT(std::abs(result.residuals_[0]), .01);
    ASSERT_EQ(result.penaltyResiduals_.size(), 3);
    ASSERT_NEAR(result.objective_, result.dataObjective_ + result.penaltyObjective_, 1e-14);
    for (size_t j = 0; j < parameters.size(); ++j) {
        Vector_<> differences;
        for (double h : {1e-6, 5e-7}) {
            Vector_<> displacement(parameters.size(), 0.);
            displacement[j] = h;
            const auto up = FitContext(result.parameters_[1] + displacement[1], result.parameters_[0] + displacement[0]);
            const auto down = FitContext(result.parameters_[1] - displacement[1], result.parameters_[0] - displacement[0]);
            for (size_t i = 0; i < rows.size(); ++i) {
                const auto& instrument = rows[i]->Instrument();
                const double high = PrepareCalibrationMc(instrument, "Price", up, settings.sampling_).Evaluate(instrument, up, false).mean_;
                const double low = PrepareCalibrationMc(instrument, "Price", down, settings.sampling_).Evaluate(instrument, down, false).mean_;
                const double fd = (high - low) / (2. * h);
                ASSERT_NEAR(result.quoteJacobian_(i, j), fd, 2e-6);
                differences.push_back(fd);
            }
        }
        for (size_t i = 0; i < rows.size(); ++i)
            ASSERT_NEAR(differences[i], differences[i + rows.size()], 2e-6);
    }
}

TEST(GSRScriptCalibrationTest, TestBasketDifferencesAndScaleDoNotOverwriteRawRows) {
    auto settings = FitSettings();
    settings.derivativeMethod_ = CalibrationDerivative_::Value_::FiniteDifference;
    const auto result = CalibrateGSRScriptPrices(FitContext(), FitRows(settings), FitParameters(), settings);
    const auto aad = CalibrateGSRScriptPrices(FitContext(), FitRows(FitSettings()), FitParameters(), FitSettings());
    ASSERT_TRUE(result.fitWithinTolerance_);
    ASSERT_EQ(result.costs_.aadRows_, 0);
    ASSERT_EQ(result.derivativeMethod_, CalibrationDerivative_::Value_::FiniteDifference);
    ASSERT_EQ(result.costs_.doubleRows_, result.costs_.basketEvaluations_ * 2);
    for (int i = 0; i < 2; ++i)
        for (int j = 0; j < 2; ++j)
            ASSERT_NEAR(result.quoteJacobian_(i, j), aad.quoteJacobian_(i, j), 2e-4);
    settings.optimizer_.parameterScale_ = .02;
    const auto scaled = CalibrateGSRScriptPrices(FitContext(.02, .025), FitRows(settings, .2), FitParameters(), settings);
    for (int i = 0; i < 2; ++i)
        for (int j = 0; j < 2; ++j)
            ASSERT_NEAR(scaled.quoteJacobian_(i, j), result.quoteJacobian_(i, j), 2e-4);
}

TEST(GSRScriptCalibrationTest, TestFitNumericsAndHeldOutFailIndependently) {
    const auto settings = FitSettings();
    const auto rows = FitRows(settings, 1e-5);
    const auto badHeld = NewCalibrationObservation("held", rows[0]->Instrument(), "Price",
                                                   NewCalibrationMarketQuote("held", 1000., CalibrationValueUnit_::Parse("Price:USD")), .1);
    const auto result = CalibrateGSRScriptPrices(FitContext(.02, .025), rows, FitParameters(), settings, {badHeld});
    ASSERT_TRUE(result.converged_);
    ASSERT_TRUE(result.fitWithinTolerance_);
    ASSERT_FALSE(result.numericalValidationPassed_);
    ASSERT_TRUE(result.heldOutRequested_);
    ASSERT_FALSE(result.heldOutPassed_);
    ASSERT_EQ(result.heldOut_[0].ranges_[0].firstPath_, 640);
    ASSERT_EQ(result.heldOut_[0].pathsPerView_, 512);
    ASSERT_NEAR(result.modelPrices_[0], rows[0]->Quote()->Value(), 1e-12);
    ASSERT_NE(result.modelPrices_[0], result.validation_[0].mean_);
    ASSERT_EQ(result.costs_.paths_, (result.costs_.doubleRows_ + result.costs_.aadRows_ - 3) * 128 + 1536);
}

TEST(GSRScriptCalibrationTest, TestBoundsRankAndOptimizerStatusAreSeparateFromFit) {
    auto settings = FitSettings();
    auto parameters = FitParameters();
    parameters[0].upper_ = .018;
    parameters[1].upper_ = .015;
    const auto rows = FitRows(settings, 1e-5);
    const auto result = CalibrateGSRScriptPrices(FitContext(), rows, parameters, settings);
    ASSERT_TRUE(result.converged_);
    ASSERT_FALSE(result.fitWithinTolerance_);
    ASSERT_TRUE(result.activeBounds_[0]);
    ASSERT_TRUE(result.activeBounds_[1]);
    const auto rank = CalibrateGSRScriptPrices(FitContext(), {rows[0]}, FitParameters(), settings);
    ASSERT_EQ(rank.jacobianRank_, 1);
    ASSERT_TRUE(std::isinf(rank.jacobianConditionEstimate_));
    settings.optimizer_.maxIterations_ = 1;
    const auto limited = CalibrateGSRScriptPrices(FitContext(), rows, FitParameters(), settings);
    ASSERT_FALSE(limited.converged_);
    ASSERT_EQ(limited.terminationReason_, "MaxIterations");
}

TEST(GSRScriptCalibrationTest, TestBudgetFailureOwnsObservationAxes) {
    auto settings = FitSettings();
    settings.maxBasketEvaluations_ = 1;
    Handle_<CalibrationReport_> report;
    try {
        static_cast<void>(CalibrateGSRScriptPrices(FitContext(), FitRows(settings), FitParameters(), settings));
        FAIL() << "expected frozen basket budget failure";
    } catch (const CalibrationError_& error) {
        ASSERT_EQ(error.Failure().diagnostic_.code_, "CalibrationBudgetExceeded");
        ASSERT_EQ(error.Failure().diagnostic_.stage_, CalibrationStage_::Value_::Solve);
        ASSERT_TRUE(error.Failure().candidate_);
        report = error.Failure().report_;
    }
    ASSERT_TRUE(report);
    ASSERT_EQ(report->Axes().observations_.size(), 2);
}

TEST(GSRScriptCalibrationTest, TestNegativeVanillaTargetIsRejected) {
    const auto settings = FitSettings();
    const auto original = FitRows(settings)[0];
    const auto row = NewCalibrationObservation("negative", original->Instrument(), "Price",
                                               NewCalibrationMarketQuote("negative", -1., CalibrationValueUnit_::Parse("Price:USD")), .1);
    ASSERT_THROW(static_cast<void>(CalibrateGSRScriptPrices(FitContext(), {row}, FitParameters(), settings)), CalibrationError_);
}

TEST(GSRScriptCalibrationTest, TestNoDescentReturnsCandidateWithIndependentFlags) {
    auto settings = FitSettings();
    settings.optimizer_.priorWeight_ = 1e8;
    settings.optimizer_.gradientTolerance_ = settings.optimizer_.stepTolerance_ = 1e-30;
    const auto result = CalibrateGSRScriptPrices(FitContext(), FitRows(settings, 1e-5), FitParameters(), settings);
    ASSERT_FALSE(result.converged_);
    ASSERT_EQ(result.terminationReason_, "NoDescent");
    ASSERT_FALSE(result.fitWithinTolerance_);
    ASSERT_TRUE(result.model_);
}

TEST(GSRScriptCalibrationTest, TestFrozenNumericalBudgetCanPassWhileFitFails) {
    auto settings = FitSettings();
    settings.sampling_.fitBatches_ = 8;
    settings.sampling_.fitPathsPerBatch_ = 8192;
    settings.sampling_.validationBatches_ = settings.sampling_.heldOutBatches_ = 16;
    settings.sampling_.validationPathsPerBatch_ = settings.sampling_.heldOutPathsPerBatch_ = 16384;
    settings.sampling_.maxPathsPerView_ = 589824;
    const auto rows = FitRows(settings);
    const auto result = CalibrateGSRScriptPrices(FitContext(), {rows[0]}, {{0, 0, .001, .015}}, settings);
    ASSERT_TRUE(result.converged_);
    ASSERT_FALSE(result.fitWithinTolerance_);
    ASSERT_TRUE(result.numericalValidationPassed_);
    ASSERT_FALSE(result.heldOutRequested_);
    ASSERT_FALSE(result.heldOutPassed_);
    ASSERT_EQ(result.fit_[0].pathsPerView_, 65536);
    ASSERT_EQ(result.validation_[0].pathsPerView_, 262144);
    const double volatility = .02 * (366. / 365.);
    const double d1 = std::log(std::exp(-.04) / .97) / volatility + .5 * volatility;
    const auto cdf = [](double x) { return .5 * std::erfc(-x / std::sqrt(2.)); };
    const double oracle = 100. * (std::exp(-.08) * cdf(d1) - .97 * std::exp(-.04) * cdf(d1 - volatility));
    const auto held = NewCalibrationObservation("oracle-held", rows[0]->Instrument(), "Price",
                                                NewCalibrationMarketQuote("oracle-held", oracle, CalibrationValueUnit_::Parse("Price:USD")), .1);
    const auto root = CalibrateGSRScriptPrices(FitContext(.02, .025), {rows[0]}, {{0, 0, .001, .06}}, settings, {held});
    ASSERT_TRUE(root.fitWithinTolerance_);
    ASSERT_TRUE(root.numericalValidationPassed_);
    ASSERT_TRUE(root.heldOutPassed_);
    const auto& value = root.heldOut_[0];
    ASSERT_LE(std::abs(value.mean_ - oracle) + 3. * value.standardError_, .1);
}

TEST(GSRScriptCalibrationTest, TestFeasibleFiniteDifferenceColumnsAreReported) {
    auto settings = FitSettings();
    settings.derivativeMethod_ = CalibrationDerivative_::Value_::FiniteDifference;
    const auto rows = FitRows(settings, 1e-5);
    const auto result = CalibrateGSRScriptPrices(FitContext(), rows, {{0, 1, .001, .018}, {0, 0, .001, .015}}, settings);
    ASSERT_EQ(result.oneSidedColumns_, (Vector_<bool>{true, true}));
    ASSERT_TRUE(result.activeBounds_[0]);
    ASSERT_TRUE(result.activeBounds_[1]);
    ASSERT_TRUE(std::isfinite(result.quoteJacobian_(0, 1)));
}

TEST(GSRScriptCalibrationTest, TestStepStalledAndActualRowBatchCounts) {
    auto settings = FitSettings();
    const auto rows = FitRows(settings);
    settings.optimizer_.stepTolerance_ = 100.;
    const auto stalled = CalibrateGSRScriptPrices(FitContext(), rows, FitParameters(), settings);
    ASSERT_FALSE(stalled.converged_);
    ASSERT_EQ(stalled.terminationReason_, "StepStalled");
    const auto root = CalibrateGSRScriptPrices(FitContext(.02, .025), rows, FitParameters(), settings);
    ASSERT_EQ(root.costs_.basketEvaluations_, 5);
    ASSERT_EQ(root.costs_.jacobians_, 2);
    ASSERT_EQ(root.costs_.doubleRows_, 2);
    ASSERT_EQ(root.costs_.aadRows_, 8);
    ASSERT_EQ(root.costs_.batchRuns_, 24);
    ASSERT_EQ(root.costs_.paths_, 2048);
    settings.derivativeMethod_ = CalibrationDerivative_::Value_::FiniteDifference;
    const auto fd = CalibrateGSRScriptPrices(FitContext(.02, .025), rows, FitParameters(), settings);
    ASSERT_EQ(fd.costs_.basketEvaluations_, 11);
    ASSERT_EQ(fd.costs_.aadRows_, 0);
    ASSERT_EQ(fd.costs_.doubleRows_, 22);
    ASSERT_EQ(fd.costs_.batchRuns_, 48);
    ASSERT_EQ(fd.costs_.paths_, 3584);
}

TEST(GSRScriptCalibrationTest, TestNestedTapeFailurePreservesCallerAndOwnsReport) {
    const auto settings = FitSettings();
    const auto rows = FitRows(settings);
    {
        AAD::RecordingScope_ recording;
        AAD::Number_ x;
        recording.RegisterInput(x, 2.);
        recording.StartRecording();
        ASSERT_THROW(static_cast<void>(CalibrateGSRScriptPrices(FitContext(), rows, FitParameters(), settings)), CalibrationError_);
        AAD::Number_ y = x * x;
        recording.FinishRecording();
        recording.ClearAdjoints();
        Adjoint(y) = 1.;
        recording.Reverse();
        ASSERT_DOUBLE_EQ(Adjoint(x), 4.);
    }
    ASSERT_TRUE(CalibrateGSRScriptPrices(FitContext(), rows, FitParameters(), settings).fitWithinTolerance_);
    ASSERT_FALSE(AAD::LastRecordingCleanupFailure());
}

TEST(GSRScriptCalibrationTest, TestNormalizedBoundsStayFeasibleInNativeUnits) {
    const auto settings = FitSettings();
    const auto source = FitRows(settings)[0];
    for (bool upper : {false, true}) {
        const double target = upper ? source->Quote()->Value() + 1. : 0.;
        const auto row = NewCalibrationObservation("bound", source->Instrument(), "Price",
                                                   NewCalibrationMarketQuote("bound", target, CalibrationValueUnit_::Parse("Price:USD")), .1);
        const GSRCalibrationParameter_ parameter{0, 0, upper ? .001 : .027, upper ? .031 : .06};
        const auto result = CalibrateGSRScriptPrices(FitContext(.03), {row}, {parameter}, settings);
        ASSERT_TRUE(result.activeBounds_[0]);
        ASSERT_GE(result.parameters_[0], parameter.lower_);
        ASSERT_LE(result.parameters_[0], parameter.upper_);
        ASSERT_DOUBLE_EQ(result.model_->vol_->gValues_(0, 0), result.parameters_[0]);
    }
}

TEST(GSRScriptCalibrationTest, TestUnresolvedNativeStepNamesParameterAndStage) {
    auto settings = FitSettings();
    settings.derivativeMethod_ = CalibrationDerivative_::Value_::FiniteDifference;
    settings.optimizer_.finiteDifferenceStep_ = 1e-20;
    try {
        static_cast<void>(CalibrateGSRScriptPrices(FitContext(), FitRows(settings), FitParameters(), settings));
        FAIL() << "expected unrepresentable native step";
    } catch (const CalibrationError_& error) {
        const auto& d = error.Failure().diagnostic_;
        ASSERT_EQ(d.code_, "DerivativeUnavailable");
        ASSERT_EQ(d.stage_, CalibrationStage_::Value_::Solve);
        ASSERT_TRUE(d.parameterKey_);
        ASSERT_EQ(*d.parameterKey_, "g:level:2027-10-21");
        ASSERT_TRUE(error.Failure().candidate_);
        ASSERT_EQ(error.Failure().report_->Axes().observations_.size(), 2);
    }
}

namespace {
    Handle_<CalibrationObservation_> RepairRow(const String_& id, const Handle_<CalibrationInstrument_>& instrument, double target = 0.) {
        return NewCalibrationObservation(id, instrument, "Price",
                                         NewCalibrationMarketQuote(id + "-quote", target, CalibrationValueUnit_::Parse("Price:USD")), .1);
    }
    Handle_<CalibrationInstrument_> RepairRefinedAverage() {
        auto source = AverageDefinition();
        const auto rows = source.source_.rows_;
        source.source_.rows_ = {rows[0],
                                rows[1],
                                {ScriptSourceRowKind_::Value_::Event, "2027-04-21", "clock=0"},
                                rows[2],
                                {ScriptSourceRowKind_::Value_::Event, "2028-01-21", "clock=0"},
                                rows[3],
                                {ScriptSourceRowKind_::Value_::Event, "2028-07-21", "clock=0"},
                                rows[4]};
        return NewScriptCalibrationInstrument({"six-dimension", NewScriptInstrumentDefinition(source)});
    }
    Handle_<CalibrationInstrument_> RepairBond() {
        return NewBondOptionScriptInstrument(BondOption_{AVG_F1, AVG_PAY, .97, OptionType_("CALL")}, {"one-dimension", Ccy_("USD"), "USD.OIS", 100.});
    }
    template <class F_> std::optional<CalibrationError_> RepairFailure(const F_& action) {
        try {
            action();
        } catch (const CalibrationError_& error) {
            return error;
        }
        return {};
    }
    void AssertRepairRow(const CalibrationError_& error, const String_& rowId, CalibrationStage_ stage, size_t count, size_t ordinal) {
        const auto& f = error.Failure();
        const auto& d = f.diagnostic_;
        ASSERT_EQ(d.stage_, stage);
        ASSERT_EQ(d.observationId_, std::optional<String_>(rowId));
        ASSERT_EQ(d.quoteId_, std::optional<String_>(rowId + "-quote"));
        ASSERT_EQ(d.measureId_, std::optional<String_>("Price"));
        ASSERT_TRUE(f.report_);
        ASSERT_EQ(f.report_->Stage(), stage);
        const auto& axes = f.report_->Axes();
        ASSERT_EQ(axes.observations_.size(), count);
        ASSERT_EQ(axes.quotes_.size(), count);
        ASSERT_EQ(axes.observations_[ordinal]->Id(), rowId);
        ASSERT_EQ(axes.quotes_[ordinal]->Id(), rowId + "-quote");
        ASSERT_EQ(axes.quoteIndex_[ordinal], static_cast<int>(ordinal));
        ASSERT_EQ(f.report_->Errors().size(), 1);
        ASSERT_EQ(f.report_->Errors()[0].observationId_, d.observationId_);
        ASSERT_EQ(f.report_->Errors()[0].message_, d.message_);
    }
} // namespace

TEST(GSRScriptRepairTest, TestMixedFitDimensionsRejectActualDrawOverlap) {
    const auto context = AverageContext();
    const auto settings = FitSettings();
    const auto high = RepairRefinedAverage(), low = RepairBond();
    const auto highPlan = PrepareCalibrationMc(high, "Price", context, settings.sampling_);
    const auto lowPlan = PrepareCalibrationMc(low, "Price", context, settings.sampling_);
    ASSERT_EQ(highPlan.Mapping().dimension_, 6);
    ASSERT_EQ(lowPlan.Mapping().dimension_, 1);
    ASSERT_EQ(highPlan.Mapping().normalCoordinates_.size(), 6);
    for (size_t draw : {size_t(128), size_t(640)}) {
        auto wide = New(RNGType_("MRG32"), settings.sampling_.seed_, 6, true);
        auto narrow = New(RNGType_("MRG32"), settings.sampling_.seed_, 1, true);
        Vector_<> z(6), w(1);
        wide->SkipNormalTo(draw / 6);
        narrow->SkipNormalTo(draw);
        wide->FillNormal(&z);
        narrow->FillNormal(&w);
        ASSERT_LT(draw / 6, 128);
        ASSERT_DOUBLE_EQ(z[draw % 6], w[0]);
    }
    for (bool reverse : {false, true}) {
        const auto rows = Vector_<Handle_<CalibrationObservation_>>{RepairRow("first-stream", reverse ? low : high),
                                                                    RepairRow("second-stream", reverse ? high : low)};
        const auto error = RepairFailure([&] { static_cast<void>(CalibrateGSRScriptPrices(context, rows, {{0, 0, .001, .06}}, settings)); });
        ASSERT_TRUE(error);
        ASSERT_EQ(error->Failure().diagnostic_.code_, "InvalidCalibrationPlan");
        ASSERT_EQ(error->Failure().diagnostic_.field_, std::optional<String_>("/mc/mapping/dimension"));
        AssertRepairRow(*error, "second-stream", CalibrationStage_::Value_::Prepare, 2, 1);
        ASSERT_FALSE(error->Failure().candidate_);
    }
}

TEST(GSRScriptRepairTest, TestMixedHeldOutDimensionsRejectBeforeSolverWork) {
    const auto context = AverageContext();
    const auto settings = FitSettings();
    const auto error = RepairFailure([&] {
        static_cast<void>(CalibrateGSRScriptPrices(context, {RepairRow("fit-stream", RepairRefinedAverage())}, {{0, 0, .001, .06}}, settings,
                                                   {RepairRow("held-stream", RepairBond())}));
    });
    ASSERT_TRUE(error);
    ASSERT_EQ(error->Failure().diagnostic_.code_, "InvalidCalibrationPlan");
    ASSERT_EQ(error->Failure().diagnostic_.field_, std::optional<String_>("/mc/mapping/dimension"));
    AssertRepairRow(*error, "held-stream", CalibrationStage_::Value_::Prepare, 1, 0);
    ASSERT_FALSE(error->Failure().candidate_);
    ASSERT_NE(error->Failure().diagnostic_.message_.find("held-out"), String_::npos);
}

TEST(GSRScriptRepairTest, TestSameDimensionDrawReservationsAndIdentityStayFixed) {
    const auto context = AverageContext();
    const auto settings = FitSettings();
    const auto instrument = AverageInstrument("same-dimension");
    auto sampling = settings.sampling_;
    sampling.aad_ = true;
    sampling.parameterLabels_ = CalibrationMcGLabels(context.model_, {{0, 0, .001, .06}});
    const auto plan = PrepareCalibrationMc(instrument, "Price", context, sampling);
    const auto fit = plan.Evaluate(instrument, context, false);
    const auto validation = plan.Evaluate(instrument, context, true);
    const auto held = plan.EvaluateHeldOut(instrument, context);
    const size_t dimension = plan.Mapping().dimension_;
    ASSERT_EQ(dimension, 3);
    ASSERT_EQ(fit.ranges_.front().firstPath_, 0);
    ASSERT_EQ(validation.ranges_.front().firstPath_, 128);
    ASSERT_EQ(held.ranges_.front().firstPath_, 640);
    ASSERT_LE((fit.ranges_.back().firstPath_ + fit.ranges_.back().pathCount_) * dimension, validation.ranges_.front().firstPath_ * dimension);
    ASSERT_LE((validation.ranges_.back().firstPath_ + validation.ranges_.back().pathCount_) * dimension, held.ranges_.front().firstPath_ * dimension);
    const auto result =
        CalibrateGSRScriptPrices(context, {RepairRow("same-first", instrument, fit.mean_), RepairRow("same-second", instrument, fit.mean_)},
                                 {{0, 0, .001, .06}}, settings, {RepairRow("same-held", instrument, fit.mean_)});
    ASSERT_TRUE(result.fitWithinTolerance_);
    ASSERT_TRUE(result.heldOutRequested_);
    ASSERT_EQ(result.fit_[0].mappingIdentity_, plan.Mapping().identity_);
    ASSERT_EQ(result.fit_[0].planIdentity_, fit.planIdentity_);
    ASSERT_EQ(result.fit_[0].mean_, fit.mean_);
    ASSERT_EQ(result.validation_[0].ranges_[0].firstPath_, 128);
    ASSERT_EQ(result.heldOut_[0].ranges_[0].firstPath_, 640);
    ASSERT_EQ(plan.RefreshG(AverageContext(.021)).Mapping().identity_, plan.Mapping().identity_);
}

TEST(GSRScriptRepairTest, TestPrepareFailureOwnsFitOrHeldOutRowAfterInputsDie) {
    for (bool held : {false, true}) {
        std::optional<CalibrationError_> saved;
        {
            auto source = AverageDefinition();
            source.source_.rows_.back().text_ = "EXERCISE 1 pay PAYS N*MAX(avg-K,0)";
            const auto bad = NewScriptCalibrationInstrument({"bad-prepare", NewScriptInstrumentDefinition(source)});
            const auto good = AverageInstrument("good-prepare");
            const auto first = RepairRow("prepare-first", good), second = RepairRow("prepare-second", bad);
            const auto settings = FitSettings();
            saved = RepairFailure([&] {
                if (held)
                    static_cast<void>(
                        CalibrateGSRScriptPrices(AverageContext(), {RepairRow("prepare-fit", good)}, {{0, 0, .001, .06}}, settings, {first, second}));
                else
                    static_cast<void>(CalibrateGSRScriptPrices(AverageContext(), {first, second}, {{0, 0, .001, .06}}, settings));
            });
        }
        ASSERT_TRUE(saved);
        AssertRepairRow(*saved, "prepare-second", CalibrationStage_::Value_::Prepare, 2, 1);
        ASSERT_EQ(saved->Failure().diagnostic_.instrumentId_, std::optional<String_>("bad-prepare"));
        ASSERT_EQ(saved->Failure().diagnostic_.code_, "UnsupportedCalibrationProduct");
        ASSERT_EQ(saved->Failure().diagnostic_.sourceRow_, std::optional<int>(5));
        ASSERT_EQ(saved->Failure().diagnostic_.eventDate_, std::optional<Date_>(AVG_PAY));
        ASSERT_FALSE(saved->Failure().candidate_);
    }
}

TEST(GSRScriptRepairTest, TestExecutionFailureOwnsItsScopeAndLastCompleteCandidate) {
    for (bool held : {false, true}) {
        std::optional<CalibrationError_> saved, original;
        {
            const auto context = AverageContext();
            const auto settings = FitSettings();
            const auto good = AverageInstrument("good-execution");
            auto source = AverageDefinition();
            source.source_.rows_.back().text_ = "pay PAYS N/(avg-avg)";
            const auto bad = NewScriptCalibrationInstrument({"bad-execution", NewScriptInstrumentDefinition(source)});
            const auto plan = PrepareCalibrationMc(good, "Price", context, settings.sampling_);
            const double target = plan.Evaluate(good, context, false).mean_;
            original = RepairFailure(
                [&] { static_cast<void>(PrepareCalibrationMc(bad, "Price", context, settings.sampling_).Evaluate(bad, context, false)); });
            const auto first = RepairRow("execution-first", good, target), second = RepairRow("execution-second", bad);
            saved = RepairFailure([&] {
                if (held)
                    static_cast<void>(CalibrateGSRScriptPrices(context, {RepairRow("execution-fit", good, target)}, {{0, 0, .001, .06}}, settings,
                                                               {first, second}));
                else
                    static_cast<void>(CalibrateGSRScriptPrices(context, {first, second}, {{0, 0, .001, .06}}, settings));
            });
        }
        ASSERT_TRUE(saved);
        ASSERT_TRUE(original);
        const auto& f = saved->Failure();
        AssertRepairRow(*saved, "execution-second", held ? CalibrationStage_::Value_::Validate : CalibrationStage_::Value_::Solve, 2, 1);
        ASSERT_EQ(f.diagnostic_.instrumentId_, std::optional<String_>("bad-execution"));
        ASSERT_EQ(f.diagnostic_.code_, original->Failure().diagnostic_.code_);
        ASSERT_EQ(f.diagnostic_.field_, original->Failure().diagnostic_.field_);
        ASSERT_EQ(f.diagnostic_.sourceRow_, original->Failure().diagnostic_.sourceRow_);
        ASSERT_EQ(f.diagnostic_.eventDate_, original->Failure().diagnostic_.eventDate_);
        ASSERT_EQ(f.diagnostic_.statement_, original->Failure().diagnostic_.statement_);
        ASSERT_NE(f.diagnostic_.message_.find(original->Failure().diagnostic_.message_), String_::npos);
        if (held) {
            ASSERT_NE(f.diagnostic_.message_.find("held-out"), String_::npos);
            const auto* model = dynamic_cast<const MultiFactorGSRModelData_*>(f.candidate_.get());
            ASSERT_NE(model, nullptr);
            ASSERT_DOUBLE_EQ(model->vol_->gValues_(0, 0), .02);
        } else {
            ASSERT_FALSE(f.candidate_);
        }
        ASSERT_EQ(AAD::Tape()->nodes_.Size(), 0);
        ASSERT_FALSE(AAD::LastRecordingCleanupFailure());
    }
}

TEST(GSRScriptRepairTest, TestRefreshFailureRetainsOnlyACompletePriceableCandidate) {
    std::optional<CalibrationError_> saved;
    {
        auto source = RepairBond()->Definition()->Spec();
        source.source_.rows_.back().text_ = "pay PAYS N*MAX(FIX(IR[USD,DF,2028-10-21])-K,0)-1";
        const auto instrument = NewScriptCalibrationInstrument({"refresh-boundary", NewScriptInstrumentDefinition(source)});
        saved = RepairFailure([&] {
            static_cast<void>(
                CalibrateGSRScriptPrices(AverageContext(), {RepairRow("refresh-row", instrument, -100.)}, {{0, 0, 0., .06}}, FitSettings()));
        });
    }
    ASSERT_TRUE(saved);
    AssertRepairRow(*saved, "refresh-row", CalibrationStage_::Value_::Solve, 1, 0);
    const auto& f = saved->Failure();
    ASSERT_EQ(f.diagnostic_.code_, "DerivativeUnavailable");
    ASSERT_EQ(f.diagnostic_.field_, std::optional<String_>("/mc/parameter_labels"));
    ASSERT_EQ(f.diagnostic_.instrumentId_, std::optional<String_>("refresh-boundary"));
    const auto* model = dynamic_cast<const MultiFactorGSRModelData_*>(f.candidate_.get());
    ASSERT_NE(model, nullptr);
    ASSERT_DOUBLE_EQ(model->vol_->gValues_(0, 0), 0.);
    auto context = AverageContext();
    context.model_ = handle_cast<ModelData_>(f.candidate_);
    const auto& instrument = f.report_->Axes().observations_[0]->Instrument();
    const auto sampling = FitSettings().sampling_;
    const double value = PrepareCalibrationMc(instrument, "Price", context, sampling).Evaluate(instrument, context, false).mean_;
    ASSERT_NEAR(value, -std::exp(-.12 * AverageTime(AVG_F1) / AverageTime(AVG_END)), 1e-12);
    ASSERT_EQ(AAD::Tape()->nodes_.Size(), 0);
    ASSERT_FALSE(AAD::LastRecordingCleanupFailure());
}

TEST(GSRScriptRepairTest, TestMissingHeldOutSamplingOwnsTheRequestedHeldOutAxis) {
    auto settings = FitSettings();
    settings.sampling_.heldOutBatches_ = settings.sampling_.heldOutPathsPerBatch_ = 0;
    const auto instrument = AverageInstrument();
    const auto error = RepairFailure([&] {
        static_cast<void>(CalibrateGSRScriptPrices(AverageContext(), {RepairRow("sampling-fit", instrument)}, {{0, 0, .001, .06}}, settings,
                                                   {RepairRow("sampling-held", instrument)}));
    });
    ASSERT_TRUE(error);
    AssertRepairRow(*error, "sampling-held", CalibrationStage_::Value_::Prepare, 1, 0);
    ASSERT_EQ(error->Failure().diagnostic_.field_, std::optional<String_>("/mc/held_out"));
    ASSERT_FALSE(error->Failure().candidate_);
}

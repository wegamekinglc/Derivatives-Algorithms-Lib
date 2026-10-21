#include <gtest/gtest.h>

#include <climits>
#include <functional>

#include <dal/math/aad/recording.hpp>
#include <dal/math/random/pseudorandom.hpp>
#include <dal/model/gsrcalibration.hpp>
#include <dal/model/gsrmultidata.hpp>
#include <dal/script/calibrationmc.hpp>
#include <dal/script/calibrationmc_internal.hpp>
#include <dal/time/dateutils.hpp>

using namespace Dal;

namespace {
    ScriptInstrumentDefinitionSpec_ McSpec(bool option = false) {
        const auto price = CalibrationValueUnit_::Parse("Price:USD");
        ScriptInstrumentDefinitionSpec_ s;
        s.header_.payCurrency_ = s.header_.resultCurrency_ = s.header_.collateralCurrency_ = Ccy_("USD");
        s.header_.discountRoute_ = "USD.OIS";
        s.source_.rows_ = {{ScriptSourceRowKind_::Value_::Definition, "N", "100"},
                           {ScriptSourceRowKind_::Value_::Definition, "K", option ? "0.97" : "0.025"}};
        s.parameters_ = {{"N", ScriptParameterType_::Value_::Real, price, ScriptParameterRole_::Value_::Notional, true, 0., {}, false, true},
                         {"K", ScriptParameterType_::Value_::Real, CalibrationValueUnit_::Parse(option ? "BondPrice" : "Rate"),
                          option ? ScriptParameterRole_::Value_::Strike : ScriptParameterRole_::Value_::Coupon, true}};
        s.measures_ = {{"Price", CalibrationMeasureKind_::Value_::Price, "PAY", {}, price}};
        if (option) {
            s.parameters_[1].lower_ = 0.;
            s.parameters_[1].lowerInclusive_ = false;
            s.source_.rows_.push_back({ScriptSourceRowKind_::Value_::Event, "2027-10-21", "pay PAYS N*MAX(FIX(IR[USD,DF,2028-10-21])-K,0)"});
        } else {
            s.source_.rows_.push_back({ScriptSourceRowKind_::Value_::Definition, "ALPHA", "1.2"});
            s.source_.rows_.push_back({ScriptSourceRowKind_::Value_::Event, "2026-10-21", "pay PAYS -N"});
            s.source_.rows_.push_back({ScriptSourceRowKind_::Value_::Event, "2027-10-21", "pay PAYS N*(1+K*ALPHA)"});
            s.parameters_.push_back({"ALPHA",
                                     ScriptParameterType_::Value_::Real,
                                     CalibrationValueUnit_::Parse("YearFraction"),
                                     ScriptParameterRole_::Value_::Accrual,
                                     false,
                                     0.,
                                     {},
                                     false,
                                     true});
            s.measures_.push_back(
                {"ParRate", CalibrationMeasureKind_::Value_::ParAffineRate, "PAY", String_("K"), CalibrationValueUnit_::Parse("Rate"), 1, 1e-12});
        }
        return s;
    }
    Handle_<CalibrationInstrument_> McInstrument(bool option = false) {
        return NewScriptCalibrationInstrument({option ? "option" : "deposit", NewScriptInstrumentDefinition(McSpec(option))});
    }
    CalibrationPreparationContext_ McContext(double g = 0.02, double logDiscount = -0.04) {
        CalibrationPreparationContext_ c;
        const Date_ v(2026, 10, 21);
        c.currency_ = Ccy_("USD");
        c.discountRoute_ = "USD.OIS";
        c.valuation_.evaluationDate_ = v;
        c.valuation_.fixings_.reset(new MarketFixingSnapshot_());
        c.curve_.reset(new GSRCurveData_("curve", v, "USD", {v, Date_(2027, 10, 21), Date_(2028, 10, 21)}, {0., logDiscount, -0.08}, {}, {}));
        Handle_<MultiFactorGSRVolData_> vol(
            new MultiFactorGSRVolData_("vol", {"level"}, {v}, Matrix_<>(1, 1, g), {v}, Matrix_<>(1, 1, 1.), Matrix_<>(1, 1, 1.)));
        c.model_.reset(new MultiFactorGSRModelData_("model", c.curve_, vol));
        return c;
    }
    CalibrationMcSettings_ McSettings(bool aad = false, bool compiled = false) {
        CalibrationMcSettings_ s;
        s.allowMonteCarloCalibration_ = true;
        s.fitBatches_ = 2;
        s.validationBatches_ = 2;
        s.fitPathsPerBatch_ = 8;
        s.validationPathsPerBatch_ = 16;
        s.maxPathsPerView_ = 48;
        s.aad_ = aad;
        s.compiled_ = compiled;
        s.retainPaths_ = true;
        if (aad)
            s.parameterLabels_ = {"g:level:2026-10-21", "logdf:OIS:2027-10-21"};
        return s;
    }
    CalibrationMcSettings_ NumericalSettings() {
        auto s = McSettings(true, true);
        s.fitBatches_ = 8;
        s.fitPathsPerBatch_ = 8192;
        s.validationBatches_ = 16;
        s.validationPathsPerBatch_ = 16384;
        s.maxPathsPerView_ = 327680;
        s.retainPaths_ = false;
        return s;
    }
    double OptionOracle(double g, bool call = true, double strike = 0.97, double notional = 100.) {
        const double tau = 366. / 365., sigma = g * tau;
        const double de = std::exp(-0.04);
        const double maturityDiscount = std::exp(-0.08);
        const double forward = maturityDiscount / de;
        const double d1 = std::log(forward / strike) / sigma + 0.5 * sigma, d2 = d1 - sigma;
        const double direction = call ? 1. : -1.;
        const auto cdf = [](double x) { return 0.5 * std::erfc(-x / std::sqrt(2.)); };
        return notional * direction * (maturityDiscount * cdf(direction * d1) - strike * de * cdf(direction * d2));
    }
    double FastOptionPrice(double g, bool call = true) {
        const auto context = McContext(g);
        const auto& model = static_cast<const MultiFactorGSRModelData_&>(*context.model_);
        return 100. *
               PriceGSREuropeanOption(model, BondOption_{Date_(2027, 10, 21), Date_(2028, 10, 21), 0.97, OptionType_(call ? "CALL" : "PUT")}).price_;
    }
    CalibrationMeasureValue_
    FastDepositValue(const Handle_<CalibrationInstrument_>& instrument, const CalibrationPreparationContext_& context, bool rate) {
        const auto plan = PrepareCalibrationInstrument(instrument, rate ? "ParRate" : "Price", context);
        const double discount = std::exp(-0.04);
        Matrix_<> partials(2, 2, 0.);
        partials(1, 1) = discount;
        const auto coefficients = plan.Coefficients({1., discount}, partials);
        if (rate)
            return plan.AffineMeasure(coefficients);
        const double k = std::get<CalibrationDepositDescriptor_>(plan.Descriptor()).coupon_;
        Vector_<> gradient(2);
        for (size_t j = 0; j < 2; ++j)
            gradient[j] = coefficients.db_[j] + k * coefficients.da_[j];
        return {plan.Price({1., discount}), gradient};
    }
    Handle_<CalibrationInstrument_> BackendRebound(const Handle_<CalibrationInstrument_>& instrument, bool option) {
        return BindCalibrationInstrumentParameters(instrument,
                                                   {{{"K", option ? 0.95 : 0.04, CalibrationValueUnit_::Parse(option ? "BondPrice" : "Rate")},
                                                     {"N", 200., CalibrationValueUnit_::Parse("Price:USD")}}});
    }
    void AssertReboundDescriptor(const PreparedCalibrationInstrument_& plan, bool option) {
        if (option) {
            const auto& d = std::get<CalibrationBondOptionDescriptor_>(plan.Descriptor());
            ASSERT_DOUBLE_EQ(d.notional_, 200.);
            ASSERT_DOUBLE_EQ(d.strike_, 0.95);
        } else {
            const auto& d = std::get<CalibrationDepositDescriptor_>(plan.Descriptor());
            ASSERT_DOUBLE_EQ(d.notional_, 200.);
            ASSERT_DOUBLE_EQ(d.coupon_, 0.04);
        }
    }
} // namespace

TEST(CalibrationMcTest, TestSameBindingTreeAndCompiledPathPayoffs) {
    const auto instrument = McInstrument(true);
    const auto context = McContext();
    const auto tree = PrepareCalibrationMc(instrument, "Price", context, McSettings());
    const auto compiled = PrepareCalibrationMc(instrument, "Price", context, McSettings(false, true));
    const auto a = tree.Evaluate(instrument, context, false);
    const auto b = compiled.Evaluate(instrument, context, false);
    ASSERT_EQ(a.pathValues_.size(), 16);
    ASSERT_EQ(a.pathValues_.size(), b.pathValues_.size());
    for (size_t i = 0; i < a.pathValues_.size(); ++i)
        ASSERT_NEAR(a.pathValues_[i], b.pathValues_[i], 1e-12);
    ASSERT_EQ(tree.Capability().backend_, CalibrationBackend_::Value_::ScriptMC);
    ASSERT_EQ(tree.Capability().derivativeMethod_, CalibrationDerivative_::Value_::None);
}

TEST(CalibrationMcTest, TestAadMatchesHardPathsCrnDifferencesAndRawJacobian) {
    const auto instrument = McInstrument(true);
    const auto context = McContext();
    const auto settings = McSettings(true);
    const auto tree = PrepareCalibrationMc(instrument, "Price", context, settings).Evaluate(instrument, context, false);
    const auto compiled = PrepareCalibrationMc(instrument, "Price", context, McSettings(true, true)).Evaluate(instrument, context, false);
    ASSERT_EQ(tree.gradient_.size(), 2);
    ASSERT_EQ(tree.pathDerivatives_.Rows(), 16);
    ASSERT_EQ(tree.quoteJacobian_.Rows(), 1);
    ASSERT_EQ(tree.quoteJacobian_.Cols(), 2);
    ASSERT_EQ(tree.parameterLabels_, settings.parameterLabels_);
    for (int j = 0; j < 2; ++j) {
        ASSERT_NEAR(tree.gradient_[j], compiled.gradient_[j], 1e-10);
        ASSERT_DOUBLE_EQ(tree.quoteJacobian_(0, j), tree.gradient_[j]);
        double average = 0.;
        for (int i = 0; i < 16; ++i) {
            ASSERT_NEAR(tree.pathDerivatives_(i, j), compiled.pathDerivatives_(i, j), 1e-10);
            average += tree.pathDerivatives_(i, j) / 16.;
        }
        ASSERT_NEAR(tree.gradient_[j], average, 1e-10);
    }
    for (double h : {1e-4, 5e-5}) {
        const auto up = McContext(0.02 + h), down = McContext(0.02 - h);
        const auto u = PrepareCalibrationMc(instrument, "Price", up, McSettings()).Evaluate(instrument, up, false);
        const auto d = PrepareCalibrationMc(instrument, "Price", down, McSettings()).Evaluate(instrument, down, false);
        for (int i = 0; i < 16; ++i)
            ASSERT_NEAR(tree.pathDerivatives_(i, 0), (u.pathValues_[i] - d.pathValues_[i]) / (2. * h), 1e-5);
    }
}

TEST(CalibrationMcTest, TestMissingDuplicateAndCaseMismatchedLabelsFail) {
    const auto instrument = McInstrument(true);
    const auto context = McContext();
    for (const Vector_<String_>& labels :
         {Vector_<String_>{"g:0:2026-10-21"}, Vector_<String_>{"g:LEVEL:2026-10-21"}, Vector_<String_>{"g:level:2026-10-21", "g:level:2026-10-21"}}) {
        auto settings = McSettings(true);
        settings.parameterLabels_ = labels;
        ASSERT_THROW(PrepareCalibrationMc(instrument, "Price", context, settings), CalibrationError_);
    }
}

TEST(CalibrationMcTest, TestAffinePoolsCoefficientsAndUsesFullCovariance) {
    const Vector_<CalibrationAffineCoefficients_> batches{{1., 1., {0.2}, {0.1}}, {2., 4., {0.5}, {0.3}}, {3., 9., {0.7}, {1.}}};
    const auto estimate = CalibrationMcInternal::AffineEstimate(batches, 1, 0.1, 3.);
    const double a = 2., b = 14. / 3., da = 1.4 / 3., db = 1.4 / 3.;
    ASSERT_NEAR(estimate.value_.value_, -b / a, 1e-12);
    ASSERT_GT(std::abs(estimate.value_.value_ + 2.), 0.3);
    ASSERT_NEAR(estimate.value_.derivatives_[0], -db / a + b * da / (a * a), 1e-12);
    double valueVariance = 0., gradientVariance = 0.;
    for (const auto& x : batches) {
        const double v = -(x.b_ - b) / a + b * (x.a_ - a) / (a * a);
        const double g =
            da / (a * a) * (x.b_ - b) + (db / (a * a) - 2. * b * da / (a * a * a)) * (x.a_ - a) - (x.db_[0] - db) / a + b * (x.da_[0] - da) / (a * a);
        valueVariance += v * v / 6.;
        gradientVariance += g * g / 6.;
    }
    ASSERT_NEAR(estimate.standardError_, std::sqrt(valueVariance), 1e-12);
    ASSERT_NEAR(estimate.gradientStandardError_[0], std::sqrt(gradientVariance), 1e-12);
    ASSERT_NEAR(estimate.coefficientCovariance_(0, 1), 4., 1e-12);
    ASSERT_THROW(CalibrationMcInternal::AffineEstimate(batches, 1, 0.5, 3.), CalibrationError_);
}

TEST(CalibrationMcTest, TestDeterministicAffineSameDefinitionExecution) {
    const auto instrument = McInstrument();
    const auto context = McContext(0.);
    const auto result = PrepareCalibrationMc(instrument, "ParRate", context, McSettings()).Evaluate(instrument, context, false);
    ASSERT_NEAR(result.mean_, (std::exp(0.04) - 1.) / 1.2, 1e-12);
    ASSERT_NEAR(result.standardError_, 0., 1e-12);
    ASSERT_EQ(result.engineRuns_, 4);
    ASSERT_TRUE(result.affine_);
    ASSERT_NEAR(result.affine_->meanCoefficients_.a_, 120. * std::exp(-0.04), 1e-12);
    ASSERT_NEAR(result.affine_->meanCoefficients_.b_, 100. * (std::exp(-0.04) - 1.), 1e-12);
}

TEST(CalibrationMcTest, TestValidationRechecksAnnuityAtRequestedConfidence) {
    const Vector_<CalibrationAffineCoefficients_> batches{{1., 1., {0.2}, {0.1}}, {2., 4., {0.5}, {0.3}}, {3., 9., {0.7}, {1.}}};
    const auto estimate = CalibrationMcInternal::AffineEstimate(batches, 1, 0.1, 3.);
    CalibrationMcResult_ validation;
    validation.validation_ = true;
    validation.batches_ = Vector_<CalibrationMcBatchValue_>(3);
    validation.parameterLabels_ = {"g:x"};
    validation.mean_ = estimate.value_.value_;
    validation.standardError_ = estimate.standardError_;
    validation.gradient_ = estimate.value_.derivatives_;
    validation.gradientStandardError_ = estimate.gradientStandardError_;
    validation.affine_ = estimate;
    CalibrationMcPrecision_ precision;
    precision.errorScale_ = 10.;
    precision.sigma_ = 4.;
    precision.parameterScales_ = {0.01};
    precision.epsilonGradient_ = {1e-7};
    ASSERT_THROW(ValidateCalibrationMc(validation, {validation.mean_, 0., 0., validation.gradient_, {0.}, {0.}}, precision), CalibrationError_);
}

TEST(CalibrationMcTest, TestValidationEnforcesErrorPrecisionGradientAndDisjointStreams) {
    const auto instrument = McInstrument(true);
    const auto context = McContext();
    const auto plan = PrepareCalibrationMc(instrument, "Price", context, McSettings(true));
    const auto fit = plan.Evaluate(instrument, context, false);
    const auto validation = plan.Evaluate(instrument, context, true);
    CalibrationMcPrecision_ precision;
    precision.errorScale_ = 0.1;
    precision.parameterScales_ = {0.01, 1.};
    precision.epsilonGradient_ = {1e-7, 1e-7};
    const auto report = ValidateCalibrationMc(fit, validation, precision);
    ASSERT_FALSE(report.numericalValidationPassed_);
    ASSERT_TRUE(report.budgetExhausted_);
    ASSERT_EQ(report.terminationReason_, "NumericalValidationFailed");
    ASSERT_NEAR(report.deltaStandardError_, std::hypot(fit.standardError_, validation.standardError_), 1e-12);
    ASSERT_THROW(ValidateCalibrationMc(fit, fit, precision), CalibrationError_);
    auto bad = precision;
    bad.epsilonValue_ = 0.1;
    ASSERT_THROW(ValidateCalibrationMc(fit, validation, bad), CalibrationError_);
}

TEST(CalibrationMcTest, TestRangesMatchSequentialNormalsAndIndependentPathOracle) {
    const auto instrument = McInstrument(true);
    const auto context = McContext();
    const auto plan = PrepareCalibrationMc(instrument, "Price", context, McSettings());
    const auto fit = plan.Evaluate(instrument, context, false), val = plan.Evaluate(instrument, context, true);
    ASSERT_EQ(plan.Batches(false)[0].firstPath_, 0);
    ASSERT_EQ(plan.Batches(false)[1].firstPath_, 8);
    ASSERT_EQ(plan.Batches(true)[0].firstPath_, 16);
    ASSERT_EQ(plan.Batches(true)[1].firstPath_, 32);
    ASSERT_EQ(plan.Mapping().dimension_, 1);
    ASSERT_EQ(plan.Mapping().normalCoordinates_.size(), 1);
    ASSERT_EQ(plan.Mapping().timeline_, Vector_<>{1.});
    auto random = New(RNGType_("MRG32"), 1024, 1, true);
    const double tau = 366. / 365., logForward = -0.04;
    for (size_t i = 0; i < 48; ++i) {
        Vector_<> z(1);
        random->FillNormal(&z);
        const double bond = std::exp(logForward - tau * (0.02 * z[0] + 0.0002) - 0.5 * tau * tau * 0.0004);
        const double numeraire = std::exp(0.04 + 0.00005 + 0.01 * z[0]);
        const double payoff = 100. * std::max(bond - 0.97, 0.) / numeraire;
        ASSERT_NEAR(i < 16 ? fit.pathValues_[i] : val.pathValues_[i - 16], payoff, 1e-11);
    }
    const auto again = plan.Evaluate(instrument, context, true);
    ASSERT_EQ(again.pathValues_, val.pathValues_);
    auto different = McSpec(true);
    different.source_.rows_.back().key_ = "2027-10-22";
    const auto other = NewScriptCalibrationInstrument({"different", NewScriptInstrumentDefinition(different)});
    ASSERT_NE(PrepareCalibrationMc(other, "Price", context, McSettings()).Mapping().identity_, plan.Mapping().identity_);
}

TEST(CalibrationMcTest, TestRebindingKAndNChangesAllBackendsAndPreservesOldPlans) {
    for (bool option : {false, true}) {
        const auto instrument = McInstrument(option);
        const auto context = McContext();
        const auto rebound = BackendRebound(instrument, option);
        Vector_<> reference;
        for (bool aad : {false, true})
            for (bool compiled : {false, true}) {
                const auto settings = McSettings(aad, compiled);
                const auto old = PrepareCalibrationMc(instrument, "Price", context, settings);
                const auto before = old.Evaluate(instrument, context, false);
                const auto fresh = PrepareCalibrationMc(rebound, "Price", context, settings).Evaluate(rebound, context, false);
                ASSERT_EQ(old.Evaluate(instrument, context, false).pathValues_, before.pathValues_);
                ASSERT_THROW(static_cast<void>(old.Evaluate(rebound, context, false)), CalibrationError_);
                ASSERT_GT(std::abs(fresh.mean_ - before.mean_), 0.01);
                if (reference.empty())
                    reference = fresh.pathValues_;
                for (size_t i = 0; i < reference.size(); ++i)
                    ASSERT_NEAR(fresh.pathValues_[i], reference[i], 1e-10);
            }
        AssertReboundDescriptor(PrepareCalibrationInstrument(rebound, "Price", context), option);
    }
}

TEST(CalibrationMcTest, TestContextChangesRequirePreparation) {
    const auto instrument = McInstrument(true);
    const auto context = McContext();
    const auto plan = PrepareCalibrationMc(instrument, "Price", context, McSettings());
    const Vector_<std::function<void(CalibrationPreparationContext_*)>> changes{
        [](auto* c) { c->discountRoute_ = "other"; },
        [](auto* c) { c->smoothing_ = 0.01; },
        [](auto* c) { c->valuation_.evaluationDate_ = Date_(2026, 10, 22); },
        [](auto* c) { c->valuation_.todayFixingPolicy_ = TodayFixingPolicy_::Value_::REQUIREHISTORICAL; },
        [](auto* c) { *c = McContext(0.021); },
        [](auto* c) { c->currency_ = Ccy_("EUR"); },
        [](auto* c) { c->valuation_.fixings_.reset(new MarketFixingSnapshot_({{"EQ[OTHER]", {{DateTime_(Date_(2026, 10, 20), 0.), 1.}}}})); }};
    for (const auto& change : changes) {
        auto c = McContext();
        change(&c);
        ASSERT_THROW(static_cast<void>(plan.Evaluate(instrument, c, false)), CalibrationError_);
    }
    auto smooth = context;
    smooth.smoothing_ = 0.01;
    ASSERT_THROW(PrepareCalibrationMc(instrument, "Price", smooth, McSettings(true)), CalibrationError_);
}

TEST(CalibrationMcTest, TestSamplingRequiresOptInBatchesSeedAndBudget) {
    const auto instrument = McInstrument(true);
    const auto context = McContext();
    auto denied = McSettings(), lowBudget = McSettings(), oneBatch = McSettings(), badSeed = McSettings();
    denied.allowMonteCarloCalibration_ = false;
    lowBudget.maxPathsPerView_ = 47;
    oneBatch.fitBatches_ = 1;
    badSeed.seed_ = -1;
    for (const auto& s : {denied, lowBudget, oneBatch, badSeed})
        ASSERT_THROW(PrepareCalibrationMc(instrument, "Price", context, s), CalibrationError_);
}

TEST(CalibrationMcTest, TestGCoordinatesResolveOriginalNamesAndRejectDuplicateRuntimeLabels) {
    auto context = McContext();
    const auto date = *context.valuation_.evaluationDate_;
    Matrix_<> g(2, 1), h(2, 1), correlation(2, 2, 0.3);
    g(0, 0) = 0.02;
    g(1, 0) = 0.01;
    h(0, 0) = 1.;
    h(1, 0) = 0.4;
    correlation(0, 0) = correlation(1, 1) = 1.;
    Handle_<MultiFactorGSRVolData_> vol(new MultiFactorGSRVolData_("vol", {"level", "slope"}, {date}, g, {date}, h, correlation));
    context.model_.reset(new MultiFactorGSRModelData_("model", context.curve_, vol));
    const auto labels = CalibrationMcGLabels(context.model_, {{1, 0}, {0, 0}});
    ASSERT_EQ(labels, (Vector_<String_>{"g:slope:2026-10-21", "g:level:2026-10-21"}));
    ASSERT_THROW(CalibrationMcGLabels(context.model_, {{2, 0}}), CalibrationError_);
    ASSERT_THROW(CalibrationMcGLabels(context.model_, {{0, 1}}), CalibrationError_);
    ASSERT_THROW(CalibrationMcGLabels(context.model_, {{0, 0}, {0, 0}}), CalibrationError_);
    ASSERT_THROW(CalibrationMcParameterColumns({"g:x", "g:x"}, {"g:x"}), CalibrationError_);
    auto s = McSettings(true);
    s.parameterLabels_ = labels;
    const auto instrument = McInstrument(true);
    const auto tree = PrepareCalibrationMc(instrument, "Price", context, s).Evaluate(instrument, context, false);
    s.compiled_ = true;
    const auto compiled = PrepareCalibrationMc(instrument, "Price", context, s).Evaluate(instrument, context, false);
    ASSERT_EQ(tree.parameterLabels_, labels);
    for (int j = 0; j < 2; ++j)
        ASSERT_NEAR(tree.gradient_[j], compiled.gradient_[j], 1e-10);
}

TEST(CalibrationMcTest, TestTapeCleanupAfterPathFailureAndNestedScopeRejection) {
    auto spec = McSpec(true);
    spec.source_.rows_[1].text_ = "1e308";
    spec.source_.rows_.back().text_ = "pay PAYS N*MAX(K-FIX(IR[USD,DF,2028-10-21]),0)";
    const auto bad = NewScriptCalibrationInstrument({"overflow", NewScriptInstrumentDefinition(spec)});
    const auto instrument = McInstrument(true);
    const auto context = McContext();
    const auto plan = PrepareCalibrationMc(instrument, "Price", context, McSettings(true));
    const auto before = plan.Evaluate(instrument, context, false);
    const auto overflow = PrepareCalibrationMc(bad, "Price", context, McSettings(true));
    try {
        static_cast<void>(overflow.Evaluate(bad, context, false));
        FAIL() << "expected path overflow";
    } catch (const CalibrationError_& error) {
        ASSERT_EQ(error.Failure().diagnostic_.code_, "CalibrationNumericalFailure");
        ASSERT_EQ(error.Failure().report_->Stage(), CalibrationStage_::Value_::Evaluate);
    }
    ASSERT_EQ(plan.Evaluate(instrument, context, false).gradient_, before.gradient_);
    {
        AAD::RecordingScope_ recording;
        AAD::Number_ x;
        recording.RegisterInput(x, 2.);
        recording.StartRecording();
        ASSERT_THROW(static_cast<void>(plan.Evaluate(instrument, context, false)), CalibrationError_);
        AAD::Number_ y = x * x;
        recording.FinishRecording();
        recording.ClearAdjoints();
        Adjoint(y) = 1.;
        recording.Reverse();
        ASSERT_DOUBLE_EQ(Adjoint(x), 4.);
    }
    ASSERT_EQ(plan.Evaluate(instrument, context, false).gradient_, before.gradient_);
    ASSERT_FALSE(AAD::LastRecordingCleanupFailure());
}

TEST(CalibrationMcTest, TestDegenerateOptionDerivativeIsUnavailable) {
    const auto instrument = McInstrument(true);
    const auto context = McContext(0.);
    ASSERT_THROW(PrepareCalibrationMc(instrument, "Price", context, McSettings(true)), CalibrationError_);
    ASSERT_NO_THROW(PrepareCalibrationMc(instrument, "Price", context, McSettings()));
}

TEST(CalibrationMcTest, TestMultiAdjointModeIsRejectedWithoutChangingCallerMode) {
    const auto instrument = McInstrument(true);
    const auto context = McContext();
    const auto plan = PrepareCalibrationMc(instrument, "Price", context, McSettings(true));
    {
        const auto reset = AAD::SetNumResultsForAAD(true, 2);
        std::optional<CalibrationError_> saved;
        try {
            static_cast<void>(plan.Evaluate(instrument, context, false));
        } catch (const CalibrationError_& error) {
            saved = error;
        }
        ASSERT_TRUE(saved);
        ASSERT_EQ(saved->Failure().diagnostic_.code_, "DerivativeUnavailable");
        ASSERT_EQ(saved->Failure().report_->Stage(), CalibrationStage_::Value_::Evaluate);
        ASSERT_TRUE(AAD::Tape()->multi_);
        ASSERT_EQ(AAD::Tape()->numAdj_, 2);
    }
    ASSERT_NO_THROW(static_cast<void>(plan.Evaluate(instrument, context, false)));
}

TEST(CalibrationMcTest, TestAffineTreeCompiledAadCrnAndRebinding) {
    const auto instrument = McInstrument();
    const auto context = McContext();
    const auto tree = PrepareCalibrationMc(instrument, "ParRate", context, McSettings(true)).Evaluate(instrument, context, false);
    const auto compiled = PrepareCalibrationMc(instrument, "ParRate", context, McSettings(true, true)).Evaluate(instrument, context, false);
    ASSERT_NEAR(tree.mean_, compiled.mean_, 1e-12);
    ASSERT_TRUE(tree.pathValues_.empty());
    for (size_t j = 0; j < 2; ++j) {
        ASSERT_NEAR(tree.gradient_[j], compiled.gradient_[j], 1e-10);
        for (double h : {1e-4, 5e-5}) {
            const auto up = j == 0 ? McContext(0.02 + h) : McContext(0.02, -0.04 + h);
            const auto down = j == 0 ? McContext(0.02 - h) : McContext(0.02, -0.04 - h);
            const auto u = PrepareCalibrationMc(instrument, "ParRate", up, McSettings()).Evaluate(instrument, up, false);
            const auto d = PrepareCalibrationMc(instrument, "ParRate", down, McSettings()).Evaluate(instrument, down, false);
            ASSERT_NEAR(tree.gradient_[j], (u.mean_ - d.mean_) / (2. * h), 1e-7);
        }
    }
    const auto rebound = BindCalibrationInstrumentParameters(
        instrument, {{{"K", -0.01, CalibrationValueUnit_::Parse("Rate")}, {"N", 200., CalibrationValueUnit_::Parse("Price:USD")}}});
    const auto fresh = PrepareCalibrationMc(rebound, "ParRate", context, McSettings(true)).Evaluate(rebound, context, false);
    ASSERT_NEAR(fresh.mean_, tree.mean_, 1e-12);
    for (size_t j = 0; j < 2; ++j)
        ASSERT_NEAR(fresh.gradient_[j], tree.gradient_[j], 1e-10);
}

TEST(CalibrationMcTest, TestZeroSeedAndExplicitCountBoundaries) {
    const auto instrument = McInstrument(true);
    const auto context = McContext();
    auto settings = McSettings();
    settings.seed_ = 0;
    const auto plan = PrepareCalibrationMc(instrument, "Price", context, settings);
    ASSERT_EQ(plan.Evaluate(instrument, context, false).pathValues_, plan.Evaluate(instrument, context, false).pathValues_);
    settings.fitPathsPerBatch_ = 1;
    ASSERT_THROW(PrepareCalibrationMc(instrument, "Price", context, settings), CalibrationError_);
    settings = McSettings();
    settings.maxPathsPerView_ = static_cast<size_t>(INT_MAX) + 1;
    ASSERT_THROW(PrepareCalibrationMc(instrument, "Price", context, settings), CalibrationError_);
}

TEST(CalibrationMcNumericalTest, TestFrozenBondOptionPriceAndGradientBudget) {
    const auto instrument = McInstrument(true);
    const auto context = McContext();
    auto settings = NumericalSettings();
    settings.parameterLabels_ = {"g:level:2026-10-21"};
    const auto plan = PrepareCalibrationMc(instrument, "Price", context, settings);
    const auto fit = plan.Evaluate(instrument, context, false), val = plan.Evaluate(instrument, context, true);
    const auto& model = static_cast<const MultiFactorGSRModelData_&>(*context.model_);
    const BondOption_ native{Date_(2027, 10, 21), Date_(2028, 10, 21), 0.97, OptionType_("CALL")};
    const double fast = 100. * PriceGSREuropeanOption(model, native).price_;
    const double oracle = OptionOracle(0.02);
    ASSERT_NEAR(fast, oracle, 1e-10);
    for (double g : {0.0199, 0.01995, 0.02005, 0.0201})
        ASSERT_NEAR(FastOptionPrice(g), OptionOracle(g), 1e-10);
    const double fd = (FastOptionPrice(0.0201) - FastOptionPrice(0.0199)) / 0.0002;
    const double fdHalf = (FastOptionPrice(0.02005) - FastOptionPrice(0.01995)) / 0.0001;
    const double sigma = 0.02 * 366. / 365.;
    const double d1 = std::log(std::exp(-0.04) / 0.97) / sigma + 0.5 * sigma;
    const double gradient = 100. * std::exp(-0.08) * std::exp(-0.5 * d1 * d1) / std::sqrt(2. * std::acos(-1.)) * 366. / 365.;
    ASSERT_NEAR(fdHalf, gradient, std::abs(fd - fdHalf) + 1e-7);
    CalibrationMcPrecision_ precision;
    precision.errorScale_ = 0.1;
    precision.parameterScales_ = {0.01};
    precision.epsilonGradient_ = {1e-7};
    const auto report = ValidateCalibrationMc(val, {fast, 0., 0., {fdHalf}, {0.}, {std::abs(fd - fdHalf)}}, precision);
    ASSERT_TRUE(report.numericalValidationPassed_) << "price=" << val.mean_ << " oracle=" << oracle << " SE=" << val.standardError_
                                                   << " g=" << val.gradient_[0] << " ref=" << gradient << " SEg=" << val.gradientStandardError_[0];
    ASSERT_TRUE(ValidateCalibrationMc(fit, val, precision).numericalValidationPassed_);
    ASSERT_EQ(val.pathsPerView_, 262144);
    ASSERT_EQ(val.engineRuns_, 16);
    ASSERT_GT(val.peakTapeNodes_, 0);
    ASSERT_GT(val.peakTapeBytes_, 0);
    auto halfSettings = settings;
    halfSettings.validationBatches_ = 8;
    const auto half = PrepareCalibrationMc(instrument, "Price", context, halfSettings).Evaluate(instrument, context, true);
    ASSERT_TRUE(ValidateCalibrationMc(half, {oracle, 0., 0., {fdHalf}, {0.}, {std::abs(fd - fdHalf)}}, precision).numericalValidationPassed_);
    ASSERT_LT(std::abs(half.mean_ - val.mean_), 0.025);
    RecordProperty("price", val.mean_);
    RecordProperty("price_se", val.standardError_);
    RecordProperty("g", val.gradient_[0]);
    RecordProperty("g_se", val.gradientStandardError_[0]);
    RecordProperty("refinement_delta", std::abs(half.mean_ - val.mean_));
    RecordProperty("tape_nodes", static_cast<int>(val.peakTapeNodes_));
    RecordProperty("tape_bytes", static_cast<int>(val.peakTapeBytes_));
    RecordProperty("fast_fd", fd);
    RecordProperty("fast_fd_half", fdHalf);
}

TEST(CalibrationMcNumericalTest, TestFrozenDepositPriceAndRateGradientBudgets) {
    const auto instrument = McInstrument();
    const auto context = McContext();
    const auto settings = NumericalSettings();
    for (bool rate : {false, true}) {
        const auto measure = rate ? "ParRate" : "Price";
        const std::string prefix = rate ? "rate" : "price";
        const auto plan = PrepareCalibrationMc(instrument, measure, context, settings);
        const auto fit = plan.Evaluate(instrument, context, false), val = plan.Evaluate(instrument, context, true);
        const double discount = std::exp(-0.04);
        const double oracle = rate ? (1. / discount - 1.) / 1.2 : -100. + 103. * discount;
        const Vector_<> gradients{0., rate ? -1. / (1.2 * discount) : 103. * discount};
        const auto fast = FastDepositValue(instrument, context, rate);
        ASSERT_NEAR(fast.value_, oracle, 1e-12);
        for (size_t j = 0; j < gradients.size(); ++j)
            ASSERT_NEAR(fast.derivatives_[j], gradients[j], 1e-10);
        CalibrationMcPrecision_ precision;
        precision.errorScale_ = rate ? 0.001 : 0.1;
        precision.parameterScales_ = {0.01, 1.};
        precision.epsilonGradient_ = {1e-7, 1e-7};
        const auto report = ValidateCalibrationMc(val, {fast.value_, 0., 0., fast.derivatives_, {0., 0.}, {0., 0.}}, precision);
        ASSERT_TRUE(report.numericalValidationPassed_)
            << measure << " mean=" << val.mean_ << " oracle=" << oracle << " SE=" << val.standardError_ << " g=" << val.gradient_[0]
            << " SEg=" << val.gradientStandardError_[0] << " logdf=" << val.gradient_[1] << " expected=" << gradients[1]
            << " SEdf=" << val.gradientStandardError_[1];
        ASSERT_TRUE(ValidateCalibrationMc(fit, val, precision).numericalValidationPassed_);
        auto halfSettings = settings;
        halfSettings.validationBatches_ = 8;
        const auto half = PrepareCalibrationMc(instrument, measure, context, halfSettings).Evaluate(instrument, context, true);
        ASSERT_TRUE(ValidateCalibrationMc(half, {oracle, 0., 0., gradients, {0., 0.}, {0., 0.}}, precision).numericalValidationPassed_);
        ASSERT_LT(std::abs(half.mean_ - val.mean_), 0.25 * precision.errorScale_);
        RecordProperty(prefix, val.mean_);
        RecordProperty(prefix + "_se", val.standardError_);
        RecordProperty(prefix + "_g", val.gradient_[0]);
        RecordProperty(prefix + "_g_se", val.gradientStandardError_[0]);
        RecordProperty(prefix + "_refinement_delta", std::abs(half.mean_ - val.mean_));
        RecordProperty(prefix + "_logdf", val.gradient_[1]);
        RecordProperty(prefix + "_logdf_se", val.gradientStandardError_[1]);
        RecordProperty(prefix + "_tape_nodes", static_cast<int>(val.peakTapeNodes_));
    }
}

TEST(CalibrationMcNumericalTest, TestFrozenPutUsesSameHardTargetAndBudget) {
    auto spec = McSpec(true);
    spec.source_.rows_.back().text_ = "pay PAYS N*MAX(K-FIX(IR[USD,DF,2028-10-21]),0)";
    const auto instrument = NewScriptCalibrationInstrument({"put", NewScriptInstrumentDefinition(spec)});
    const auto context = McContext();
    auto settings = NumericalSettings();
    settings.parameterLabels_ = {"g:level:2026-10-21"};
    const auto val = PrepareCalibrationMc(instrument, "Price", context, settings).Evaluate(instrument, context, true);
    const auto& model = static_cast<const MultiFactorGSRModelData_&>(*context.model_);
    const double fast = 100. * PriceGSREuropeanOption(model, BondOption_{Date_(2027, 10, 21), Date_(2028, 10, 21), 0.97, OptionType_("PUT")}).price_;
    ASSERT_NEAR(fast, OptionOracle(0.02, false), 1e-10);
    for (double g : {0.0199, 0.01995, 0.02005, 0.0201})
        ASSERT_NEAR(FastOptionPrice(g, false), OptionOracle(g, false), 1e-10);
    const double fd = (FastOptionPrice(0.0201, false) - FastOptionPrice(0.0199, false)) / 0.0002;
    const double fdHalf = (FastOptionPrice(0.02005, false) - FastOptionPrice(0.01995, false)) / 0.0001;
    CalibrationMcPrecision_ precision;
    precision.errorScale_ = 0.1;
    precision.parameterScales_ = {0.01};
    precision.epsilonGradient_ = {1e-7};
    ASSERT_TRUE(ValidateCalibrationMc(val, {fast, 0., 0., {fdHalf}, {0.}, {std::abs(fd - fdHalf)}}, precision).numericalValidationPassed_);
    RecordProperty("price", val.mean_);
    RecordProperty("price_se", val.standardError_);
    RecordProperty("g", val.gradient_[0]);
    RecordProperty("g_se", val.gradientStandardError_[0]);
}

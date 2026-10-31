#include <gtest/gtest.h>

#include <cmath>
#include <limits>

#include <dal/curve/depositscript.hpp>
#include <dal/curve/jointrate.hpp>
#include <dal/math/aad/recording.hpp>
#include <dal/script/calibrationmc.hpp>

using namespace Dal;

namespace {
    const Date_ VALUE_DATE(2026, 10, 21), END_DATE(2027, 10, 21);
    constexpr double ALPHA = 365. / 360.;
    Handle_<CalibrationInstrument_> ImportedDeposit() {
        const Deposit_ native(VALUE_DATE, END_DATE, .025, DayBasis_("ACT_360"));
        return NewDepositScriptInstrument(native, {"deposit", Ccy_("USD"), "USD.OIS", 100.});
    }
    Handle_<GSRCurveData_> Candidate(double theta = -.04) {
        return Handle_<GSRCurveData_>(new GSRCurveData_("candidate", VALUE_DATE, "USD", {VALUE_DATE, END_DATE}, {0., theta}, {}, {}));
    }
    CalibrationPreparationContext_ CurveContext(double theta = -.04) {
        CalibrationPreparationContext_ c;
        c.valuation_.evaluationDate_ = VALUE_DATE;
        c.valuation_.fixings_.reset(new MarketFixingSnapshot_());
        c.currency_ = Ccy_("USD");
        c.discountRoute_ = "USD.OIS";
        c.curve_ = Candidate(theta);
        return c;
    }
    CalibrationPreparationContext_ ModelContext(double theta = -.04, double g = .02) {
        auto c = CurveContext(theta);
        Handle_<GSRVolData_> vol(new GSRVolData_("vol", {VALUE_DATE}, {g}, {VALUE_DATE}, {1.}));
        c.model_.reset(new GSRModelData_("model", c.curve_, vol));
        return c;
    }
    CalibrationMcSettings_ SmallSettings(bool compiled = true) {
        CalibrationMcSettings_ s;
        s.allowMonteCarloCalibration_ = true;
        s.aad_ = true;
        s.compiled_ = compiled;
        s.retainPaths_ = true;
        s.fitBatches_ = s.validationBatches_ = 2;
        s.fitPathsPerBatch_ = 8;
        s.validationPathsPerBatch_ = 16;
        s.maxPathsPerView_ = 48;
        s.parameterLabels_ = {"logdf:OIS:2027-10-21", "g:2026-10-21"};
        return s;
    }
    CalibrationMcSettings_ NumericalSettings() {
        auto s = SmallSettings();
        s.retainPaths_ = false;
        s.fitBatches_ = 8;
        s.validationBatches_ = 16;
        s.fitPathsPerBatch_ = 8192;
        s.validationPathsPerBatch_ = 16384;
        s.maxPathsPerView_ = 327680;
        return s;
    }
    CalibrationMcPrecision_ Precision(bool rate) {
        CalibrationMcPrecision_ p;
        p.errorScale_ = rate ? .001 : .1;
        p.parameterScales_ = {1., .01};
        p.epsilonGradient_ = {1e-7, 1e-7};
        return p;
    }
    CalibrationMcReference_ Oracle(double theta, bool rate) {
        const double discount = std::exp(theta), redemption = 100. * (1. + .025 * ALPHA);
        const double value = rate ? (std::exp(-theta) - 1.) / ALPHA : -100. + redemption * discount;
        const double derivative = rate ? -std::exp(-theta) / ALPHA : redemption * discount;
        return {value, 0., 0., {derivative, 0.}, {0., 0.}, {0., 0.}};
    }
    void AssertCrnDifferences(const Handle_<CalibrationInstrument_>& instrument, const String_& measure, int column, double derivative) {
        for (double h : {1e-4, 5e-5}) {
            const double thetaBump = column == 0 ? h : 0., gBump = column == 1 ? h : 0.;
            const auto up = ModelContext(-.04 + thetaBump, .02 + gBump), down = ModelContext(-.04 - thetaBump, .02 - gBump);
            const auto u = PrepareCalibrationMc(instrument, measure, up, SmallSettings()).Evaluate(instrument, up, false);
            const auto d = PrepareCalibrationMc(instrument, measure, down, SmallSettings()).Evaluate(instrument, down, false);
            ASSERT_NEAR((u.mean_ - d.mean_) / (2. * h), derivative, 1e-6);
            ASSERT_EQ(u.mappingIdentity_, d.mappingIdentity_);
        }
    }
    void AssertRatioCovariance(const CalibrationMcResult_& result) {
        ASSERT_TRUE(result.affine_);
        const auto& estimate = *result.affine_;
        const double b = estimate.meanCoefficients_.b_, a = estimate.meanCoefficients_.a_;
        const double w0 = -1. / a, w1 = b / (a * a);
        const auto& covariance = estimate.coefficientCovariance_;
        const double variance =
            (w0 * w0 * covariance(0, 0) + 2. * w0 * w1 * covariance(0, 1) + w1 * w1 * covariance(1, 1)) / static_cast<double>(result.batches_.size());
        ASSERT_GT(covariance(0, 1), 0.);
        ASSERT_NEAR(result.mean_, -b / a, 1e-12);
        ASSERT_NEAR(result.standardError_, std::sqrt(variance), 1e-12);
        ASSERT_GT(a - 3. * estimate.annuityStandardError_, estimate.minAbsAnnuity_);
    }
    std::optional<CalibrationError_> NumericalFitFailure(double upper, double scale, double lower = -.1) {
        const auto instrument = ImportedDeposit();
        const PreparedDepositCurve_ plan(instrument, CurveContext());
        const auto quote = NewCalibrationMarketQuote("q", .035, CalibrationValueUnit_::Parse("Rate"));
        const auto observation = NewCalibrationObservation("fit", instrument, "ParRate", quote, scale);
        try {
            static_cast<void>(FitDepositCurveNode(plan, observation, Candidate(), 1, lower, upper, 64));
        } catch (const CalibrationError_& error) {
            return error;
        }
        return {};
    }
    void AssertRetainedFitFailure(const CalibrationError_& error, std::optional<double> theta, int evaluations) {
        const auto& failure = error.Failure();
        const auto& d = failure.diagnostic_;
        ASSERT_EQ(d.code_, "CalibrationNumericalFailure");
        ASSERT_EQ(d.stage_, CalibrationStage_::Value_::Solve);
        ASSERT_EQ(d.instrumentId_, std::optional<String_>("deposit"));
        ASSERT_EQ(d.observationId_, std::optional<String_>("fit"));
        ASSERT_EQ(d.quoteId_, std::optional<String_>("q"));
        ASSERT_EQ(d.measureId_, std::optional<String_>("ParRate"));
        ASSERT_TRUE(failure.report_);
        ASSERT_EQ(failure.report_->Stage(), CalibrationStage_::Value_::Solve);
        const auto& axes = failure.report_->Axes();
        ASSERT_EQ(axes.observations_.size(), 1);
        ASSERT_EQ(axes.quotes_.size(), 1);
        ASSERT_EQ(axes.observations_[0]->Id(), "fit");
        ASSERT_EQ(axes.quotes_[0]->Id(), "q");
        ASSERT_DOUBLE_EQ(axes.quotes_[0]->Value(), .035);
        ASSERT_EQ(axes.quoteIndex_, Vector_<int>{0});
        ASSERT_EQ(axes.quoteMap_.Rows(), 1);
        ASSERT_EQ(axes.quoteMap_.Cols(), 1);
        ASSERT_DOUBLE_EQ(axes.quoteMap_(0, 0), 1.);
        ASSERT_EQ(failure.report_->Errors().size(), 1);
        ASSERT_EQ(failure.report_->Errors()[0].message_, d.message_);
        double raw = 0.;
        if (theta) {
            const auto* curve = dynamic_cast<const GSRCurveData_*>(failure.candidate_.get());
            ASSERT_NE(curve, nullptr);
            ASSERT_EQ(curve->nodeDates_, (Vector_<Date_>{VALUE_DATE, END_DATE}));
            ASSERT_EQ(curve->discountLogDF_.size(), 2);
            ASSERT_NEAR(curve->discountLogDF_[1], *theta, 1e-12);
            const PreparedDepositCurve_ plan(axes.observations_[0]->Instrument(), CurveContext());
            const Handle_<GSRCurveData_> retained(std::dynamic_pointer_cast<const GSRCurveData_>(failure.candidate_));
            const auto value = plan.Evaluate(retained);
            ASSERT_TRUE(std::isfinite(value.price_.derivatives_[0]));
            raw = value.parRate_.value_ - axes.quotes_[0]->Value();
            ::testing::Test::RecordProperty("retained_theta", curve->discountLogDF_[1]);
        } else {
            ASSERT_FALSE(failure.candidate_);
        }
        ASSERT_TRUE(d.actual_);
        const std::string actual(d.actual_->data(), d.actual_->size());
        for (const auto& [key, expected] : std::initializer_list<std::pair<std::string, double>>{
                 {"evaluations=", static_cast<double>(evaluations)}, {"raw=", raw}, {"scaled=", raw / axes.observations_[0]->ErrorScale()}}) {
            const auto start = actual.find(key);
            ASSERT_NE(start, std::string::npos) << actual;
            ASSERT_NEAR(std::stod(actual.substr(start + key.size())), expected, std::max(1e-12, std::abs(expected) * 1e-12));
        }
        ::testing::Test::RecordProperty("owned_solve_diagnostic", std::string(error.what()));
    }
} // namespace

TEST(DepositScriptTest, TestImportFreezesConcreteLenderCashflows) {
    const auto instrument = ImportedDeposit();
    const auto plan = PrepareCalibrationInstrument(instrument, "Price", CurveContext());
    const auto& d = std::get<CalibrationDepositDescriptor_>(plan.Descriptor());
    ASSERT_EQ(d.start_, VALUE_DATE);
    ASSERT_EQ(d.maturity_, END_DATE);
    ASSERT_DOUBLE_EQ(d.notional_, 100.);
    ASSERT_DOUBLE_EQ(d.coupon_, .025);
    ASSERT_DOUBLE_EQ(d.accrual_, ALPHA);
    ASSERT_EQ(plan.Cashflows().size(), 2);
    ASSERT_DOUBLE_EQ(*plan.Cashflows()[0].amount_, -100.);
    ASSERT_NEAR(*plan.Cashflows()[1].amount_, 100. * (1. + .025 * ALPHA), 1e-12);
    const auto& h = instrument->Definition()->Spec().header_;
    ASSERT_EQ(h.receiver_, "PAY");
    ASSERT_EQ(h.discountRoute_, "USD.OIS");
    ASSERT_EQ(h.payCurrency_, Ccy_("USD"));
    ASSERT_EQ(h.payCurrency_, h.resultCurrency_);
    ASSERT_EQ(h.payCurrency_, h.collateralCurrency_);
}

TEST(DepositScriptTest, TestImportRejectsProjectionSpotStartAndOtherCollateral) {
    RateIndexConvention_ convention;
    convention.forecastTenor_ = PeriodLength_("12M");
    convention.dayBasis_ = DayBasis_("ACT_360");
    const DepositScriptImportSettings_ settings{"deposit", Ccy_("USD"), "USD.OIS"};
    {
        convention.useProjectionCurve_ = true;
        const Deposit_ native(VALUE_DATE, VALUE_DATE, END_DATE, .025, convention);
        ASSERT_THROW(NewDepositScriptInstrument(native, settings), CalibrationError_);
    }
    {
        convention.useProjectionCurve_ = false;
        const Deposit_ native(VALUE_DATE, Date_(2026, 10, 23), END_DATE, .025, convention);
        ASSERT_THROW(NewDepositScriptInstrument(native, settings), CalibrationError_);
    }
    {
        convention.collateral_ = CollateralType_("GC");
        const Deposit_ native(VALUE_DATE, VALUE_DATE, END_DATE, .025, convention);
        ASSERT_THROW(NewDepositScriptInstrument(native, settings), CalibrationError_);
    }
}

TEST(DepositScriptTest, TestCandidateUsesSourceAlphaAndIndependentCashflowDerivatives) {
    const auto instrument = ImportedDeposit();
    const PreparedDepositCurve_ plan(instrument, CurveContext());
    for (double theta : {-.04, -.06, -std::log(1. + .035 * ALPHA), -.0351, -.0349}) {
        const auto value = plan.Evaluate(Candidate(theta));
        const double discount = std::exp(theta);
        ASSERT_NEAR(value.price_.value_, -100. + 100. * (1. + .025 * ALPHA) * discount, 1e-10);
        ASSERT_NEAR(value.parRate_.value_, (std::exp(-theta) - 1.) / ALPHA, 1e-10);
        ASSERT_NEAR(value.coefficients_.a_, 100. * ALPHA * discount, 1e-10);
        ASSERT_NEAR(value.coefficients_.b_, 100. * (discount - 1.), 1e-10);
        ASSERT_EQ(value.parameterLabels_, Vector_<String_>{"logdf:OIS:2027-10-21"});
        ASSERT_NEAR(value.price_.derivatives_[0], 100. * (1. + .025 * ALPHA) * discount, 1e-9);
        ASSERT_NEAR(value.parRate_.derivatives_[0], -std::exp(-theta) / ALPHA, 1e-9);
        for (double h : {1e-4, 5e-5}) {
            const auto up = plan.Evaluate(Candidate(theta + h)), down = plan.Evaluate(Candidate(theta - h));
            ASSERT_NEAR((up.price_.value_ - down.price_.value_) / (2. * h), value.price_.derivatives_[0], 1e-6);
            ASSERT_NEAR((up.parRate_.value_ - down.parRate_.value_) / (2. * h), value.parRate_.derivatives_[0], 1e-6);
            ASSERT_NE(up.candidateIdentity_, down.candidateIdentity_);
        }
    }
}

TEST(DepositScriptTest, TestQuoteFitsOneNativeNodeWithoutChangingCoupon) {
    const auto instrument = ImportedDeposit();
    const PreparedDepositCurve_ plan(instrument, CurveContext());
    const auto quote = NewCalibrationMarketQuote("quote", .035, CalibrationValueUnit_::Parse("Rate"));
    const auto observation = NewCalibrationObservation("fit", instrument, "ParRate", quote, 1e-10);
    const auto fit = FitDepositCurveNode(plan, observation, Candidate(), 1, -.1, .05, 64);
    ASSERT_NEAR(fit.curve_->discountLogDF_[1], -std::log(1. + .035 * ALPHA), 1e-10);
    ASSERT_LT(std::abs(fit.scaledResidual_), 1.);
    ASSERT_NEAR(fit.value_.parRate_.value_, .035, 1e-10);
    ASSERT_NEAR(fit.value_.price_.value_, fit.value_.coefficients_.a_ * (.025 - .035), 1e-8);
    ASSERT_GT(std::abs(fit.value_.price_.value_), .5);
    ASSERT_EQ(fit.axes_.quoteIndex_, Vector_<int>{0});
    ASSERT_DOUBLE_EQ(fit.axes_.quoteMap_(0, 0), 1.);
    ASSERT_LE(fit.evaluations_, 64);
    ASSERT_EQ(instrument->Binding()[1].value_, .025);
    const auto copy =
        BindCalibrationInstrumentParameters(instrument, {{{"K", quote->Value(), quote->Unit()}}, {{"K", quote->Id(), quote->Fingerprint()}}});
    ASSERT_NE(copy->BindingIdentity(), instrument->BindingIdentity());
    ASSERT_EQ(copy->Definition()->DefinitionIdentity(), instrument->Definition()->DefinitionIdentity());
    ASSERT_NEAR(PreparedDepositCurve_(copy, CurveContext()).Evaluate(fit.curve_).price_.value_, 0., 1e-8);
    ASSERT_NEAR(plan.Evaluate(fit.curve_).price_.value_, fit.value_.price_.value_, 1e-12);
    RecordProperty("fitted_theta", fit.curve_->discountLogDF_[1]);
    RecordProperty("fixed_coupon_pv", fit.value_.price_.value_);
    RecordProperty("solver_evaluations", fit.evaluations_);
}

TEST(DepositScriptTest, TestExplicitQuoteCopyRepricesNewMcBindingAndLeavesOldPlanUsable) {
    const auto instrument = ImportedDeposit();
    const double theta = -std::log(1. + .035 * ALPHA);
    const auto quote = NewCalibrationMarketQuote("q", .035, CalibrationValueUnit_::Parse("Rate"));
    const auto changed =
        BindCalibrationInstrumentParameters(instrument, {{{"K", quote->Value(), quote->Unit()}}, {{"K", quote->Id(), quote->Fingerprint()}}});
    const auto context = ModelContext(theta, 0.);
    auto settings = SmallSettings();
    settings.parameterLabels_ = {"logdf:OIS:2027-10-21"};
    const auto old = PrepareCalibrationMc(instrument, "Price", context, settings);
    const auto before = old.Evaluate(instrument, context, true);
    const auto after = PrepareCalibrationMc(changed, "Price", context, settings).Evaluate(changed, context, true);
    ASSERT_NEAR(before.mean_, Oracle(theta, false).value_, 1e-10);
    ASSERT_NEAR(after.mean_, 0., 1e-10);
    ASSERT_THROW(static_cast<void>(old.Evaluate(changed, context, true)), CalibrationError_);
    ASSERT_EQ(before.pathValues_, old.Evaluate(instrument, context, true).pathValues_);
}

TEST(DepositScriptTest, TestInvalidImportTermsOwnInputDiagnostics) {
    const Deposit_ native(VALUE_DATE, END_DATE, .025, DayBasis_("ACT_360"));
    for (double value : {0., -1., std::numeric_limits<double>::infinity()}) {
        std::optional<CalibrationError_> saved;
        try {
            static_cast<void>(NewDepositScriptInstrument(native, {"bad", Ccy_("USD"), "USD.OIS", value}));
        } catch (const CalibrationError_& error) {
            saved = error;
        }
        ASSERT_TRUE(saved);
        ASSERT_EQ(saved->Failure().diagnostic_.stage_, CalibrationStage_::Value_::Input);
        ASSERT_EQ(saved->Failure().diagnostic_.field_, std::optional<String_>("/import/notional"));
        ASSERT_TRUE(saved->Failure().report_);
    }
}

TEST(DepositScriptTest, TestUnsetCurrencyAndInvalidNativeTermsHaveOwnedErrors) {
    const Deposit_ native(VALUE_DATE, END_DATE, .025, DayBasis_("ACT_360"));
    ASSERT_THROW(NewDepositScriptInstrument(native, {"bad", Ccy_(), "USD.OIS"}), CalibrationError_);
    const Deposit_ reversed(VALUE_DATE, VALUE_DATE, .025, DayBasis_("ACT_360"));
    ASSERT_THROW(NewDepositScriptInstrument(reversed, {"bad", Ccy_("USD"), "USD.OIS"}), CalibrationError_);
    const Deposit_ nonfinite(VALUE_DATE, END_DATE, std::numeric_limits<double>::infinity(), DayBasis_("ACT_360"));
    ASSERT_THROW(NewDepositScriptInstrument(nonfinite, {"bad", Ccy_("USD"), "USD.OIS"}), CalibrationError_);
}

TEST(DepositScriptTest, TestSolverFailureOwnsLastCandidateAndObservationAxes) {
    const auto instrument = ImportedDeposit();
    const PreparedDepositCurve_ plan(instrument, CurveContext());
    const auto quote = NewCalibrationMarketQuote("q", .035, CalibrationValueUnit_::Parse("Rate"));
    const auto observation = NewCalibrationObservation("fit", instrument, "ParRate", quote, 1e-10);
    for (bool budget : {true, false}) {
        std::optional<CalibrationError_> saved;
        try {
            static_cast<void>(FitDepositCurveNode(plan, observation, Candidate(), 1, budget ? -.1 : .01, .05, budget ? 1 : 64));
        } catch (const CalibrationError_& error) {
            saved = error;
        }
        ASSERT_TRUE(saved);
        const auto& f = saved->Failure();
        ASSERT_EQ(f.diagnostic_.code_, budget ? "CalibrationBudgetExceeded" : "CalibrationFitFailed");
        ASSERT_EQ(f.diagnostic_.stage_, CalibrationStage_::Value_::Solve);
        ASSERT_EQ(f.diagnostic_.observationId_, std::optional<String_>("fit"));
        ASSERT_TRUE(f.candidate_);
        ASSERT_EQ(f.report_->Axes().quoteIndex_, Vector_<int>{0});
        ASSERT_EQ(f.report_->Errors().size(), 1);
    }
    ASSERT_THROW(FitDepositCurveNode(plan, observation, Candidate(), 0, -.1, .05, 64), CalibrationError_);
    ASSERT_THROW(FitDepositCurveNode(plan, observation, Candidate(), 1, .1, -.1, 64), CalibrationError_);
}

TEST(DepositScriptTest, TestUpperBracketFailureRetainsValidatedLowerCandidate) {
    const auto saved = NumericalFitFailure(1000., 1e-10);
    ASSERT_TRUE(saved);
    ASSERT_NO_FATAL_FAILURE(AssertRetainedFitFailure(*saved, -.1, 1));
    ASSERT_EQ(saved->Failure().diagnostic_.field_, std::optional<String_>("/discounts"));
    ASSERT_NE(std::string(saved->what()).find("finite positive discounts"), std::string::npos);
}

TEST(DepositScriptTest, TestSolverNumericalFailureRetainsPreviousCandidate) {
    const auto saved = NumericalFitFailure(.05, 1e-200);
    ASSERT_TRUE(saved);
    const double low = (std::exp(.1) - 1.) / ALPHA - .035;
    const double high = (std::exp(-.05) - 1.) / ALPHA - .035;
    const double first = (-.1 * high - .05 * low) / (high - low);
    const double firstResidual = (std::exp(-first) - 1.) / ALPHA - .035;
    const double second = (-.1 * firstResidual - first * low) / (firstResidual - low);
    ASSERT_NO_FATAL_FAILURE(AssertRetainedFitFailure(*saved, second, 4));
    ASSERT_EQ(saved->Failure().diagnostic_.field_, std::optional<String_>("/solver"));
    ASSERT_NE(std::string(saved->what()).find("non-finite discount logDF"), std::string::npos);
}

TEST(DepositScriptTest, TestNonfiniteResidualDoesNotPublishCandidate) {
    const auto saved = NumericalFitFailure(.05, 1e-320);
    ASSERT_TRUE(saved);
    ASSERT_NO_FATAL_FAILURE(AssertRetainedFitFailure(*saved, {}, 0));
    ASSERT_EQ(saved->Failure().diagnostic_.field_, std::optional<String_>("/solver/residual"));
    ASSERT_NE(std::string(saved->what()).find("finite scaled residual"), std::string::npos);
}

TEST(DepositScriptTest, TestResidualOverflowRetainsValidatedLowerCandidate) {
    const auto saved = NumericalFitFailure(.05, 1e-310, -.04);
    ASSERT_TRUE(saved);
    ASSERT_NO_FATAL_FAILURE(AssertRetainedFitFailure(*saved, -.04, 1));
    ASSERT_EQ(saved->Failure().diagnostic_.field_, std::optional<String_>("/solver/residual"));
    ASSERT_NE(std::string(saved->what()).find("finite scaled residual"), std::string::npos);
}

TEST(DepositScriptTest, TestIndependentNativeAadAndMultipleInterpolationWeights) {
    const auto instrument = ImportedDeposit();
    auto context = CurveContext();
    const Date_ middle(2027, 4, 21), last(2028, 10, 21);
    context.curve_.reset(new GSRCurveData_("curve", VALUE_DATE, "USD", {VALUE_DATE, middle, last}, {0., -.02, -.08}, {}, {}));
    const PreparedDepositCurve_ plan(instrument, context);
    const auto value = plan.Evaluate(context.curve_);
    const double weight = static_cast<double>(END_DATE - middle) / (last - middle);
    const double logDf = (1. - weight) * -.02 + weight * -.08;
    ASSERT_NEAR(value.price_.value_, -100. + 100. * (1. + .025 * ALPHA) * std::exp(logDf), 1e-10);
    AAD::RecordingScope_ recording;
    AAD::Number_ x, y;
    recording.RegisterInput(x, -.02);
    recording.RegisterInput(y, -.08);
    recording.StartRecording();
    const AAD::Number_ df = AAD::exp((1. - weight) * x + weight * y);
    AAD::Number_ rate = Tape::ForwardRateFromDf(df, ALPHA);
    recording.FinishRecording();
    recording.ClearAdjoints();
    AAD::Adjoint(rate) = 1.;
    recording.Reverse();
    ASSERT_NEAR(AAD::Value(rate), value.parRate_.value_, 1e-10);
    ASSERT_NEAR(AAD::AdjointValue(x), value.parRate_.derivatives_[0], 1e-9);
    ASSERT_NEAR(AAD::AdjointValue(y), value.parRate_.derivatives_[1], 1e-9);
}

TEST(DepositScriptTest, TestCandidateRejectsChangedLayoutRoutesAndUncoveredDates) {
    const auto instrument = ImportedDeposit();
    const PreparedDepositCurve_ plan(instrument, CurveContext());
    ASSERT_THROW(static_cast<void>(plan.Evaluate({})), CalibrationError_);
    const Handle_<GSRCurveData_> foreign(new GSRCurveData_("foreign", VALUE_DATE, "EUR", {VALUE_DATE, END_DATE}, {0., -.04}, {}, {}));
    ASSERT_THROW(static_cast<void>(plan.Evaluate(foreign)), CalibrationError_);
    auto changed = CurveContext();
    changed.curve_.reset(new GSRCurveData_("changed", VALUE_DATE, "USD", {VALUE_DATE, Date_(2027, 4, 21)}, {0., -.02}, {}, {}));
    ASSERT_THROW(static_cast<void>(plan.Evaluate(changed.curve_)), CalibrationError_);
    ASSERT_THROW(PreparedDepositCurve_(instrument, changed), CalibrationError_);
    changed = CurveContext();
    changed.curve_.reset(new GSRCurveData_("projection", VALUE_DATE, "USD", {VALUE_DATE, END_DATE}, {0., -.04}, {"3M"}, Matrix_<>(1, 2, 0.)));
    ASSERT_THROW(static_cast<void>(plan.Evaluate(changed.curve_)), CalibrationError_);
    changed = CurveContext();
    changed.discountRoute_ = "EUR.OIS";
    ASSERT_THROW(PreparedDepositCurve_(instrument, changed), CalibrationError_);
    changed = CurveContext();
    changed.smoothing_ = .001;
    ASSERT_THROW(PreparedDepositCurve_(instrument, changed), CalibrationError_);
    ASSERT_THROW(PreparedDepositCurve_(instrument, ModelContext()), CalibrationError_);
}

TEST(DepositScriptTest, TestFastKernelCannotReturnNonfiniteRateForFiniteAffineValue) {
    auto spec = ImportedDeposit()->Definition()->Spec();
    spec.source_.rows_[0].text_ = "1e300";
    spec.source_.rows_[1].text_ = "0";
    spec.source_.rows_[2].text_ = "100000000";
    const auto instrument = NewScriptCalibrationInstrument({"extreme", NewScriptInstrumentDefinition(spec)});
    const auto context = CurveContext(-710.);
    const auto shared = PrepareCalibrationInstrument(instrument, "ParRate", context);
    ASSERT_TRUE(std::isfinite(shared.AffineMeasure(shared.Coefficients({1., std::exp(-710.)})).value_));
    const PreparedDepositCurve_ plan(instrument, context);
    ASSERT_THROW(static_cast<void>(plan.Evaluate(context.curve_)), CalibrationError_);
}

TEST(DepositScriptTest, TestCancellationOverflowCannotReturnNonfinitePricePartial) {
    std::optional<CalibrationError_> saved;
    {
        const Deposit_ native(VALUE_DATE, END_DATE, -2., DayBasis_("ACT_360"));
        const auto instrument = NewDepositScriptInstrument(native, {"extreme", Ccy_("USD"), "USD.OIS", 5.8e307});
        const PreparedDepositCurve_ plan(instrument, CurveContext());
        const double cashflowPartial = 5.8e307 * (1. - 2. * ALPHA) * 2.;
        ASSERT_TRUE(std::isfinite(cashflowPartial));
        ASSERT_TRUE(std::isfinite(-5.8e307 + cashflowPartial));
        try {
            static_cast<void>(plan.Evaluate(Candidate(std::log(2.))));
        } catch (const CalibrationError_& error) {
            saved = error;
        }
    }
    ASSERT_TRUE(saved);
    const auto& failure = saved->Failure();
    ASSERT_EQ(failure.diagnostic_.code_, "CalibrationNumericalFailure");
    ASSERT_EQ(failure.diagnostic_.stage_, CalibrationStage_::Value_::Evaluate);
    ASSERT_EQ(failure.diagnostic_.instrumentId_, std::optional<String_>("extreme"));
    ASSERT_EQ(failure.diagnostic_.field_, std::optional<String_>("/derivatives"));
    ASSERT_TRUE(failure.report_);
    ASSERT_EQ(failure.report_->Stage(), CalibrationStage_::Value_::Evaluate);
    ASSERT_EQ(failure.report_->Errors().size(), 1);
    ASSERT_EQ(failure.report_->Errors()[0].code_, failure.diagnostic_.code_);
    ASSERT_FALSE(failure.candidate_);
}

TEST(DepositScriptTest, TestAdjustedEndIsExportedOnlyOnce) {
    const Date_ sunday(2027, 10, 24), monday(2027, 10, 25);
    const Deposit_ native(VALUE_DATE, sunday, .025, DayBasis_("ACT_360"));
    const auto instrument = NewDepositScriptInstrument(native, {"adjusted", Ccy_("USD"), "USD.OIS"});
    auto context = CurveContext();
    context.curve_.reset(new GSRCurveData_("curve", VALUE_DATE, "USD", {VALUE_DATE, monday}, {0., -.04}, {}, {}));
    const PreparedDepositCurve_ plan(instrument, context);
    const auto& d = std::get<CalibrationDepositDescriptor_>(plan.PricePlan().Descriptor());
    ASSERT_EQ(d.maturity_, monday);
    ASSERT_DOUBLE_EQ(d.accrual_, static_cast<double>(monday - VALUE_DATE) / 360.);
    ASSERT_NEAR(plan.Evaluate(context.curve_).parRate_.value_, (std::exp(.04) - 1.) / d.accrual_, 1e-10);
    const Deposit_ shifted(Date_(2026, 10, 25), Date_(2027, 10, 25), .025, DayBasis_("ACT_360"));
    ASSERT_THROW(NewDepositScriptInstrument(shifted, {"bad", Ccy_("USD"), "USD.OIS"}), CalibrationError_);
}

TEST(DepositScriptTest, TestSourceChangesReachFastAndBothScriptBackendsWithoutNativeEdits) {
    const auto original = ImportedDeposit();
    const PreparedDepositCurve_ oldPlan(original, CurveContext());
    auto spec = original->Definition()->Spec();
    spec.source_.rows_[0].text_ = "250";
    spec.source_.rows_[1].text_ = "-0.01";
    spec.source_.rows_[2].text_ = "0.75";
    const auto changed = NewScriptCalibrationInstrument({"changed", NewScriptInstrumentDefinition(spec)});
    const auto value = PreparedDepositCurve_(changed, CurveContext()).Evaluate(Candidate());
    ASSERT_NEAR(value.price_.value_, -250. + 250. * (1. - .01 * .75) * std::exp(-.04), 1e-10);
    ASSERT_NEAR(value.parRate_.value_, (std::exp(.04) - 1.) / .75, 1e-10);
    ASSERT_THROW(oldPlan.PricePlan().RequireCompatible(changed, CurveContext()), CalibrationError_);
    ASSERT_NEAR(oldPlan.Evaluate(Candidate()).price_.value_, Oracle(-.04, false).value_, 1e-10);
    for (bool compiled : {false, true}) {
        auto settings = SmallSettings(compiled);
        settings.parameterLabels_ = {"logdf:OIS:2027-10-21"};
        const auto context = ModelContext(-.04, 0.);
        for (bool rate : {false, true}) {
            const auto mc = PrepareCalibrationMc(changed, rate ? "ParRate" : "Price", context, settings).Evaluate(changed, context, true);
            const auto& fast = rate ? value.parRate_ : value.price_;
            ASSERT_NEAR(mc.mean_, fast.value_, 1e-10);
            ASSERT_NEAR(mc.gradient_[0], fast.derivatives_[0], 1e-9);
        }
    }
    for (const String_& text : {String_("pay PAYS N*(1+K*ALPHA)+1"), String_("pay PAYS -N*(1+K*ALPHA)")}) {
        spec.source_.rows_.back().text_ = text;
        const auto mismatch = NewScriptCalibrationInstrument({"mismatch", NewScriptInstrumentDefinition(spec)});
        ASSERT_THROW(PreparedDepositCurve_(mismatch, CurveContext()), CalibrationError_);
        ASSERT_THROW(PrepareCalibrationMc(mismatch, "Price", ModelContext(), SmallSettings()), CalibrationError_);
    }
}

TEST(DepositScriptTest, TestSameBindingTreeCompiledAadAndCrnDifferences) {
    const auto instrument = ImportedDeposit();
    const auto context = ModelContext();
    for (bool rate : {false, true}) {
        const String_ measure = rate ? "ParRate" : "Price";
        const auto tree = PrepareCalibrationMc(instrument, measure, context, SmallSettings(false)).Evaluate(instrument, context, false);
        const auto compiled = PrepareCalibrationMc(instrument, measure, context, SmallSettings()).Evaluate(instrument, context, false);
        ASSERT_NEAR(tree.mean_, compiled.mean_, 1e-12);
        ASSERT_EQ(tree.pathValues_, compiled.pathValues_);
        for (int j = 0; j < 2; ++j) {
            ASSERT_NEAR(tree.gradient_[j], compiled.gradient_[j], 1e-10);
            AssertCrnDifferences(instrument, measure, j, compiled.gradient_[j]);
            ASSERT_DOUBLE_EQ(compiled.quoteJacobian_(0, j), compiled.gradient_[j]);
        }
    }
    ASSERT_FALSE(AAD::LastRecordingCleanupFailure());
}

TEST(DepositScriptNumericalTest, TestFrozenThreeWayPriceRateAndDerivativeMatrix) {
    const auto instrument = ImportedDeposit();
    const PreparedDepositCurve_ fastPlan(instrument, CurveContext());
    const double fitted = -std::log(1. + .035 * ALPHA);
    const Vector_<> points{-.04, -.06, fitted, fitted + 1e-4, fitted - 1e-4, fitted + 5e-5, fitted - 5e-5};
    RecordProperty("definition", std::string(instrument->Definition()->DefinitionIdentity().c_str()));
    RecordProperty("binding", std::string(instrument->BindingIdentity().c_str()));
    RecordProperty("columns", "logdf:OIS:2027-10-21,g:2026-10-21");
    int ordinal = 0;
    for (double theta : points) {
        SCOPED_TRACE(theta);
        const auto context = ModelContext(theta);
        const auto fast = fastPlan.Evaluate(context.curve_);
        for (bool rate : {false, true}) {
            const String_ measure = rate ? "ParRate" : "Price";
            SCOPED_TRACE(measure);
            const auto oracle = Oracle(theta, rate);
            const auto& value = rate ? fast.parRate_ : fast.price_;
            ASSERT_NEAR(value.value_, oracle.value_, 1e-10);
            ASSERT_NEAR(value.derivatives_[0], oracle.gradient_[0], 1e-9);
            const auto plan = PrepareCalibrationMc(instrument, measure, context, NumericalSettings());
            const auto fit = plan.Evaluate(instrument, context, false), validation = plan.Evaluate(instrument, context, true);
            const auto precision = Precision(rate);
            const auto fromOracle = ValidateCalibrationMc(validation, oracle, precision);
            auto fastReference = oracle;
            fastReference.value_ = value.value_;
            fastReference.gradient_[0] = value.derivatives_[0];
            ASSERT_TRUE(fromOracle.numericalValidationPassed_) << validation.mean_ << " SE=" << validation.standardError_;
            ASSERT_TRUE(ValidateCalibrationMc(validation, fastReference, precision).numericalValidationPassed_);
            ASSERT_TRUE(ValidateCalibrationMc(fit, validation, precision).numericalValidationPassed_);
            ASSERT_TRUE(validation.affine_.has_value() == rate);
            if (rate)
                AssertRatioCovariance(validation);
            auto halfSettings = NumericalSettings();
            halfSettings.validationBatches_ = 8;
            const auto half = PrepareCalibrationMc(instrument, measure, context, halfSettings).Evaluate(instrument, context, true);
            ASSERT_TRUE(ValidateCalibrationMc(half, oracle, precision).numericalValidationPassed_);
            ASSERT_LT(std::abs(half.mean_ - validation.mean_), .25 * precision.errorScale_);
            const std::string prefix = std::to_string(ordinal) + (rate ? "_rate" : "_price");
            RecordProperty(prefix + "_theta", theta);
            RecordProperty(prefix + "_oracle", oracle.value_);
            RecordProperty(prefix + "_mean", validation.mean_);
            RecordProperty(prefix + "_se", validation.standardError_);
            RecordProperty(prefix + "_dtheta", validation.gradient_[0]);
            RecordProperty(prefix + "_dtheta_se", validation.gradientStandardError_[0]);
            RecordProperty(prefix + "_g", validation.gradient_[1]);
            RecordProperty(prefix + "_g_se", validation.gradientStandardError_[1]);
            RecordProperty(prefix + "_refinement", std::abs(half.mean_ - validation.mean_));
            RecordProperty(prefix + "_runs", static_cast<int>(validation.engineRuns_));
            RecordProperty(prefix + "_tape_nodes", static_cast<int>(validation.peakTapeNodes_));
            RecordProperty(prefix + "_tape_bytes", static_cast<int>(validation.peakTapeBytes_));
            RecordProperty(prefix + "_context", std::string(validation.contextIdentity_.c_str()));
            RecordProperty(prefix + "_mapping", std::string(validation.mappingIdentity_.c_str()));
            RecordProperty(prefix + "_plan", std::string(validation.planIdentity_.c_str()));
            RecordProperty(prefix + "_candidate", std::string(fast.candidateIdentity_.c_str()));
            RecordProperty(prefix + "_error_indicator", fromOracle.errorIndicator_);
        }
        ++ordinal;
    }
}

TEST(DepositScriptTest, TestSmallBudgetAndRatioDenominatorCannotPassNumerics) {
    const auto instrument = ImportedDeposit();
    const auto context = ModelContext();
    const auto result = PrepareCalibrationMc(instrument, "Price", context, SmallSettings()).Evaluate(instrument, context, true);
    auto precision = Precision(false);
    precision.errorScale_ = 1e-9;
    precision.epsilonValue_ = 1e-12;
    precision.epsilonGradient_ = {1e-12, 1e-12};
    const auto validation = ValidateCalibrationMc(result, Oracle(-.04, false), precision);
    ASSERT_FALSE(validation.numericalValidationPassed_);
    ASSERT_TRUE(validation.budgetExhausted_);
    ASSERT_TRUE(validation.report_);
    auto settings = SmallSettings();
    settings.annuitySigma_ = 1e9;
    const auto plan = PrepareCalibrationMc(instrument, "ParRate", context, settings);
    ASSERT_THROW(static_cast<void>(plan.Evaluate(instrument, context, true)), CalibrationError_);
    settings = SmallSettings();
    settings.maxPathsPerView_ = 47;
    ASSERT_THROW(PrepareCalibrationMc(instrument, "Price", context, settings), CalibrationError_);
}

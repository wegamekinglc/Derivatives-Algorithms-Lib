#include <gtest/gtest.h>

#include <dal/curve/xccygaussianstaged.hpp>
#include <dal/curve/ycconst.hpp>
#include <dal/math/aad/recording.hpp>
#include <dal/math/random/pseudorandom.hpp>
#include <dal/script/xccymc_internal.hpp>

#include "xccymcfixture.hpp"
#include "xccymcreference.hpp"

using namespace Dal;
using namespace Dal::TestXccyMoments;
using namespace Dal::TestXccyExecution;
using namespace Dal::TestXccyMc;

TEST(XccyMcBoundaryTest, TestFrozenHistoryAndAtValuationResetRemainDistinctFromPaymentSpot) {
    const auto spec = ModelSpec(Date_(2027, 8, 4));
    const auto model = Data(spec);
    const auto instrument = Instrument(XccyNotionalMode_::Value_::MARK_TO_MARKET);
    const MarketFixingSnapshot_::values_t history{
        {"IR:USD,LIBOR_3M_LCH", {{DateTime_(Date_(2027, 4, 30)), .05}, {DateTime_(Date_(2027, 8, 2)), .06}}},
        {"IR:EUR,LIBOR_6M_LCH", {{DateTime_(Date_(2027, 4, 30)), .03}}}};
    auto settings = Settings(2, 3);
    settings.retainPaths_ = true;
    ASSERT_THROW(static_cast<void>(PreparedXccyMc_(instrument, "Price", Context(spec, Empty()), model, settings)), CalibrationError_);
    const auto usdValues = history.at("IR:USD,LIBOR_3M_LCH"), eurValues = history.at("IR:EUR,LIBOR_6M_LCH");
    const ScopedHistory_ usd("IR:USD,LIBOR_3M_LCH", {Vector_<std::pair<DateTime_, double>>(usdValues.begin(), usdValues.end())});
    const ScopedHistory_ eur("IR:EUR,LIBOR_6M_LCH", {Vector_<std::pair<DateTime_, double>>(eurValues.begin(), eurValues.end())});
    const ScopedHistory_ fx("FX[EUR/USD]", {{{spec.market_.valuationTime_, 1.9}}});
    const PreparedXccyMc_ frozen(instrument, "Price", Context(spec), model, settings);
    const auto before = frozen.Evaluate();
    fx.Set({{{spec.market_.valuationTime_, 2.9}}});
    usd.Set({{{DateTime_(Date_(2027, 4, 30)), .9}, {DateTime_(Date_(2027, 8, 2)), .8}}});
    ASSERT_EQ(before.estimate_.pathValues_, frozen.Evaluate().estimate_.pathValues_);
    auto supplied = history;
    supplied["FX[EUR/USD]"][spec.market_.valuationTime_] = 1.6;
    const auto context = Context(spec, Handle_<MarketFixingSnapshot_>(new MarketFixingSnapshot_(supplied)));
    const auto atV = PreparedXccyMc_(instrument, "Price", context, model, settings).Evaluate();
    ASSERT_GT(std::abs(before.estimate_.mean_ - atV.estimate_.mean_), 1.);
    supplied["FX[EUR/USD]"][DateTime_(Date_(2027, 11, 4))] = 99.;
    const auto future =
        PreparedXccyMc_(instrument, "Price", Context(spec, Handle_<MarketFixingSnapshot_>(new MarketFixingSnapshot_(supplied))), model, settings)
            .Evaluate();
    ASSERT_EQ(atV.estimate_.pathValues_, future.estimate_.pathValues_);
    const Script::PreparedXccyScript_ source(instrument, context, model);
    const Script::XccyScriptPathProvider_<> provider(source);
    auto rng = New(RNGType_("MRG32"), settings.seed_, atV.mapping_.dimension_, true);
    Vector_<> normals(atV.mapping_.dimension_);
    rng->SkipNormalTo(atV.reservation_.ranges_[0].firstPath_);
    rng->FillNormal(&normals);
    const auto states = provider.Model().GenerateStates(normals);
    const CashflowReference_ reference(spec, states, context.xccy_->fixings_);
    ASSERT_NEAR(atV.estimate_.pathValues_[0], Sum(reference.Payments(XccyNotionalMode_::Value_::MARK_TO_MARKET, false, true)), 1e-10);
}

TEST(XccyMcBoundaryTest, TestInsufficientBudgetRetainsEstimateAndIndependentFailureFlags) {
    const auto spec = ModelSpec();
    const auto model = Data(spec);
    const auto instrument = Instrument(XccyNotionalMode_::Value_::MARK_TO_MARKET);
    const auto context = Context(spec, Empty());
    const auto result = PreparedXccyMc_(instrument, "Price", context, model, Settings(2, 2)).Evaluate();
    CalibrationMcReference_ reference;
    reference.value_ = EvaluateXccyGaussianMoments<double>(PrepareMoment(instrument, context), *model).price_;
    reference.numericalError_ = 1e-12;
    CalibrationMcPrecision_ precision;
    precision.errorScale_ = .01;
    const auto check = ValidateXccyMc(result, reference, precision);
    ASSERT_TRUE(std::isfinite(result.estimate_.mean_));
    ASSERT_GT(result.estimate_.standardError_, 0.);
    ASSERT_FALSE(check.validation_.precisionPassed_);
    ASSERT_FALSE(check.validation_.numericalValidationPassed_);
    ASSERT_TRUE(check.validation_.budgetExhausted_);
    ASSERT_TRUE(check.validation_.report_);
    ASSERT_EQ(check.validation_.report_->Data().observations_[0].modelValue_, result.estimate_.mean_);
    ASSERT_EQ(result.report_->Schema(), String_("dal.calibration-report/1"));
    ASSERT_EQ(result.version_, "dal.xccy-gaussian-mc-result/1");
    ASSERT_FALSE(result.report_->Data().summary_.numericalValidationPassed_.has_value());
}

TEST(XccyMcBoundaryTest, TestPwcBasisNativeColumnsDoNotAcquireAnExtraAnchorRisk) {
    auto spec = ModelSpec();
    const auto definition =
        MakeCurveDefinition("basis", "USD", CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD, LogDfScheme_::Value_::LOG_LINEAR,
                            {TestXccy::TODAY.AddDays(365), TestXccy::TODAY.AddDays(1095)}, TestXccy::TODAY, DayBasis::Act365F());
    spec.market_.basis_ = Handle_<DiscountCurve_>(BuildDiscountCurveT<double>(definition, {.003, .004}));
    auto staged = TestXccy::Spec();
    staged.domesticCurveBlock_ = spec.market_.domesticBlock_;
    staged.foreignCurveBlock_ = spec.market_.foreignBlock_;
    staged.gaussianDynamics_.reset(new XccyGaussianDynamics_("dynamics", spec.dynamics_));
    const auto model = XccyGaussianBasisCandidate(staged, spec.market_.basis_);
    const auto instrument = Instrument(XccyNotionalMode_::Value_::MARK_TO_MARKET);
    const auto context = Context(spec, Empty());
    auto settings = Settings(2, 3);
    settings.parameterLabels_ = model->market_->basis_->nativeLabels_;
    ASSERT_EQ(settings.parameterLabels_.size(), 2U);
    ASSERT_EQ(model->market_->basis_->nodeDates_.size(), 3U);
    settings.aad_ = true;
    const auto aad = PreparedXccyMc_(instrument, "Price", context, model, settings).Evaluate();
    const auto labels = settings.parameterLabels_;
    ASSERT_NEAR(aad.estimate_.mean_, PWC_PRICE, 1e-10);
    for (size_t j = 0; j < labels.size(); ++j) {
        const auto found = std::find_if(PWC_NATIVE.begin(), PWC_NATIVE.end(), [&](const auto& row) { return labels[j] == row.label_; });
        ASSERT_NE(found, PWC_NATIVE.end());
        ASSERT_NEAR(aad.estimate_.gradient_[j], found->full_, 1e-8);
        ASSERT_NEAR(aad.estimate_.gradient_[j], found->half_, 1e-8);
    }
    ASSERT_NEAR(aad.estimate_.gradient_[1], 0., 1e-12);
}

TEST(XccyMcBoundaryTest, TestIndependentPathControlsDetectOmittedQuantoAndReversedBasis) {
    const auto spec = ModelSpec();
    const auto model = Data(spec);
    const auto instrument = Instrument(XccyNotionalMode_::Value_::MARK_TO_MARKET);
    const auto context = Context(spec, Empty());
    auto settings = Settings(2, 2);
    settings.retainPaths_ = true;
    const auto actual = PreparedXccyMc_(instrument, "Price", context, model, settings).Evaluate();
    const Script::PreparedXccyScript_ source(instrument, context, model);
    const Script::XccyScriptPathProvider_<> provider(source);
    auto states = provider.Model().GenerateStates(actual.firstNormals_[0]);
    const CashflowReference_ reference(spec, states, Empty());
    const auto mode = XccyNotionalMode_::Value_::MARK_TO_MARKET;
    ASSERT_NEAR(actual.estimate_.pathValues_[0], Sum(reference.Payments(mode, false, true)), 1e-10);
    const auto& d = spec.dynamics_;
    const double q = -d.correlation_(1, 2) * d.foreignSigma_[0] * d.fxSigma_[0];
    for (auto& state : states) {
        const double t = (state.date_ - spec.market_.valuationTime_.Date()) / 365.;
        const double h = -std::expm1(-d.foreignA_ * t) / d.foreignA_;
        state.values_[1] -= q * h;
        state.values_[3] -= q * (t - h) / d.foreignA_;
    }
    const CashflowReference_ noQuanto(spec, states, Empty());
    ASSERT_GT(std::abs(actual.estimate_.pathValues_[0] - Sum(noQuanto.Payments(mode, false, true))), 1e-10);
    auto wrongBasis = spec;
    wrongBasis.market_.basis_ = Curve("USD", -.003);
    const auto wrong = PreparedXccyMc_(instrument, "Price", context, Data(wrongBasis), settings).Evaluate();
    ASSERT_GT(std::abs(actual.estimate_.pathValues_[0] - wrong.estimate_.pathValues_[0]), 1e-3);
    ASSERT_EQ(actual.mapping_.identity_, wrong.mapping_.identity_);
    ASSERT_NE(actual.modelIdentity_, wrong.modelIdentity_);
}

TEST(XccyMcBoundaryTest, TestCoefficientViewsRespectAdmittedCouponBindingBounds) {
    const auto spec = ModelSpec();
    const auto model = Data(spec);
    const auto original = Instrument(XccyNotionalMode_::Value_::MARK_TO_MARKET);
    auto definition = original->Definition()->Spec();
    definition.parameters_[1].lower_ = .0005;
    definition.parameters_[1].upper_ = .002;
    const auto bounded = NewScriptCalibrationInstrument({"bounded", NewScriptInstrumentDefinition(definition), {}, original->Pillar()});
    const auto context = Context(spec, Empty());
    const auto expected = PreparedXccyMc_(original, "Price", context, model, Settings(2, 3)).Evaluate();
    const auto actual = PreparedXccyMc_(bounded, "Price", context, model, Settings(2, 3)).Evaluate();
    ASSERT_NEAR(actual.estimate_.mean_, expected.estimate_.mean_, 1e-10);
    ASSERT_NEAR(actual.coefficients_.a_, expected.coefficients_.a_, 1e-10);
    ASSERT_NEAR(actual.coefficients_.b_, expected.coefficients_.b_, 1e-10);
}

TEST(XccyMcBoundaryTest, TestNonfinitePartialUncertaintyCannotEscapeWithFinitePrice) {
    XccyMcResult_ result;
    result.estimate_.parameterLabels_ = {"test-native-axis"};
    for (const double derivative : {-1e200, 1e200}) {
        CalibrationMcBatchValue_ batch;
        batch.coefficients_ = CalibrationAffineCoefficients_{-50., 1., {0.}, {derivative}};
        result.estimate_.batches_.push_back(batch);
    }
    const auto spec = ModelSpec();
    const auto admitted = PrepareMoment(Instrument(), Context(spec, Empty()));
    ASSERT_THROW(XccyMcInternal::Summarize(&result, admitted, .001), CalibrationError_);
}

TEST(XccyMcBoundaryTest, TestParAadNativeChainMatchesCrnBumps) {
    const auto spec = NonflatSpec();
    const auto model = Data(spec);
    const auto instrument = Instrument(XccyNotionalMode_::Value_::MARK_TO_MARKET, true);
    const auto context = Context(spec, Empty());
    auto settings = Settings(4, 8);
    settings.aad_ = true;
    settings.parameterLabels_ = {"fxspot:EUR/USD", model->market_->basis_->nativeLabels_.front(),
                                 "sigma:EUR/USD:" + Date::ToString(model->dynamics_->bucketEndDates_.front())};
    const auto aad = PreparedXccyMc_(instrument, "BasisSpread", context, model, settings).Evaluate();
    const auto labels = settings.parameterLabels_;
    settings.aad_ = false;
    settings.parameterLabels_.clear();
    const PreparedXccyMc_ plain(instrument, "BasisSpread", context, model, settings);
    for (size_t j = 0; j < labels.size(); ++j)
        for (const double h : {1e-3 * Scale(labels[j]), 5e-4 * Scale(labels[j])}) {
            const double up = plain.Evaluate(Bump(*model, labels[j], h)).estimate_.mean_;
            const double down = plain.Evaluate(Bump(*model, labels[j], -h)).estimate_.mean_;
            ASSERT_NEAR(aad.estimate_.gradient_[j], (up - down) / (2. * h), 1e-8);
        }
    ASSERT_EQ(aad.coefficientCovariance_.Rows(), 8);
    ASSERT_EQ(AAD::Tape()->nodes_.Size(), 0);
}

TEST(XccyMcBoundaryTest, TestOneLegSettledUsesNewPriceDomainAndKeepsLegacyRefusal) {
    auto spec = ModelSpec(Date_(2027, 11, 5));
    spec.market_.domesticBlock_.reset(new CurveBlock_("USD", "USD", spec.market_.domesticBlock_->DiscountCurves(), {}));
    spec.market_.foreignBlock_.reset(new CurveBlock_("EUR", "EUR", spec.market_.foreignBlock_->DiscountCurves(), {}));
    const auto model = Data(spec);
    const MarketFixingSnapshot_::values_t history{{"IR:EUR,LIBOR_6M_LCH", {{DateTime_(Date_(2027, 4, 30)), .031}}}};
    const auto context = Context(spec, Handle_<MarketFixingSnapshot_>(new MarketFixingSnapshot_(history)));
    for (bool foreign : {false, true}) {
        const auto instrument = Instrument(XccyNotionalMode_::Value_::MARK_TO_MARKET, foreign);
        const auto price = PreparedXccyMc_(instrument, "Price", context, model, Settings(2, 3)).Evaluate();
        ASSERT_TRUE(std::isfinite(price.estimate_.mean_));
        ASSERT_THROW(static_cast<void>(Native(PrepareMoment(instrument, context), spec)), CalibrationError_);
        if (foreign) {
            const auto par = PreparedXccyMc_(instrument, "BasisSpread", context, model, Settings(2, 3)).Evaluate();
            ASSERT_NEAR(par.estimate_.mean_, -.031, 1e-12);
        } else {
            ASSERT_EQ(price.coefficients_.a_, 0.);
            ASSERT_THROW(static_cast<void>(PreparedXccyMc_(instrument, "BasisSpread", context, model, Settings(2, 3)).Evaluate()), CalibrationError_);
        }
    }
}

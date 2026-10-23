#include <gtest/gtest.h>

#include <dal-public/src/global.hpp>
#include <dal-public/src/xccymontecarlo.hpp>

#include "../../dal-cpp/tests/curve/xccygaussianmomentfixture.hpp"

TEST(PublicXccyMonteCarloTest, TestSameCoreInputAndOwnedPhaseLedger) {
    using namespace Dal;
    InitGlobalData();
    const auto spec = TestXccyMoments::ModelSpec();
    const auto model = NewXccyGaussianModel("public", spec);
    const auto instrument = TestXccyMoments::Instrument(XccyNotionalMode_::Value_::MARK_TO_MARKET);
    XccyMonteCarloRequest_ request;
    request.measureId_ = "Price";
    request.phase_ = XccyMonteCarloPhase_::Value_::Validation;
    request.sampling_ = {"PseudoRandom", 8, 4, 1024, "dal.mrg32-normal-paths/1"};
    request.fixings_.reset(new MarketFixingSnapshot_({}));
    const auto value = ValueXccyInstrumentByMonteCarlo(instrument, model, request);
    const auto& extension = XccyMonteCarloDetails(value);
    XccyScriptContext_ xccy;
    xccy.valuationTime_ = spec.market_.valuationTime_;
    xccy.pair_ = *spec.market_.pair_;
    xccy.collateralCurrency_ = spec.market_.collateralCurrency_;
    xccy.discountRoute_ = spec.market_.discountRoute_;
    xccy.fixings_ = request.fixings_;
    CalibrationPreparationContext_ context;
    context.xccy_ = std::make_shared<const XccyScriptContext_>(xccy);
    XccyMcSettings_ settings;
    settings.phase_ = request.phase_;
    settings.batches_ = 4;
    settings.pathsPerBatch_ = 8;
    const auto core = PreparedXccyMc_(instrument, "Price", context, model, settings).Evaluate();
    ASSERT_DOUBLE_EQ(value.mean_, core.estimate_.mean_);
    ASSERT_DOUBLE_EQ(*value.standardError_, core.estimate_.standardError_);
    ASSERT_EQ(extension.reservation_.identity_, core.reservation_.identity_);
    ASSERT_EQ(extension.mapping_.identity_, core.mapping_.identity_);
    ASSERT_TRUE(extension.estimate_.validation_);
    ASSERT_FALSE(extension.estimate_.heldOut_);
    ASSERT_FALSE(value.report_->Data().summary_.numericalValidationPassed_.has_value());
    ASSERT_FALSE(value.report_->QuoteRiskAvailable());
}

TEST(PublicXccyMonteCarloTest, TestIndependentFlagsBudgetAndOwnedInputFailures) {
    using namespace Dal;
    InitGlobalData();
    const auto model = NewXccyGaussianModel("flags", TestXccyMoments::ModelSpec());
    const auto instrument = TestXccyMoments::Instrument();
    XccyMonteCarloRequest_ request;
    request.measureId_ = "Price";
    request.sampling_ = {"PseudoRandom", 8, 4, 1024, "dal.mrg32-normal-paths/1"};
    try {
        static_cast<void>(ValueXccyInstrumentByMonteCarlo(instrument, model, request));
        FAIL();
    } catch (const CalibrationError_& error) {
        ASSERT_EQ(error.Failure().diagnostic_.code_, "InvalidCalibrationPlan");
        ASSERT_EQ(error.Failure().diagnostic_.field_, "/phase");
        ASSERT_EQ(error.Failure().diagnostic_.instrumentId_, instrument->Id());
        ASSERT_TRUE(error.Failure().report_);
    }
    request.phase_ = XccyMonteCarloPhase_::Value_::Validation;
    const auto value = ValueXccyInstrumentByMonteCarlo(instrument, model, request);
    CalibrationMcReference_ reference;
    reference.value_ = value.mean_;
    CalibrationMcPrecision_ precision;
    precision.errorScale_ = 1e-12;
    precision.epsilonValue_ = 1e-16;
    const auto validation = ValidateXccyMonteCarlo(value, reference, precision);
    ASSERT_TRUE(validation.validation_.parityPassed_);
    ASSERT_FALSE(validation.validation_.precisionPassed_);
    ASSERT_FALSE(validation.validation_.valueErrorPassed_);
    ASSERT_FALSE(validation.validation_.numericalValidationPassed_);
    ASSERT_TRUE(validation.validation_.budgetExhausted_);
    request.phase_ = XccyMonteCarloPhase_::Value_::HeldOut;
    const auto held = ValueXccyInstrumentByMonteCarlo(instrument, model, request);
    ASSERT_NE(XccyMonteCarloDetails(held).reservation_.identity_, XccyMonteCarloDetails(value).reservation_.identity_);
    ASSERT_THROW(ValidateXccyMonteCarlo(held, reference, precision), CalibrationError_);
    request.sampling_.batches_ = 1024;
    request.sampling_.pathsPerBatch_ = 2048;
    try {
        static_cast<void>(ValueXccyInstrumentByMonteCarlo(instrument, model, request));
        FAIL();
    } catch (const CalibrationError_& error) {
        ASSERT_EQ(error.Failure().diagnostic_.code_, "CalibrationBudgetExceeded");
        ASSERT_EQ(error.Failure().diagnostic_.field_, "/sampling");
    }
}

TEST(PublicXccyMonteCarloTest, TestRawPartialsFastAndCapabilityBoundaries) {
    using namespace Dal;
    InitGlobalData();
    const auto spec = TestXccyMoments::ModelSpec();
    const auto model = NewXccyGaussianModel("partials", spec);
    const auto instrument = TestXccyMoments::Instrument(XccyNotionalMode_::Value_::RESETTABLE);
    const auto fast = ValueXccyInstrumentByGaussian(instrument, model, "Price");
    const auto prepared = TestXccyMoments::Prepare(instrument, spec);
    const auto core = EvaluateXccyGaussianMoments<double>(prepared, *model);
    ASSERT_DOUBLE_EQ(fast.a_, core.a_);
    ASSERT_DOUBLE_EQ(fast.b_, core.b_);
    ASSERT_DOUBLE_EQ(fast.value_, core.Value(prepared));
    XccyMonteCarloRequest_ request;
    request.measureId_ = "Price";
    request.phase_ = XccyMonteCarloPhase_::Value_::Fit;
    request.sampling_ = {"PseudoRandom", 8, 4, 1024, "dal.mrg32-normal-paths/1"};
    request.derivativeMethod_ = CalibrationDerivative_::Value_::AAD;
    request.parameterLabels_ = {"fxspot:EUR/USD"};
    auto value = ValueXccyInstrumentByMonteCarlo(instrument, model, request);
    const auto details = value.details_;
    value = {};
    ASSERT_EQ(details->estimate_.gradient_.size(), 1);
    ASSERT_EQ(details->estimate_.gradientStandardError_.size(), 1);
    ASSERT_FALSE(details->report_->QuoteRiskAvailable());
    auto capabilities = XccyGaussianInstrumentCapabilities(instrument, model, request);
    ASSERT_TRUE(capabilities->Data().plans_[0].admitted_);
    ASSERT_TRUE(capabilities->Data().plans_[1].admitted_);
    request.parameterLabels_ = {"a:USD"};
    capabilities = XccyGaussianInstrumentCapabilities(instrument, model, request);
    ASSERT_TRUE(capabilities->Data().plans_[0].admitted_);
    ASSERT_FALSE(capabilities->Data().plans_[1].admitted_);
    ASSERT_EQ(capabilities->Data().plans_[1].reasonCode_, "DerivativeUnavailable");
    ASSERT_THROW(XccyMonteCarloDetails({}), CalibrationError_);
    ASSERT_THROW(XccyInstrumentResultGaussianModel({}), CalibrationError_);
}

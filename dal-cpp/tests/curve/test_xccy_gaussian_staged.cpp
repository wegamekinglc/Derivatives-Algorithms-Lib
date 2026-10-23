#include <gtest/gtest.h>

#include <iomanip>
#include <iostream>

#include <dal/curve/xccygaussianmoments.hpp>

#include "xccygaussianmomentfixture.hpp"
#include "xccygaussianrecovery.hpp"

using namespace Dal;
using namespace Dal::TestXccyMoments;

TEST(XccyGaussianStagedTest, TestExplicitProviderRefreshAndRawJacobian) {
    const auto modelSpec = ModelSpec();
    auto spec = TestXccy::Spec(XccyNotionalMode_::Value_::MARK_TO_MARKET);
    spec.domesticCurveBlock_ = modelSpec.market_.domesticBlock_;
    spec.foreignCurveBlock_ = modelSpec.market_.foreignBlock_;
    spec.knotDates_ = {Date_(2027, 11, 8)};
    spec.gaussianDynamics_.reset(new XccyGaussianDynamics_("dynamics", modelSpec.dynamics_));
    const PreparedXccyInstruments_ plan(spec);
    const auto first = plan.Evaluate({.003});
    ASSERT_TRUE(first.gaussianModel_);
    ASSERT_EQ(first.capabilities_[0].backend_, CalibrationBackend_::Value_::FastGaussian);
    const auto prepared = Prepare(spec.observations_[0]->Instrument(), modelSpec, "BasisSpread");
    ASSERT_NEAR(first.modelMeasures_[0], EvaluateXccyGaussianMoments<double>(prepared, *first.gaussianModel_).Value(prepared), 1e-12);
    const auto next = plan.Evaluate({.004});
    ASSERT_NE(first.gaussianModel_->Identity(), next.gaussianModel_->Identity());
    ASSERT_NE(first.report_->Data().execution_.modelIdentity_, next.report_->Data().execution_.modelIdentity_);
    ASSERT_NE(first.modelMeasures_[0], next.modelMeasures_[0]);
    const double h = 1e-5;
    const auto up = plan.Evaluate({.003 + h}, false), down = plan.Evaluate({.003 - h}, false);
    ASSERT_NEAR(first.quoteJacobian_(0, 0), (up.modelMeasures_[0] - down.modelMeasures_[0]) / (2. * h), 1e-8);
    ASSERT_EQ(first.report_->Data().observations_[0].backend_, "FastGaussian");
    ASSERT_FALSE(first.report_->QuoteRiskAvailable());
}

TEST(XccyGaussianStagedTest, TestThreeMaturityIndependentQuotesAndLogBasis) {
    const auto spec = SyntheticSpec();
    const auto target = TestXccyRecovery::Target();
    CurveInstrumentCalibrationOptions_ options;
    options.computeQuoteJacobian_ = true;
    options.jacobianMode_ = CalibrationDerivative_::Value_::AAD;
    const auto result = CalibrateXccyInstruments(spec, options);
    ASSERT_TRUE(result.fitWithinTolerance_);
    ASSERT_LE(result.maxAbsScaledResidual_, 1.);
    ASSERT_EQ(result.parameters_.size(), 3U);
    ASSERT_EQ(result.report_->Data().parameters_[0].unit_, "Dimensionless");
    ASSERT_TRUE(result.gaussianModel_);
    const auto checks = TestXccyRecovery::Qualify(spec, result);
    ASSERT_TRUE(checks.nodes_);
    ASSERT_TRUE(checks.values_);
    ASSERT_TRUE(checks.partials_);
    ASSERT_TRUE(checks.rank_);
    ASSERT_TRUE(checks.secant_);
    ASSERT_TRUE(checks.quotes_);
    std::cout << "Dimensionless logB recovery at quote scale 1e-4\nDate         Target              Initial             Returned            Error\n";
    for (size_t i = 0; i < target.size(); ++i) {
        const double error = result.parameters_[i] - target[i];
        std::cout << Date::ToString(spec.knotDates_[i]) << std::setprecision(12) << std::setw(20) << target[i] << std::setw(20)
                  << result.initialParameters_[i] << std::setw(20) << result.parameters_[i] << std::setw(20) << error << '\n';
        ASSERT_DOUBLE_EQ(result.initialParameters_[i], spec.solver_.initialGuessPerNode_[i]);
        ASSERT_LT(std::abs(error), std::abs(result.initialParameters_[i] - target[i]));
        ASSERT_EQ(result.report_->Axes().observations_[i]->Quote()->Fingerprint(), spec.observations_[i]->Quote()->Fingerprint());
        ASSERT_EQ(result.freeParameters_[i].component_, CurveFreeParameterComponent_::Value_::LOG_DISCOUNT_FACTOR);
    }
    std::cout << "Secant error " << checks.secantError_ << "; refinement " << checks.refinement_ << "; scaled move " << checks.scaledMove_
              << "; node upper bound not qualified\n";
}

TEST(XccyGaussianStagedTest, TestRecoveryQualificationRejectsFalsePrecisionAndStaleInitial) {
    const auto spec = SyntheticSpec();
    CurveInstrumentCalibrationOptions_ options;
    options.jacobianMode_ = CalibrationDerivative_::Value_::AAD;
    const PreparedXccyInstruments_ plan(spec, options);
    auto alternative = TestXccyRecovery::Target();
    alternative[2] += .001;
    const auto alternativeChecks = TestXccyRecovery::Qualify(spec, plan.Evaluate(alternative));
    ASSERT_TRUE(alternativeChecks.Accepted());
    ASSERT_FALSE(TestXccyRecovery::RecoveredTo1eMinus5(alternative));
    ASSERT_GT(alternativeChecks.scaledMove_, .23L);
    ASSERT_LT(alternativeChecks.scaledMove_, .24L);
    const auto initialChecks = TestXccyRecovery::Qualify(spec, plan.Evaluate(spec.solver_.initialGuessPerNode_));
    ASSERT_TRUE(initialChecks.nodes_);
    ASSERT_TRUE(initialChecks.values_);
    ASSERT_TRUE(initialChecks.partials_);
    ASSERT_TRUE(initialChecks.rank_);
    ASSERT_TRUE(initialChecks.secant_);
    ASSERT_FALSE(initialChecks.quotes_);
    ASSERT_FALSE(initialChecks.Accepted());
    ASSERT_GT(initialChecks.scaledMove_, 19.L);
    std::cout << "Alternative scaled move " << alternativeChecks.scaledMove_ << "; stale initial " << initialChecks.scaledMove_ << '\n';
}

TEST(XccyGaussianStagedTest, TestRecoveryQualificationRejectsMixedCandidateAndAxes) {
    const auto spec = SyntheticSpec();
    CurveInstrumentCalibrationOptions_ options;
    options.jacobianMode_ = CalibrationDerivative_::Value_::AAD;
    options.computeQuoteJacobian_ = true;
    const auto result = CalibrateXccyInstruments(spec, options);
    ASSERT_TRUE(TestXccyRecovery::Qualify(spec, result).Accepted());
    {
        auto wrong = result;
        std::swap(wrong.freeParameters_[1].date_, wrong.freeParameters_[2].date_);
        ASSERT_FALSE(TestXccyRecovery::Qualify(spec, wrong).nodes_);
    }
    {
        auto wrong = result;
        wrong.parameters_[2] += .001;
        const auto checks = TestXccyRecovery::Qualify(spec, wrong);
        ASSERT_FALSE(checks.nodes_);
        ASSERT_FALSE(checks.values_);
        ASSERT_FALSE(checks.partials_);
        ASSERT_FALSE(checks.Accepted());
    }
    {
        auto wrong = result;
        wrong.modelMeasures_[1] += 1e-4;
        ASSERT_FALSE(TestXccyRecovery::Qualify(spec, wrong).values_);
    }
    {
        auto wrong = result;
        for (int row = 0; row < 3; ++row)
            std::swap(wrong.quoteJacobian_(row, 1), wrong.quoteJacobian_(row, 2));
        ASSERT_FALSE(TestXccyRecovery::Qualify(spec, wrong).partials_);
    }
    {
        auto wrong = result;
        for (int row = 0; row < 3; ++row)
            wrong.quoteJacobian_(row, 2) = 0.;
        const auto checks = TestXccyRecovery::Qualify(spec, wrong);
        ASSERT_FALSE(checks.rank_);
        ASSERT_FALSE(checks.partials_);
        ASSERT_FALSE(checks.Accepted());
    }
}

TEST(XccyGaussianStagedTest, TestFinalDerivativeFailureRetainsGaussianCandidate) {
    Handle_<XccyInstrumentCalibrationCandidate_> candidate;
    {
        const auto model = ModelSpec();
        auto spec = TestXccy::Spec(XccyNotionalMode_::Value_::MARK_TO_MARKET);
        spec.domesticCurveBlock_ = model.market_.domesticBlock_;
        spec.foreignCurveBlock_ = model.market_.foreignBlock_;
        spec.gaussianDynamics_.reset(new XccyGaussianDynamics_("dynamics", model.dynamics_));
        spec.knotDates_ = {Date_(2027, 11, 8)};
        const auto quote = NewCalibrationMarketQuote("oracle", -.06705259701832678066767811569223010914, CalibrationValueUnit_::Parse("BasisSpread"));
        spec.observations_ = {NewCalibrationObservation("row", Instrument(XccyNotionalMode_::Value_::MARK_TO_MARKET), "BasisSpread", quote, 1e-4)};
        spec.solver_.initialGuess_ = .003;
        CurveInstrumentCalibrationOptions_ options;
        options.computeQuoteJacobian_ = true;
        options.differenceStep_ = 1e-30;
        try {
            static_cast<void>(CalibrateXccyInstruments(spec, options));
            FAIL();
        } catch (const CalibrationError_& error) {
            ASSERT_EQ(error.Failure().diagnostic_.code_, "CalibrationNumericalFailure");
            candidate = handle_cast<XccyInstrumentCalibrationCandidate_>(error.Failure().candidate_);
            ASSERT_TRUE(candidate);
        }
    }
    const auto& result = candidate->Result();
    ASSERT_TRUE(result.gaussianModel_);
    ASSERT_TRUE(result.curve_);
    ASSERT_EQ(result.parameters_.size(), 1U);
    ASSERT_EQ(result.quoteJacobian_.Rows(), 0);
    ASSERT_EQ(result.report_->Data().execution_.modelIdentity_, result.gaussianModel_->Identity());
    ASSERT_NEAR(result.modelMeasures_[0], -.06705259701832678066767811569223010914, 1e-12);
}

TEST(XccyGaussianStagedTest, TestApproximateUsesOriginalScalesAndMetric) {
    const auto model = ModelSpec();
    auto spec = TestXccy::Spec(XccyNotionalMode_::Value_::MARK_TO_MARKET);
    spec.domesticCurveBlock_ = model.market_.domesticBlock_;
    spec.foreignCurveBlock_ = model.market_.foreignBlock_;
    spec.gaussianDynamics_.reset(new XccyGaussianDynamics_("dynamics", model.dynamics_));
    spec.knotDates_ = {Date_(2027, 11, 8)};
    const auto quote = NewCalibrationMarketQuote("oracle", -.06705259701832678066767811569223010914, CalibrationValueUnit_::Parse("BasisSpread"));
    spec.observations_ = {NewCalibrationObservation("row", Instrument(XccyNotionalMode_::Value_::MARK_TO_MARKET), "BasisSpread", quote, 1e-4)};
    spec.solver_.initialGuess_ = .004;
    spec.solver_.parameterScales_ = {.01};
    spec.solver_.solveMode_ = CurveSolveMode_::Value_::APPROXIMATE;
    spec.solver_.fitWeight_ = 1e8;
    spec.solver_.fitNormTolerance_ = .5;
    CurveInstrumentCalibrationOptions_ options;
    options.jacobianMode_ = CalibrationDerivative_::Value_::AAD;
    const auto result = CalibrateXccyInstruments(spec, options);
    ASSERT_TRUE(result.fitWithinTolerance_);
    ASSERT_LE(result.scaledNorm_, .5);
    const double move = result.parameters_[0] - .004;
    const double expected = .5 * result.nativeWeights_(0, 0) * move * move + .5e8 * result.scaledResiduals_[0] * result.scaledResiduals_[0];
    ASSERT_TRUE(result.report_->Data().summary_.objective_);
    ASSERT_NEAR(*result.report_->Data().summary_.objective_, expected, 1e-10);
    ASSERT_NEAR(result.weights_(0, 0), .01 * .01 * result.nativeWeights_(0, 0), 1e-12);
}

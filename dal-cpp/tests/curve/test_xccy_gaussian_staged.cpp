#include <gtest/gtest.h>

#include <dal/curve/xccygaussianmoments.hpp>

#include "xccygaussianmomentfixture.hpp"

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
    Vector_<> target;
    for (const auto& date : spec.knotDates_)
        target.push_back(-.003 * (date - TestXccy::TODAY) / 365.);
    CurveInstrumentCalibrationOptions_ options;
    options.computeQuoteJacobian_ = true;
    options.jacobianMode_ = CalibrationDerivative_::Value_::AAD;
    const auto result = CalibrateXccyInstruments(spec, options);
    ASSERT_TRUE(result.fitWithinTolerance_);
    ASSERT_LE(result.maxAbsScaledResidual_, 1.);
    ASSERT_EQ(result.parameters_.size(), 3U);
    ASSERT_EQ(result.report_->Data().parameters_[0].unit_, "Dimensionless");
    ASSERT_TRUE(result.gaussianModel_);
    for (size_t i = 0; i < target.size(); ++i) {
        ASSERT_NEAR(result.parameters_[i], target[i], 1e-5);
        ASSERT_EQ(result.report_->Axes().observations_[i]->Quote()->Fingerprint(), spec.observations_[i]->Quote()->Fingerprint());
        ASSERT_EQ(result.freeParameters_[i].component_, CurveFreeParameterComponent_::Value_::LOG_DISCOUNT_FACTOR);
    }
    const auto& j = result.quoteJacobian_;
    const double determinant = j(0, 0) * (j(1, 1) * j(2, 2) - j(1, 2) * j(2, 1)) - j(0, 1) * (j(1, 0) * j(2, 2) - j(1, 2) * j(2, 0)) +
                               j(0, 2) * (j(1, 0) * j(2, 1) - j(1, 1) * j(2, 0));
    ASSERT_GT(std::abs(determinant), 1e-4);
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

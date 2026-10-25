#include <gtest/gtest.h>

#include <dal-public/src/global.hpp>
#include <dal-public/src/storage.hpp>
#include <dal/platform/platform.hpp>

#include "xccy_acceptance_fixture.hpp"

using namespace Dal;

TEST(PublicXccyAcceptanceTest, TestCompleteCorePayloadAndOrderedPartialsAcrossPhases) {
    InitGlobalData();
    AAD::Clear(*AAD::Tape());
    const auto spec = TestXccyMoments::ModelSpec();
    const auto model = TestXccyAcceptance::Model();
    ASSERT_EQ(model->Identity(), NewXccyGaussianModel("acceptance", spec)->Identity());
    RecordProperty("model_archive", std::string(WriteObjectJson(*model).c_str()));
    for (const auto& c : TestXccyAcceptance::Cases()) {
        SCOPED_TRACE(c.mode_.String());
        const auto instrument = TestXccyAcceptance::Instrument(c.mode_);
        const auto source = DumpCalibrationInstrumentJson(instrument);
        ASSERT_EQ(source, DumpCalibrationInstrumentJson(TestXccyMoments::Instrument(c.mode_)));
        RecordProperty(std::string(c.mode_.String()) + "_source", std::string(source.c_str()));
        const auto request = TestXccyAcceptance::Request(c.phase_, c.measure_);
        const auto core = TestXccyAcceptance::Core(instrument, model, request);
        const auto actual = ValueXccyInstrumentByMonteCarlo(instrument, model, request);
        RecordProperty(std::string(c.measure_.data(), c.measure_.size()) + "_" + std::to_string(c.firstPath_),
                       std::string(DumpXccyMonteCarloResultJson(actual).c_str()));
        ASSERT_EQ(DumpXccyMonteCarloResultJson(actual), TestXccyAcceptance::Json(core));
        ASSERT_DOUBLE_EQ(actual.mean_, core.estimate_.mean_);
        ASSERT_DOUBLE_EQ(*actual.standardError_, core.estimate_.standardError_);
        const auto& details = XccyMonteCarloDetails(actual);
        ASSERT_EQ(details.estimate_.parameterLabels_, request.parameterLabels_);
        ASSERT_EQ(details.coefficientCovariance_.Rows(), 6);
        ASSERT_EQ(details.estimate_.gradient_.size(), 2);
        ASSERT_EQ(details.reservation_.ranges_.front().firstPath_, c.firstPath_);
        ASSERT_EQ(details.reservation_.ranges_.back().pathEnd_, c.firstPath_ + 32);
        ASSERT_EQ(details.consumedCoordinates_, 32 * details.mapping_.dimension_);
        ASSERT_EQ(details.estimate_.validation_, c.phase_ == XccyMonteCarloPhase_::Value_::Validation);
        ASSERT_EQ(details.estimate_.heldOut_, c.phase_ == XccyMonteCarloPhase_::Value_::HeldOut);
        ASSERT_EQ(DumpCalibrationInstrumentJson(instrument), source);
        const auto fast = ValueXccyInstrumentByGaussian(instrument, model, c.measure_, request.fixings_);
        const auto prepared = PrepareCalibrationInstrument(instrument, c.measure_, TestXccyAcceptance::Context(*model, request.fixings_));
        const auto moments = EvaluateXccyGaussianMoments<double>(prepared, *model);
        ASSERT_DOUBLE_EQ(fast.value_, moments.Value(prepared));
        ASSERT_EQ(AAD::Tape()->nodes_.Size(), 0);
    }
    AAD::Clear(*AAD::Tape());
}

TEST(PublicXccyAcceptanceTest, TestStagedCandidateSurvivesInputsAndFeedsCoreAndPublic) {
    InitGlobalData();
    XccyInstrumentCalibrationResult_ candidate;
    Handle_<CalibrationInstrument_> instrument;
    {
        auto spec = TestXccyMoments::SyntheticSpec();
        instrument = spec.observations_.front()->Instrument();
        candidate = PreparedXccyInstruments_(spec).Evaluate(spec.solver_.initialGuessPerNode_);
    }
    auto model = XccyInstrumentResultGaussianModel(candidate);
    ASSERT_TRUE(model);
    ASSERT_TRUE(candidate.curve_);
    ASSERT_NE(handle_cast<Storable_>(candidate.curve_).get(), handle_cast<Storable_>(model).get());
    auto request = TestXccyAcceptance::Request(XccyMonteCarloPhase_::Value_::Validation, "BasisSpread");
    auto core = TestXccyAcceptance::Core(instrument, model, request);
    const auto value = ValueXccyInstrumentByMonteCarlo(instrument, model, request);
    ASSERT_EQ(DumpXccyMonteCarloResultJson(value), TestXccyAcceptance::Json(core));
    {
        const auto fast = ValueXccyInstrumentByGaussian(instrument, model, "BasisSpread", request.fixings_);
        RecordProperty("staged_fast_report", std::string(DumpCalibrationReportJson(fast.report_).c_str()));
        RecordProperty("staged_report", std::string(DumpCalibrationReportJson(candidate.report_).c_str()));
        ASSERT_NEAR(fast.value_, candidate.modelMeasures_.front(), 1e-12);
    }
    candidate = {};
    core = {};
    instrument.reset();
    model.reset();
    request = {};
    ASSERT_FALSE(DumpXccyMonteCarloResultJson(value).empty());
    ASSERT_TRUE(value.report_);
    ASSERT_EQ(AAD::Tape()->nodes_.Size(), 0);
}

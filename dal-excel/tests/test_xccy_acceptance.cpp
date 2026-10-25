#include <gtest/gtest.h>

#include <limits>

#include <dal-excel/src/__gsrinstrument.hpp>
#include <dal-excel/src/__script_test_api.hpp>
#include <dal-excel/src/__xccymontecarlo.hpp>
#include <dal-public/src/global.hpp>
#include <dal/platform/platform.hpp>

#include "../../dal-public/tests/xccy_acceptance_fixture.hpp"

using namespace Dal;

TEST(ExcelXccyAcceptanceTest, TestCompleteCorePayloadAndOrderedSpillSurviveInputs) {
    InitGlobalData();
    Excel::ScriptTestInitialize(1);
    for (const auto& c : TestXccyAcceptance::Cases()) {
        SCOPED_TRACE(c.mode_.String());
        Handle_<StorableXccyMonteCarloOutcome_> outcome;
        XccyMcResult_ core;
        Vector_<String_> labels;
        {
            const auto model = TestXccyAcceptance::Model();
            const auto instrument = TestXccyAcceptance::Instrument(c.mode_);
            auto request = TestXccyAcceptance::Request(c.phase_, c.measure_);
            labels = request.parameterLabels_;
            core = TestXccyAcceptance::Core(instrument, model, request);
            const Handle_<StorableXccyMonteCarloRequest_> input(new StorableXccyMonteCarloRequest_(request));
            XccyInstrument_ValueByMonteCarlo(instrument, model, input, &outcome);
        }
        ASSERT_EQ(outcome->val_.status_, "Estimate");
        const auto expectedJson = TestXccyAcceptance::Json(core);
        const auto expectedGradient = core.estimate_.gradient_;
        const auto expectedSe = core.estimate_.gradientStandardError_;
        const auto modelIdentity = core.modelIdentity_;
        core = {};
        Vector_<String_> chunks;
        XccyMonteCarloOutcome_Get_Json(outcome, &chunks);
        String_ json;
        for (const auto& chunk : chunks)
            json += chunk;
        RecordProperty(std::string(c.measure_.data(), c.measure_.size()) + "_" + std::to_string(c.firstPath_), std::string(json.c_str()));
        ASSERT_EQ(json, expectedJson);
        Matrix_<Cell_> spill;
        XccyMonteCarloOutcome_Get_Partials(outcome, &spill);
        ASSERT_EQ(spill.Rows(), 3);
        ASSERT_EQ(spill.Cols(), 3);
        ASSERT_EQ(Cell::ToString(spill(0, 0)), "parameter_label");
        ASSERT_EQ(Cell::ToString(spill(0, 1)), "native_partial");
        ASSERT_EQ(Cell::ToString(spill(0, 2)), "standard_error");
        for (int row = 0; row < 2; ++row) {
            ASSERT_EQ(Cell::ToString(spill(row + 1, 0)), labels[row]);
            ASSERT_DOUBLE_EQ(Cell::ToDouble(spill(row + 1, 1)), expectedGradient[row]);
            ASSERT_DOUBLE_EQ(Cell::ToDouble(spill(row + 1, 2)), expectedSe[row]);
        }
        Handle_<XccyGaussianModelData_> retained;
        XccyMonteCarloOutcome_Get_Model(outcome, &retained);
        ASSERT_EQ(retained->Identity(), modelIdentity);
    }
}

TEST(ExcelXccyAcceptanceTest, TestTrailingSettingsPreserveExactErrorLocation) {
    InitGlobalData();
    {
        Matrix_<Cell_> fields(4, 2);
        fields(0, 0) = "error_scale";
        fields(0, 1) = .01;
        fields(3, 0) = "sigma";
        fields(3, 1) = 3.;
        Handle_<StorableXccyMonteCarloPrecision_> precision;
        XccyMonteCarloPrecision_New(fields, &precision);
        ASSERT_DOUBLE_EQ(precision->val_.sigma_, 3.);
    }
    for (const auto bad : {Cell_(true), Cell_(std::numeric_limits<double>::infinity()), Cell_("3")}) {
        Matrix_<Cell_> fields(4, 2);
        fields(0, 0) = "error_scale";
        fields(0, 1) = .01;
        fields(3, 0) = "sigma";
        fields(3, 1) = bad;
        Handle_<StorableXccyMonteCarloPrecision_> precision;
        try {
            XccyMonteCarloPrecision_New(fields, &precision);
            FAIL() << "Invalid trailing field accepted";
        } catch (const Exception_& error) {
            const std::string message(error.what());
            ASSERT_NE(message.find("XccyMonteCarloPrecision_New"), std::string::npos);
            ASSERT_NE(message.find("settings row=4 column=2"), std::string::npos);
            ASSERT_NE(message.find("field=sigma"), std::string::npos);
        }
    }
    for (const char* key : {"SIGMA", "unrecognized"}) {
        Matrix_<Cell_> fields(4, 2);
        fields(0, 0) = "error_scale";
        fields(0, 1) = .01;
        fields(1, 0) = "sigma";
        fields(1, 1) = 3.;
        fields(3, 0) = key;
        fields(3, 1) = 3.;
        Handle_<StorableXccyMonteCarloPrecision_> precision;
        try {
            XccyMonteCarloPrecision_New(fields, &precision);
            FAIL() << "Invalid trailing key accepted";
        } catch (const Exception_& error) {
            const std::string message(error.what());
            ASSERT_NE(message.find("settings row=4 column=1"), std::string::npos);
            ASSERT_NE(message.find(key), std::string::npos);
        }
    }
}

TEST(ExcelXccyAcceptanceTest, TestLastCompleteStagedFailureCandidateFeedsMcAfterInputDestruction) {
    InitGlobalData();
    Excel::ScriptTestInitialize(1);
    Handle_<CalibrationInstrument_> instrument;
    Handle_<XccyGaussianModelData_> model;
    {
        auto spec = TestXccyMoments::SyntheticSpec();
        spec.solver_.maxEvaluations_ = 1;
        instrument = spec.observations_.front()->Instrument();
        const Handle_<StorableXccyInstrumentCalibrationSpec_> input(new StorableXccyInstrumentCalibrationSpec_(spec));
        Handle_<StorableCalibrationOutcome_> staged;
        Calibrate_XccyInstruments(input, {}, &staged);
        ASSERT_EQ(staged->status_, "Failed");
        ASSERT_EQ(staged->report_->Errors().front().code_, "CalibrationBudgetExceeded");
        ASSERT_TRUE(staged->curve_);
        Handle_<ModelData_> candidate;
        CalibrationOutcome_Get_Model(staged, &candidate);
        model = handle_cast<XccyGaussianModelData_>(candidate);
        ASSERT_TRUE(model);
        ASSERT_NE(handle_cast<Storable_>(staged->curve_).get(), handle_cast<Storable_>(model).get());
    }
    auto request = TestXccyAcceptance::Request(XccyMonteCarloPhase_::Value_::Validation, "BasisSpread");
    String_ expected;
    {
        const auto core = TestXccyAcceptance::Core(instrument, model, request);
        expected = TestXccyAcceptance::Json(core);
    }
    Handle_<StorableXccyMonteCarloRequest_> input(new StorableXccyMonteCarloRequest_(request));
    Handle_<StorableXccyMonteCarloOutcome_> outcome;
    XccyInstrument_ValueByMonteCarlo(instrument, model, input, &outcome);
    input.reset();
    request = {};
    instrument.reset();
    model.reset();
    ASSERT_EQ(outcome->val_.status_, "Estimate");
    Vector_<String_> chunks;
    XccyMonteCarloOutcome_Get_Json(outcome, &chunks);
    String_ actual;
    for (const auto& chunk : chunks)
        actual += chunk;
    ASSERT_EQ(actual, expected);
    RecordProperty("staged_candidate_payload", std::string(actual.c_str()));
}

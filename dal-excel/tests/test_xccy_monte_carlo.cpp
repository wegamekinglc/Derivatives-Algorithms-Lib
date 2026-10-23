#include <gtest/gtest.h>

#include <dal-excel/src/__gsrinstrument.hpp>
#include <dal-excel/src/__script_test_api.hpp>
#include <dal-excel/src/__xccymontecarlo.hpp>
#include <dal-public/src/global.hpp>

#include "../../dal-cpp/tests/curve/xccygaussianmomentfixture.hpp"

TEST(ExcelXccyMonteCarloTest, TestSameCoreInputAndOwningOutcome) {
    using namespace Dal;
    InitGlobalData();
    Excel::ScriptTestInitialize(1);
    const auto spec = TestXccyMoments::ModelSpec();
    Handle_<StorableXccyGaussianModelSpec_> input(new StorableXccyGaussianModelSpec_(spec));
    Handle_<XccyGaussianModelData_> model;
    XccyGaussianModel_New("excel", input, &model);
    XccyMonteCarloRequest_ request;
    request.measureId_ = "Price";
    request.phase_ = XccyMonteCarloPhase_::Value_::Validation;
    request.sampling_ = {"PseudoRandom", 8, 4, 1024, "dal.mrg32-normal-paths/1"};
    request.fixings_.reset(new MarketFixingSnapshot_({}));
    Handle_<StorableXccyMonteCarloRequest_> settings(new StorableXccyMonteCarloRequest_(request));
    Handle_<StorableXccyMonteCarloOutcome_> outcome;
    auto instrument = TestXccyMoments::Instrument(XccyNotionalMode_::Value_::MARK_TO_MARKET);
    XccyInstrument_ValueByMonteCarlo(instrument, model, settings, &outcome);
    const auto direct = ValueXccyInstrumentByMonteCarlo(instrument, model, request);
    Handle_<StorableCalibrationOutcome_> fast;
    XccyInstrument_ValueByGaussian(instrument, model, "Price", {}, &fast);
    ASSERT_EQ(fast->status_, "Succeeded");
    ASSERT_DOUBLE_EQ(*fast->mean_, ValueXccyInstrumentByGaussian(instrument, model, "Price").value_);
    ASSERT_EQ(fast->report_->Data().observations_[0].backend_, "FastGaussian");
    ASSERT_TRUE(outcome->val_.result_);
    ASSERT_DOUBLE_EQ(outcome->val_.result_->mean_, direct.mean_);
    ASSERT_EQ(DumpXccyMonteCarloResultJson(*outcome->val_.result_), DumpXccyMonteCarloResultJson(direct));
    input.reset();
    model.reset();
    instrument.reset();
    settings.reset();
    Vector_<String_> chunks;
    XccyMonteCarloOutcome_Get_Json(outcome, &chunks);
    ASSERT_FALSE(chunks.empty());
    ASSERT_EQ(outcome->val_.status_, "Estimate");
    ASSERT_TRUE(outcome->val_.report_);
}

namespace {
    Dal::Matrix_<Dal::Cell_> McFields(std::initializer_list<std::pair<const char*, Dal::Cell_>> rows) {
        Dal::Matrix_<Dal::Cell_> result(static_cast<int>(rows.size()), 2);
        int row = 0;
        for (const auto& [key, value] : rows) {
            result(row, 0) = key;
            result(row++, 1) = value;
        }
        return result;
    }
} // namespace

TEST(ExcelXccyMonteCarloTest, TestRequiredPhaseSettingsAndTypedSnapshotMarkers) {
    using namespace Dal;
    InitGlobalData();
    Excel::ScriptTestInitialize(1);
    const Handle_<StorableCalibrationMonteCarloSampling_> sampling(
        new StorableCalibrationMonteCarloSampling_({"PseudoRandom", 8, 4, 1024, "dal.mrg32-normal-paths/1"}));
    Handle_<StorableXccyMonteCarloRequest_> request;
    for (const Cell_& phase : {Cell_(), Cell_(""), Cell_("validation")}) {
        try {
            XccyMonteCarloRequest_New("Price", phase, sampling, {}, &request);
            FAIL();
        } catch (const CalibrationError_& error) {
            ASSERT_EQ(error.Failure().diagnostic_.code_, "InvalidCalibrationPlan");
            ASSERT_EQ(error.Failure().diagnostic_.field_, "/phase");
            ASSERT_NE(std::string(error.what()).find("XccyMonteCarloRequest_New"), std::string::npos);
        }
    }
    XccyMonteCarloRequest_New("Price", Cell_("Validation"), sampling, {}, &request);
    ASSERT_FALSE(request->val_.fixings_);
    const Handle_<StorableMarketFixingSnapshot_> empty(
        new StorableMarketFixingSnapshot_(Handle_<MarketFixingSnapshot_>(new MarketFixingSnapshot_({}))));
    const auto key = Excel::ScriptTestStoreRepository(handle_cast<Storable_>(empty));
    XccyMonteCarloRequest_New("Price", Cell_("Validation"), sampling, McFields({{"fixings", Cell_(key)}}), &request);
    ASSERT_TRUE(request->val_.fixings_);
    for (const auto& fields :
         {McFields({{"phase", Cell_("Fit")}}), McFields({{"simulation", Cell_(1.)}}), McFields({{"fixings", Cell_(key)}, {"FIXINGS", Cell_(key)}})}) {
        try {
            XccyMonteCarloRequest_New("Price", Cell_("Validation"), sampling, fields, &request);
            FAIL();
        } catch (const Exception_& error) {
            ASSERT_NE(std::string(error.what()).find("XccyMonteCarloRequest_New"), std::string::npos);
            ASSERT_NE(std::string(error.what()).find("column="), std::string::npos);
        }
    }
}

TEST(ExcelXccyMonteCarloTest, TestNamedDynamicsPreciseCellAndNormalizedDuplicate) {
    using namespace Dal;
    InitGlobalData();
    const auto spec = TestXccyMoments::ModelSpec();
    Matrix_<Cell_> buckets(2, 4), correlation(4, 4);
    const Vector_<String_> columns{"end_date", "sigma_domestic", "sigma_foreign", "sigma_fx"};
    for (int col = 0; col < 4; ++col)
        buckets(0, col) = columns[col];
    buckets(1, 0) = spec.dynamics_.bucketEndDates_[0];
    buckets(1, 1) = .01;
    buckets(1, 2) = .009;
    buckets(1, 3) = .12;
    correlation(0, 0) = "factor";
    for (int i = 0; i < 3; ++i) {
        correlation(0, i + 1) = correlation(i + 1, 0) = spec.dynamics_.factorNames_[i];
        for (int j = 0; j < 3; ++j)
            correlation(i + 1, j + 1) = spec.dynamics_.correlation_(i, j);
    }
    const auto fields = McFields({{"domestic_a", Cell_(.03)}, {"foreign_a", Cell_(.07)}});
    Handle_<StorableXccyGaussianDynamicsSpec_> dynamics;
    XccyGaussianDynamicsSpec_New(fields, buckets, correlation, &dynamics);
    ASSERT_EQ(dynamics->val_.factorNames_, spec.dynamics_.factorNames_);
    buckets(1, 2) = true;
    try {
        XccyGaussianDynamicsSpec_New(fields, buckets, correlation, &dynamics);
        FAIL();
    } catch (const Exception_& error) {
        const std::string message = error.what();
        ASSERT_NE(message.find("XccyGaussianDynamicsSpec_New"), std::string::npos);
        ASSERT_NE(message.find("buckets row=2 column=3"), std::string::npos);
        ASSERT_NE(message.find("field=sigma_foreign"), std::string::npos);
    }
    buckets(1, 2) = .009;
    buckets(0, 2) = "SIGMA_DOMESTIC";
    ASSERT_THROW(XccyGaussianDynamicsSpec_New(fields, buckets, correlation, &dynamics), Exception_);
}

TEST(ExcelXccyMonteCarloTest, TestNewFieldsKeepOriginalRowsAndLegacyRejectsBlankRows) {
    using namespace Dal;
    Matrix_<Cell_> cells(3, 2);
    cells(0, 0) = "error_scale";
    cells(0, 1) = .01;
    cells(2, 0) = "sigma";
    cells(2, 1) = true;
    Handle_<StorableXccyMonteCarloPrecision_> precision;
    try {
        XccyMonteCarloPrecision_New(cells, &precision);
        FAIL();
    } catch (const Exception_& error) {
        const std::string message = error.what();
        ASSERT_NE(message.find("settings row=3 column=2"), std::string::npos);
        ASSERT_NE(message.find("field=sigma"), std::string::npos);
        ASSERT_NE(message.find("finite number excluding bool"), std::string::npos);
    }
    auto old = McFields({{"instrument_id", Cell_("old")}, {"discount_route", Cell_("USD.OIS")}, {"min_abs_annuity", Cell_(1e-12)}});
    Handle_<StorableXccyScriptImportSettings_> legacy;
    ASSERT_NO_THROW(XccyScriptImportSettings_New(old, &legacy));
    old(2, 0) = Cell_();
    old(2, 1) = Cell_();
    ASSERT_THROW(XccyScriptImportSettings_New(old, &legacy), Exception_);
}

TEST(ExcelXccyMonteCarloTest, TestUnavailablePartialsAndOwnedFailureSpillColumns) {
    using namespace Dal;
    InitGlobalData();
    const auto model = NewXccyGaussianModel("failure", TestXccyMoments::ModelSpec());
    auto instrument = TestXccyMoments::Instrument();
    XccyMonteCarloRequest_ request;
    request.measureId_ = "Price";
    request.phase_ = XccyMonteCarloPhase_::Value_::Validation;
    request.sampling_ = {"PseudoRandom", 8, 4, 1024, "dal.mrg32-normal-paths/1"};
    Handle_<StorableXccyMonteCarloRequest_> input(new StorableXccyMonteCarloRequest_(request));
    Handle_<StorableXccyMonteCarloOutcome_> outcome;
    XccyInstrument_ValueByMonteCarlo(instrument, model, input, &outcome);
    Matrix_<Cell_> partials;
    ASSERT_THROW(XccyMonteCarloOutcome_Get_Partials(outcome, &partials), CalibrationError_);
    request.derivativeMethod_ = CalibrationDerivative_::Value_::AAD;
    request.parameterLabels_ = {"a:USD"};
    input.reset(new StorableXccyMonteCarloRequest_(request));
    XccyInstrument_ValueByMonteCarlo(instrument, model, input, &outcome);
    input.reset();
    instrument.reset();
    ASSERT_EQ(outcome->val_.status_, "Failed");
    double mean;
    ASSERT_THROW(XccyMonteCarloOutcome_Get_Mean(outcome, &mean), CalibrationError_);
    const auto errors = CalibrationReportTable(outcome->val_.report_, "errors");
    ASSERT_EQ(errors.Cols(), 14);
    ASSERT_EQ(Cell::ToString(errors(0, 0)), "code");
    ASSERT_EQ(Cell::ToString(errors(0, 3)), "field");
    ASSERT_EQ(Cell::ToString(errors(1, 0)), "DerivativeUnavailable");
    ASSERT_EQ(Cell::ToString(errors(1, 5)), "xccy-2027-11-04");
    request.derivativeMethod_ = CalibrationDerivative_::Value_::None;
    request.parameterLabels_.clear();
    request.sampling_.batches_ = 1024;
    request.sampling_.pathsPerBatch_ = 2048;
    input.reset(new StorableXccyMonteCarloRequest_(request));
    XccyInstrument_ValueByMonteCarlo(TestXccyMoments::Instrument(), model, input, &outcome);
    const auto budgetErrors = CalibrationReportTable(outcome->val_.report_, "errors");
    ASSERT_EQ(Cell::ToString(budgetErrors(1, 0)), "CalibrationBudgetExceeded");
    ASSERT_EQ(Cell::ToString(budgetErrors(1, 3)), "/sampling");
}

TEST(ExcelXccyMonteCarloTest, TestStagedCandidateModelLifetime) {
    using namespace Dal;
    InitGlobalData();
    auto spec = TestXccyMoments::SyntheticSpec();
    spec.solver_.maxEvaluations_ = 1;
    Handle_<StorableXccyInstrumentCalibrationSpec_> input(new StorableXccyInstrumentCalibrationSpec_(spec));
    Handle_<StorableCalibrationOutcome_> outcome;
    Calibrate_XccyInstruments(input, {}, &outcome);
    input.reset();
    spec = {};
    ASSERT_EQ(outcome->status_, "Failed");
    Handle_<ModelData_> candidate;
    CalibrationOutcome_Get_Model(outcome, &candidate);
    ASSERT_TRUE(handle_cast<XccyGaussianModelData_>(candidate));
    ASSERT_TRUE(outcome->curve_);
}

TEST(ExcelXccyMonteCarloTest, TestValidationFlagsKeepEstimateAndRejectHeldOut) {
    using namespace Dal;
    InitGlobalData();
    const auto model = NewXccyGaussianModel("validation", TestXccyMoments::ModelSpec());
    const auto instrument = TestXccyMoments::Instrument();
    XccyMonteCarloRequest_ request;
    request.measureId_ = "Price";
    request.phase_ = XccyMonteCarloPhase_::Value_::Validation;
    request.sampling_ = {"PseudoRandom", 8, 4, 1024, "dal.mrg32-normal-paths/1"};
    Handle_<StorableXccyMonteCarloOutcome_> outcome;
    XccyInstrument_ValueByMonteCarlo(instrument, model, Handle_<StorableXccyMonteCarloRequest_>(new StorableXccyMonteCarloRequest_(request)),
                                     &outcome);
    Handle_<StorableXccyMonteCarloReference_> reference;
    XccyMonteCarloReference_New(McFields({{"value", Cell_(outcome->val_.result_->mean_)}, {"numerical_error", Cell_(0.)}}), &reference);
    Handle_<StorableXccyMonteCarloPrecision_> precision;
    XccyMonteCarloPrecision_New(McFields({{"error_scale", Cell_(1e-12)}, {"epsilon_value", Cell_(1e-16)}}), &precision);
    Handle_<StorableXccyMonteCarloValidation_> validation;
    XccyMonteCarloOutcome_Validate(outcome, reference, precision, &validation);
    Matrix_<Cell_> flags;
    XccyMonteCarloValidation_Get_Flags(validation, &flags);
    ASSERT_TRUE(Cell::ToBool(flags(2, 1)));
    ASSERT_FALSE(Cell::ToBool(flags(3, 1)));
    ASSERT_TRUE(Cell::IsEmpty(flags(7, 1)));
    ASSERT_TRUE(Cell::ToBool(flags(9, 1)));
    ASSERT_TRUE(outcome->val_.result_);
    request.phase_ = XccyMonteCarloPhase_::Value_::HeldOut;
    XccyInstrument_ValueByMonteCarlo(instrument, model, Handle_<StorableXccyMonteCarloRequest_>(new StorableXccyMonteCarloRequest_(request)),
                                     &outcome);
    ASSERT_THROW(XccyMonteCarloOutcome_Validate(outcome, reference, precision, &validation), CalibrationError_);
}

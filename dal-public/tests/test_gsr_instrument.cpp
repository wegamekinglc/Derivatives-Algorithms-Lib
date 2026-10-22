#include <gtest/gtest.h>
#include <rapidjson/document.h>

#include <dal-public/src/global.hpp>
#include <dal-public/src/gsr.hpp>
#include <dal/model/gsrbondoption.hpp>
#include <dal/model/gsrscriptcalibration.hpp>

using namespace Dal;

namespace {
    struct GsrFixture_ {
        const Date_ today_{2026, 10, 21}, expiry_{2027, 10, 21}, maturity_{2028, 10, 21};
        Handle_<ModelData_> model_;
        Handle_<CalibrationInstrument_> instrument_;
        Vector_<Handle_<CalibrationObservation_>> rows_;
        Vector_<GSRCalibrationParameter_> parameters_{{0, 0, .001, .1}};
        GSRInstrumentCalibrationSettings_ settings_;

        GsrFixture_() {
            InitGlobalData();
            Handle_<GSRCurveData_> curve(new GSRCurveData_("curve", today_, "USD", {today_, maturity_}, {0., -.08}, {}, {}));
            Handle_<MultiFactorGSRVolData_> vol(
                new MultiFactorGSRVolData_("vol", {"level"}, {today_}, Matrix_<>(1, 1, .02), {today_}, Matrix_<>(1, 1, 1.), Matrix_<>(1, 1, 1.)));
            model_.reset(new MultiFactorGSRModelData_("model", curve, vol));
            instrument_ = NewBondOptionScriptInstrument({expiry_, maturity_, .96, OptionType_("Call")}, {"bond", Ccy_("USD"), "USD.OIS", 100.});
            settings_.valuation_.evaluationDate_ = today_;
            settings_.valuation_.fixings_.reset(new MarketFixingSnapshot_());
            const double target = 100. * PriceGSREuropeanOption(model_, BondOption_{expiry_, maturity_, .96, OptionType_("Call")}).price_;
            rows_ = {NewCalibrationObservation("obs", instrument_, "Price",
                                               NewCalibrationMarketQuote("quote", target, CalibrationValueUnit_::Parse("Price:USD")), .1)};
        }
        CalibrationPreparationContext_ Context() const {
            return {settings_.valuation_, Ccy_("USD"), "USD.OIS", handle_cast<MultiFactorGSRModelData_>(model_)->curve_, model_};
        }
        void Mc() {
            settings_.evaluation_.evaluation_ = CalibrationEvaluation_("ScriptMC");
            settings_.evaluation_.allowMonteCarloCalibration_ = true;
            settings_.fitSampling_ = CalibrationMonteCarloSampling_{"PseudoRandom", 16, 2, 1024, "dal.mrg32-normal-paths/1"};
            settings_.validationSampling_ = CalibrationMonteCarloSampling_{"PseudoRandom", 32, 2, 1024, "dal.mrg32-normal-paths/1"};
            settings_.maxPathsPerView_ = 96;
            settings_.solver_.maxIterations_ = 1;
        }
    };
    void InputError(const GsrFixture_& f, const char* code, const char* field) {
        try {
            static_cast<void>(CalibrateGSRInstruments(f.model_, f.rows_, f.parameters_, f.settings_));
            FAIL() << "Expected owning calibration error";
        } catch (const CalibrationError_& error) {
            ASSERT_EQ(error.Failure().diagnostic_.code_, code);
            ASSERT_EQ(error.Failure().diagnostic_.field_, field);
            ASSERT_TRUE(error.Failure().report_);
        }
    }
} // namespace

TEST(PublicGsrInstrumentTest, TestExplicitValuationAndSamplingCannotBeOmitted) {
    GsrFixture_ f;
    f.settings_.valuation_.fixings_.reset();
    InputError(f, "InvalidCalibrationPlan", "/settings/valuation/fixings");
    f.settings_.valuation_.fixings_.reset(new MarketFixingSnapshot_());
    f.Mc();
    f.settings_.fitSampling_.reset();
    InputError(f, "InvalidCalibrationPlan", "/settings/fit_sampling");
}

TEST(PublicGsrInstrumentTest, TestFastMatchesExistingDriverAndOwnsRawAxes) {
    const GsrFixture_ f;
    const auto native = CalibrateGaussianBondOptions(f.Context(), f.rows_, f.parameters_, f.settings_.solver_);
    const auto result = CalibrateGSRInstruments(f.model_, f.rows_, f.parameters_, f.settings_);
    ASSERT_EQ(result.modelPrices_, native.modelPrices_);
    ASSERT_EQ(result.parameters_, native.parameters_);
    ASSERT_EQ(result.converged_, native.converged_);
    ASSERT_EQ(result.fastCosts_.fastPrices_, native.costs_.fastPrices_);
    ASSERT_EQ(result.costs_.paths_, 0u);
    ASSERT_EQ(result.parameterLabels_, (Vector_<String_>{"g:level:2026-10-21"}));
    const auto& data = result.report_->Data();
    ASSERT_EQ(data.observations_[0].instrument_->BindingIdentity(), f.instrument_->BindingIdentity());
    ASSERT_EQ(data.observations_[0].derivativeMethod_, "FiniteDifference");
    ASSERT_EQ(data.observations_[0].backend_, "FastGaussian");
    ASSERT_EQ(data.observations_[0].modelValue_, native.modelPrices_[0]);
    ASSERT_TRUE(data.quoteJacobian_);
    ASSERT_DOUBLE_EQ((*data.quoteJacobian_)(0, 0), native.quoteJacobian_(0, 0));
    ASSERT_DOUBLE_EQ(result.report_->Axes().quoteMap_(0, 0), 1.);
    ASSERT_FALSE(result.report_->QuoteRiskAvailable());
    rapidjson::Document json;
    json.Parse(DumpCalibrationReportJson(result.report_).c_str());
    ASSERT_FALSE(json.HasParseError());
    ASSERT_TRUE(json["execution"]["sampling"].IsNull());
    ASSERT_TRUE(json["observations"][0]["standard_error"].IsNull());
}

TEST(PublicGsrInstrumentTest, TestMcMapsCountsAndHeldOutWithoutAnotherEvaluation) {
    GsrFixture_ f;
    f.Mc();
    f.settings_.heldOutSampling_ = CalibrationMonteCarloSampling_{"PseudoRandom", 8, 2, 1024, "dal.mrg32-normal-paths/1"};
    f.settings_.heldOutObservations_ = f.rows_;
    f.settings_.maxPathsPerView_ = 112;
    GSRScriptCalibrationSettings_ core;
    core.optimizer_ = f.settings_.solver_;
    auto& mc = core.sampling_;
    mc.allowMonteCarloCalibration_ = true;
    mc.fitBatches_ = mc.validationBatches_ = mc.heldOutBatches_ = 2;
    mc.fitPathsPerBatch_ = 16;
    mc.validationPathsPerBatch_ = 32;
    mc.heldOutPathsPerBatch_ = 8;
    mc.maxPathsPerView_ = 112;
    const auto native = CalibrateGSRScriptPrices(f.Context(), f.rows_, f.parameters_, core, f.rows_);
    const auto result = CalibrateGSRInstruments(f.model_, f.rows_, f.parameters_, f.settings_);
    ASSERT_EQ(result.parameters_, native.parameters_);
    ASSERT_EQ(result.modelPrices_, native.modelPrices_);
    ASSERT_EQ(result.costs_.paths_, native.costs_.paths_);
    ASSERT_EQ(result.costs_.basketEvaluations_, native.costs_.basketEvaluations_);
    ASSERT_EQ(result.heldOutRequested_, native.heldOutRequested_);
    ASSERT_EQ(result.heldOutPassed_, native.heldOutPassed_);
    ASSERT_DOUBLE_EQ(result.quoteJacobian_(0, 0), native.quoteJacobian_(0, 0));
    const auto& data = result.report_->Data();
    ASSERT_EQ(data.execution_.sampling_->firstPaths_, (Vector_<>{0., 16.}));
    ASSERT_EQ(data.execution_.validationSampling_->firstPaths_, (Vector_<>{32., 64.}));
    ASSERT_EQ(data.validation_.size(), 2u);
    ASSERT_EQ(data.validation_[1].edge_, "HeldOut");
    ASSERT_DOUBLE_EQ(*data.validation_[1].value_, native.heldOut_[0].mean_);
    ASSERT_EQ(data.observations_.size(), 1u);
    ASSERT_DOUBLE_EQ(*data.observations_[0].standardError_, native.fit_[0].standardError_);
    ASSERT_DOUBLE_EQ(*data.validation_[0].standardError_, native.validation_[0].standardError_);
    ASSERT_DOUBLE_EQ(*data.validation_[0].difference_, native.validation_[0].mean_ - native.fit_[0].mean_);
    ASSERT_DOUBLE_EQ(*data.validation_[0].absoluteTolerance_, f.settings_.solver_.numericalErrorFraction_ * f.rows_[0]->ErrorScale());
}

TEST(PublicGsrInstrumentTest, TestSamplingConflictsAndCheckedBudgetEndpoints) {
    GsrFixture_ f;
    f.Mc();
    f.settings_.maxPathsPerView_ = 95;
    InputError(f, "CalibrationBudgetExceeded", "/settings/max_paths_per_view");
    f.settings_.maxPathsPerView_ = 96;
    f.settings_.validationSampling_->pathsPerBatch_ = 16;
    InputError(f, "CalibrationBudgetExceeded", "/settings/validation_sampling");
    f.settings_.validationSampling_->pathsPerBatch_ = 32;
    f.settings_.validationSampling_->seed_ = 1025;
    InputError(f, "InvalidCalibrationPlan", "/settings/validation_sampling/seed");
    f.settings_.validationSampling_->seed_ = 1024;
    f.settings_.evaluation_.sampling_ = CalibrationSamplingSpec_{31, 1024, "mrg32"};
    InputError(f, "InvalidCalibrationPlan", "/settings/evaluation/sampling/paths");
    f.settings_.evaluation_.sampling_->paths_ = 32;
    f.settings_.evaluation_.sampling_->generator_ = "irn";
    InputError(f, "InvalidCalibrationPlan", "/settings/evaluation/sampling/generator");
    f.settings_.evaluation_.sampling_.reset();
    f.settings_.heldOutSampling_ = f.settings_.fitSampling_;
    InputError(f, "InvalidCalibrationPlan", "/settings/held_out_observations");
    f.settings_.heldOutSampling_.reset();
    f.settings_.fitSampling_->batches_ = f.settings_.fitSampling_->pathsPerBatch_ = INT_MAX;
    f.settings_.validationSampling_->batches_ = f.settings_.validationSampling_->pathsPerBatch_ = INT_MAX;
    InputError(f, "CalibrationBudgetExceeded", "/settings/validation_sampling");
}

TEST(PublicGsrInstrumentTest, TestFastDoesNotSilentlyIgnoreSamplingAndMethods) {
    GsrFixture_ f;
    f.settings_.fitSampling_ = CalibrationMonteCarloSampling_{"PseudoRandom", 16, 2, 1024, "dal.mrg32-normal-paths/1"};
    InputError(f, "InvalidCalibrationPlan", "/settings/fit_sampling");
    f.settings_.fitSampling_.reset();
    f.settings_.evaluation_.derivativeMethod_ = CalibrationDerivative_("AAD");
    InputError(f, "DerivativeUnavailable", "/settings/evaluation/derivative_method");
    f.settings_.evaluation_.derivativeMethod_ = CalibrationDerivative_("None");
    InputError(f, "DerivativeUnavailable", "/settings/evaluation/derivative_method");
    f.settings_.evaluation_.derivativeMethod_ = CalibrationDerivative_("Auto");
    f.settings_.maxBasketEvaluations_ = 2;
    InputError(f, "InvalidCalibrationPlan", "/settings/max_basket_evaluations");
    f.Mc();
    f.settings_.solver_.pricing_.quadratureOrder_ = 32;
    InputError(f, "InvalidCalibrationPlan", "/settings/solver/pricing");
}

TEST(PublicGsrInstrumentTest, TestFailureReportAndLastCompleteModelOutliveInputs) {
    std::shared_ptr<const CalibrationFailure_> owned;
    {
        GsrFixture_ f;
        f.Mc();
        f.settings_.maxBasketEvaluations_ = 1;
        try {
            static_cast<void>(CalibrateGSRInstruments(f.model_, f.rows_, f.parameters_, f.settings_));
            FAIL() << "Expected exact basket budget failure";
        } catch (const CalibrationError_& error) {
            owned = std::make_shared<const CalibrationFailure_>(error.Failure());
        }
    }
    ASSERT_TRUE(owned);
    ASSERT_EQ(owned->diagnostic_.code_, "CalibrationBudgetExceeded");
    ASSERT_EQ(owned->diagnostic_.field_, "/solver/basket_evaluations");
    ASSERT_EQ(owned->diagnostic_.stage_, CalibrationStage_::Value_::Solve);
    ASSERT_TRUE(handle_cast<MultiFactorGSRModelData_>(owned->candidate_));
    ASSERT_EQ(owned->report_->Data().summary_.evaluations_, 1);
    ASSERT_EQ(owned->report_->Data().summary_.status_, "Failed");
    ASSERT_EQ(owned->report_->Data().observations_[0].observationId_, "obs");
    ASSERT_FALSE(owned->report_->Data().summary_.objective_);
    ASSERT_FALSE(owned->report_->Data().quoteJacobian_);
    ASSERT_NE(DumpCalibrationReportJson(owned->report_).find("CalibrationBudgetExceeded"), String_::npos);
}

TEST(PublicGsrInstrumentTest, TestFastAdmissionPreservesFailingObservationAndQuote) {
    GsrFixture_ f;
    f.settings_.valuation_.evaluationDate_ = f.expiry_;
    try {
        static_cast<void>(CalibrateGSRInstruments(f.model_, f.rows_, f.parameters_, f.settings_));
        FAIL() << "Expected context mismatch";
    } catch (const CalibrationError_& error) {
        ASSERT_EQ(error.Failure().diagnostic_.observationId_, "obs");
        ASSERT_EQ(error.Failure().diagnostic_.quoteId_, "quote");
        ASSERT_EQ(error.Failure().report_->Axes().quoteMap_.Rows(), 1);
    }
}

TEST(PublicGsrInstrumentTest, TestMcFiniteDifferenceKeepsItsValuePlanAdmission) {
    GsrFixture_ f;
    f.Mc();
    f.settings_.evaluation_.derivativeMethod_ = CalibrationDerivative_("FiniteDifference");
    const auto r = CalibrateGSRInstruments(f.model_, f.rows_, f.parameters_, f.settings_);
    ASSERT_EQ(r.derivativeMethod_, CalibrationDerivative_::Value_::FiniteDifference);
    ASSERT_TRUE(r.fit_[0].gradient_.empty());
    ASSERT_TRUE(r.nativeJacobianAvailable_);
    ASSERT_EQ(r.report_->Data().observations_[0].derivativeMethod_, "FiniteDifference");
    ASSERT_EQ(r.report_->Data().plans_[0].ruleId_, "dal.gaussian-bond-option/1");
}

TEST(PublicGsrInstrumentTest, TestAutoFallbackAdmitsGeneralPriceOnlyWithExplicitCompleteMc) {
    GsrFixture_ f;
    ScriptInstrumentDefinitionSpec_ spec;
    const auto price = CalibrationValueUnit_::Parse("Price:USD");
    spec.header_.payCurrency_ = spec.header_.resultCurrency_ = spec.header_.collateralCurrency_ = Ccy_("USD");
    spec.header_.discountRoute_ = "USD.OIS";
    spec.source_.rows_ = {{ScriptSourceRowKind_::Value_::Definition, "N", "100"},
                          {ScriptSourceRowKind_::Value_::Definition, "K", ".04"},
                          {ScriptSourceRowKind_::Value_::Event, "2027-10-21", "avg=.5*FIX(IR[USD,LIBOR_6M_LCH,2027-10-21])"},
                          {ScriptSourceRowKind_::Value_::Event, "2028-04-21", "avg=avg+.5*FIX(IR[USD,LIBOR_6M_LCH,2028-04-21])"},
                          {ScriptSourceRowKind_::Value_::Event, "2028-10-21", "pay PAYS N*MAX(avg-K,0)"}};
    spec.parameters_ = {
        {"N", ScriptParameterType_::Value_::Real, price, ScriptParameterRole_::Value_::Notional, true, 0., {}, false, true},
        {"K", ScriptParameterType_::Value_::Real, CalibrationValueUnit_::Parse("Rate"), ScriptParameterRole_::Value_::RateStrike, true}};
    spec.measures_ = {{"Price", CalibrationMeasureKind_::Value_::Price, "PAY", {}, price}};
    const auto instrument = NewScriptCalibrationInstrument({"average", NewScriptInstrumentDefinition(spec)});
    f.rows_ = {NewCalibrationObservation("average-row", instrument, "Price", NewCalibrationMarketQuote("average-quote", .4, price), 1.)};
    f.Mc();
    f.settings_.evaluation_.evaluation_ = CalibrationEvaluation_("AutoFast");
    const auto result = CalibrateGSRInstruments(f.model_, f.rows_, f.parameters_, f.settings_);
    ASSERT_EQ(result.backend_, "ScriptMC");
    ASSERT_EQ(result.report_->Data().plans_[0].ruleId_, "dal.gaussian-script-price/1");
    ASSERT_EQ(result.report_->Data().observations_[0].instrument_->BindingIdentity(), instrument->BindingIdentity());
    f.settings_.evaluation_.allowMonteCarloCalibration_ = false;
    ASSERT_THROW(CalibrateGSRInstruments(f.model_, f.rows_, f.parameters_, f.settings_), CalibrationError_);
}

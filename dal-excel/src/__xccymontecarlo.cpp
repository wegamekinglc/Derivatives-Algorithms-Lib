#include "__xccymontecarlo.hpp"
#include "__platform.hpp"
#include "__script_test_api.hpp"
#include "__xccyinput.hpp"

/*IF--------------------------------------------------------------------------
public XccyInstrument_ValueByGaussian
    Evaluate the same immutable XCCY source with Gaussian moments
&inputs
instrument is handle CalibrationInstrument
    Shared immutable instrument
model is handle XccyGaussianModelData
    Explicit Gaussian model
measureId is string
    Price or BasisSpread
&optional
fixings is handle StorableMarketFixingSnapshot
    Omitted captures history; explicit empty remains authoritative
&outputs
outcome is handle StorableCalibrationOutcome
    Owning mean, report and validated model
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public XccyGaussianMarketSpec_New
    XccyGaussianMarketSpec New
&inputs
settings is cell[][]+ or Xccy->Excel::XccyTableInput_("XccyGaussianMarketSpec_New", "settings", 2)
    Typed settings
domestic is handle StorableCurveBlock
    Typed domestic
foreign is handle StorableCurveBlock
    Typed foreign
basis is handle StorableDiscountCurve
    Typed basis
&outputs
value is handle StorableXccyGaussianMarketSpec
    Typed value
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public XccyGaussianDynamicsSpec_New
    XccyGaussianDynamicsSpec New
&inputs
settings is cell[][]+ or Xccy->Excel::XccyTableInput_("XccyGaussianDynamicsSpec_New", "settings", 2)
    Typed settings
buckets is cell[][]+ or Xccy->Excel::XccyTableInput_("XccyGaussianDynamicsSpec_New", "buckets", 4)
    Typed buckets
correlation is cell[][]+ or Xccy->Excel::XccyTableInput_("XccyGaussianDynamicsSpec_New", "correlation", 4)
    Typed correlation
&outputs
value is handle StorableXccyGaussianDynamicsSpec
    Typed value
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public XccyGaussianModelSpec_New
    XccyGaussianModelSpec New
&inputs
market is handle StorableXccyGaussianMarketSpec
    Typed market
dynamics is handle StorableXccyGaussianDynamicsSpec
    Typed dynamics
&outputs
value is handle StorableXccyGaussianModelSpec
    Typed value
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public XccyGaussianDynamics_New
    XccyGaussianDynamics New
&inputs
name is string
    Typed name
spec is handle StorableXccyGaussianDynamicsSpec
    Typed spec
&outputs
value is handle XccyGaussianDynamics
    Typed value
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public XccyGaussianModel_New
    XccyGaussianModel New
&inputs
name is string
    Typed name
spec is handle StorableXccyGaussianModelSpec
    Typed spec
&outputs
value is handle XccyGaussianModelData
    Typed value
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public XccyParameterLabels_New
    XccyParameterLabels New
&inputs
labels is string[]
    Typed labels
+argName = "labels"; Excel::ValidateXccyLabels(xl_labels);
&outputs
value is handle StorableXccyParameterLabels
    Typed value
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public XccyMonteCarloRequest_New
    XccyMonteCarloRequest New
&inputs
measureId is string
    Typed measureId
+argName = "phase"; Excel::ValidateXccyPhaseInput(xl_phase);
phase is cell
    Typed phase
sampling is handle StorableCalibrationMonteCarloSampling
    Typed sampling
&optional
settings is cell[][]+ or Xccy->Excel::XccyTableInput_("XccyMonteCarloRequest_New", "settings", 2)
    Typed settings
&outputs
value is handle StorableXccyMonteCarloRequest
    Typed value
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public XccyInstrument_ValueByMonteCarlo
    XccyInstrument ValueByMonteCarlo
&inputs
instrument is handle CalibrationInstrument
    Typed instrument
model is handle XccyGaussianModelData
    Typed model
request is handle StorableXccyMonteCarloRequest
    Typed request
&outputs
outcome is handle StorableXccyMonteCarloOutcome
    Typed outcome
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public XccyMonteCarloOutcome_Get_Json
    XccyMonteCarloOutcome Get Json
&inputs
outcome is handle StorableXccyMonteCarloOutcome
    Typed outcome
&outputs
json is string[]
    Typed json
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public XccyMonteCarloOutcome_Get_Status
    XccyMonteCarloOutcome Get Status
&inputs
outcome is handle StorableXccyMonteCarloOutcome
    Typed outcome
&outputs
status is string
    Typed status
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public XccyMonteCarloOutcome_Get_Report
    XccyMonteCarloOutcome Get Report
&inputs
outcome is handle StorableXccyMonteCarloOutcome
    Typed outcome
&outputs
report is handle CalibrationReport
    Typed report
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public XccyMonteCarloOutcome_Get_Mean
    XccyMonteCarloOutcome Get Mean
&inputs
outcome is handle StorableXccyMonteCarloOutcome
    Typed outcome
&outputs
mean is number
    Typed mean
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public XccyMonteCarloOutcome_Get_StandardError
    XccyMonteCarloOutcome Get StandardError
&inputs
outcome is handle StorableXccyMonteCarloOutcome
    Typed outcome
&outputs
error is number
    Typed error
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public XccyMonteCarloOutcome_Get_Partials
    XccyMonteCarloOutcome Get Partials
&inputs
outcome is handle StorableXccyMonteCarloOutcome
    Typed outcome
&outputs
partials is cell[][]
    Typed partials
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public XccyMonteCarloOutcome_Get_Model
    XccyMonteCarloOutcome Get Model
&inputs
outcome is handle StorableXccyMonteCarloOutcome
    Typed outcome
&outputs
model is handle XccyGaussianModelData
    Typed model
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public XccyMonteCarloReference_New
    XccyMonteCarloReference New
&inputs
settings is cell[][]+ or Xccy->Excel::XccyTableInput_("XccyMonteCarloReference_New", "settings", 2)
    Typed settings
&outputs
value is handle StorableXccyMonteCarloReference
    Typed value
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public XccyMonteCarloPrecision_New
    XccyMonteCarloPrecision New
&inputs
settings is cell[][]+ or Xccy->Excel::XccyTableInput_("XccyMonteCarloPrecision_New", "settings", 2)
    Typed settings
&outputs
value is handle StorableXccyMonteCarloPrecision
    Typed value
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public XccyMonteCarloOutcome_Validate
    XccyMonteCarloOutcome Validate
&inputs
outcome is handle StorableXccyMonteCarloOutcome
    Typed outcome
reference is handle StorableXccyMonteCarloReference
    Typed reference
precision is handle StorableXccyMonteCarloPrecision
    Typed precision
&outputs
value is handle StorableXccyMonteCarloValidation
    Typed value
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public XccyMonteCarloValidation_Get_Flags
    XccyMonteCarloValidation Get Flags
&inputs
value is handle StorableXccyMonteCarloValidation
    Typed value
&outputs
flags is cell[][]
    Typed flags
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public XccyMonteCarloValidation_Get_Report
    XccyMonteCarloValidation Get Report
&inputs
value is handle StorableXccyMonteCarloValidation
    Typed value
&outputs
report is handle CalibrationReport
    Typed report
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CalibrationInstrument_Get_XccyGaussianCapabilities
    CalibrationInstrument Get XccyGaussianCapabilities
&inputs
instrument is handle CalibrationInstrument
    Typed instrument
model is handle XccyGaussianModelData
    Typed model
request is handle StorableXccyMonteCarloRequest
    Typed request
&outputs
report is handle CalibrationReport
    Typed report
-IF-------------------------------------------------------------------------*/

namespace Dal {
    namespace {
        using namespace Excel::Calibration;
        template <class T_> const T_& Value(const Handle_<StorableCalibrationValue_<T_>>& input) {
            REQUIRE(input, "InvalidCalibrationInput: nonnull typed XCCY session handle");
            return input->val_;
        }
        const XccyMonteCarloResult_& Result(const Handle_<StorableXccyMonteCarloOutcome_>& outcome) {
            const auto& value = Value(outcome);
            if (!value.result_)
                CalibrationInputFailure("CalibrationCandidateUnavailable", "/outcome/result", "completed XCCY MC estimate");
            return *value.result_;
        }
        template <class T_> T_ TableValue(const Matrix_<Cell_>& cells, int row, int col, const String_& function, const String_& argument) {
            try {
                return Input_<T_>::Read(cells(row, col));
            } catch (const Exception_& error) {
                const String_ field = row > 0 && Cell::IsString(cells(0, col)) ? Cell::ToString(cells(0, col)) : String_("column_header");
                THROW(Excel::ScriptSettingLocation(function, argument, row + 1, col + 1) + "field=" + field + "; " + error.what());
            }
        }
        std::map<String_, int> BucketColumns(const Matrix_<Cell_>& cells, const String_& function) {
            const std::set<String_> allowed{"end_date", "sigma_domestic", "sigma_foreign", "sigma_fx"};
            REQUIRE(cells.Rows() >= 2 && cells.Cols() == 4,
                    Excel::ScriptSettingLocation(function, "buckets", 1, 1) + "expected four named columns and data");
            std::map<String_, int> columns;
            for (int col = 0; col < 4; ++col) {
                const auto key = TableValue<String_>(cells, 0, col, function, "buckets");
                REQUIRE(allowed.count(key) && columns.emplace(key, col).second,
                        Excel::ScriptSettingLocation(function, "buckets", 1, col + 1) + "unknown or normalized duplicate column");
            }
            return columns;
        }
    } // namespace
    void XccyGaussianMarketSpec_New(const Matrix_<Cell_>& settings,
                                    const Handle_<StorableCurveBlock_>& domestic,
                                    const Handle_<StorableCurveBlock_>& foreign,
                                    const Handle_<StorableDiscountCurve_>& basis,
                                    Handle_<StorableXccyGaussianMarketSpec_>* value) {
        XccyFields_ f(settings, "XccyGaussianMarketSpec_New", {"valuation_time", "pair", "collateral_currency", "fx_spot", "discount_route"}, true);
        XccyGaussianMarketSpec_ v;
        v.valuationTime_ = f.Required<DateTime_>("valuation_time");
        v.pair_ = f.Required<Handle_<StorableCurrencyPair_>>("pair")->val_;
        v.collateralCurrency_ = f.Required<Ccy_>("collateral_currency");
        v.fxSpot_ = f.Required<double>("fx_spot");
        v.discountRoute_ = f.Required<String_>("discount_route");
        REQUIRE(domestic && foreign && basis, "XccyGaussianMarketSpec_New: required domestic, foreign and basis typed curve handles");
        v.domesticBlock_ = domestic->val_;
        v.foreignBlock_ = foreign->val_;
        v.basis_ = basis->val_;
        f.Finish();
        value->reset(new StorableXccyGaussianMarketSpec_(v));
    }
    void XccyGaussianDynamicsSpec_New(const Matrix_<Cell_>& settings,
                                      const Matrix_<Cell_>& buckets,
                                      const Matrix_<Cell_>& correlation,
                                      Handle_<StorableXccyGaussianDynamicsSpec_>* value) {
        const String_ function = "XccyGaussianDynamicsSpec_New";
        XccyFields_ f(settings, function, {"domestic_a", "foreign_a"}, true);
        XccyGaussianDynamicsSpec_ v;
        v.domesticA_ = f.Required<double>("domestic_a");
        v.foreignA_ = f.Required<double>("foreign_a");
        f.Finish();
        const auto columns = BucketColumns(buckets, function);
        for (int row = 1; row < buckets.Rows(); ++row) {
            v.bucketEndDates_.push_back(TableValue<Date_>(buckets, row, columns.at("end_date"), function, "buckets"));
            v.domesticSigma_.push_back(TableValue<double>(buckets, row, columns.at("sigma_domestic"), function, "buckets"));
            v.foreignSigma_.push_back(TableValue<double>(buckets, row, columns.at("sigma_foreign"), function, "buckets"));
            v.fxSigma_.push_back(TableValue<double>(buckets, row, columns.at("sigma_fx"), function, "buckets"));
        }
        REQUIRE(correlation.Rows() == 4 && correlation.Cols() == 4,
                Excel::ScriptSettingLocation(function, "correlation", 1, 1) + "expected named 3x3 correlation with row and column headers");
        REQUIRE(TableValue<String_>(correlation, 0, 0, function, "correlation") == "factor",
                Excel::ScriptSettingLocation(function, "correlation", 1, 1) + "expected factor header");
        std::set<String_> names;
        v.correlation_ = Matrix_<>(3, 3);
        for (int i = 0; i < 3; ++i) {
            const auto name = TableValue<String_>(correlation, 0, i + 1, function, "correlation");
            REQUIRE(names.insert(name).second, Excel::ScriptSettingLocation(function, "correlation", 1, i + 2) + "normalized duplicate factor");
            REQUIRE(Bytes(TableValue<String_>(correlation, i + 1, 0, function, "correlation")) == Bytes(name),
                    Excel::ScriptSettingLocation(function, "correlation", i + 2, 1) + "row factor must match named column");
            v.factorNames_.push_back(name);
            for (int j = 0; j < 3; ++j)
                v.correlation_(i, j) = TableValue<double>(correlation, i + 1, j + 1, function, "correlation");
        }
        value->reset(new StorableXccyGaussianDynamicsSpec_(v));
    }
    void XccyGaussianModelSpec_New(const Handle_<StorableXccyGaussianMarketSpec_>& market,
                                   const Handle_<StorableXccyGaussianDynamicsSpec_>& dynamics,
                                   Handle_<StorableXccyGaussianModelSpec_>* value) {
        value->reset(new StorableXccyGaussianModelSpec_({Value(market), Value(dynamics)}));
    }
    void
    XccyGaussianDynamics_New(const String_& name, const Handle_<StorableXccyGaussianDynamicsSpec_>& spec, Handle_<XccyGaussianDynamics_>* value) {
        value->reset(new XccyGaussianDynamics_(name, Value(spec)));
    }
    void XccyGaussianModel_New(const String_& name, const Handle_<StorableXccyGaussianModelSpec_>& spec, Handle_<XccyGaussianModelData_>* value) {
        *value = NewXccyGaussianModel(name, Value(spec));
    }
    void XccyParameterLabels_New(const Vector_<String_>& labels, Handle_<StorableXccyParameterLabels_>* value) {
        value->reset(new StorableXccyParameterLabels_(labels));
    }
    void XccyMonteCarloRequest_New(const String_& measureId,
                                   const Cell_& phase,
                                   const Handle_<StorableCalibrationMonteCarloSampling_>& sampling,
                                   const Matrix_<Cell_>& settings,
                                   Handle_<StorableXccyMonteCarloRequest_>* value) {
        XccyFields_ f(settings, "XccyMonteCarloRequest_New", {"fixings", "simulation", "derivative_method", "parameter_labels"}, true);
        XccyMonteCarloRequest_ v;
        v.measureId_ = measureId;
        try {
            v.phase_ = ParseXccyMonteCarloPhase(Cell::IsEmpty(phase) ? String_() : Input_<String_>::Read(phase));
        } catch (const CalibrationError_& error) {
            auto failure = std::make_shared<CalibrationFailure_>(error.Failure());
            failure->diagnostic_.message_ = "XccyMonteCarloRequest_New; phase; " + failure->diagnostic_.message_;
            failure->report_.reset(new CalibrationReport_(CalibrationStage_::Value_::Input, {}, {failure->diagnostic_}));
            throw CalibrationError_(__FILE__, __LINE__, __func__, failure);
        }
        v.sampling_ = Value(sampling);
        const auto fixings = f.Maybe<Handle_<StorableMarketFixingSnapshot_>>("fixings");
        if (fixings)
            v.fixings_ = (*fixings)->val_;
        v.simulation_ = f.Default<CalibrationSimulationSettings_>("simulation", v.simulation_);
        v.derivativeMethod_ = f.Default<CalibrationDerivative_>("derivative_method", v.derivativeMethod_);
        v.parameterLabels_ = f.Default<Vector_<String_>>("parameter_labels", {});
        f.Finish();
        value->reset(new StorableXccyMonteCarloRequest_(v));
    }
    void XccyInstrument_ValueByMonteCarlo(const Handle_<CalibrationInstrument_>& instrument,
                                          const Handle_<XccyGaussianModelData_>& model,
                                          const Handle_<StorableXccyMonteCarloRequest_>& request,
                                          Handle_<StorableXccyMonteCarloOutcome_>* outcome) {
        const auto input = Value(request);
        XccyMonteCarloOutcome_ result;
        try {
            result.result_ = ValueXccyInstrumentByMonteCarlo(instrument, model, input);
            result.status_ = "Estimate";
            result.report_ = result.result_->report_;
            result.candidate_ = handle_cast<Storable_>(model);
        } catch (const CalibrationError_& error) {
            result.status_ = "Failed";
            result.report_ = error.Failure().report_;
            result.candidate_ = error.Failure().candidate_;
        }
        outcome->reset(new StorableXccyMonteCarloOutcome_(result));
    }
    void XccyInstrument_ValueByGaussian(const Handle_<CalibrationInstrument_>& instrument,
                                        const Handle_<XccyGaussianModelData_>& model,
                                        const String_& measureId,
                                        const Handle_<StorableMarketFixingSnapshot_>& fixings,
                                        Handle_<StorableCalibrationOutcome_>* outcome) {
        try {
            const auto value =
                ValueXccyInstrumentByGaussian(instrument, model, measureId, fixings ? fixings->val_ : Handle_<MarketFixingSnapshot_>());
            outcome->reset(new StorableCalibrationOutcome_("Succeeded", value.report_, {}, value.value_, handle_cast<ModelData_>(model)));
        } catch (const CalibrationError_& error) {
            outcome->reset(
                new StorableCalibrationOutcome_("Failed", error.Failure().report_, {}, {}, handle_cast<ModelData_>(error.Failure().candidate_)));
        }
    }
    void XccyMonteCarloOutcome_Get_Json(const Handle_<StorableXccyMonteCarloOutcome_>& outcome, Vector_<String_>* json) {
        *json = Excel::ScriptDiagnosticChunks(DumpXccyMonteCarloResultJson(Result(outcome)), "XccyMonteCarloOutcome_Get_Json");
    }
    void XccyMonteCarloOutcome_Get_Status(const Handle_<StorableXccyMonteCarloOutcome_>& outcome, String_* status) {
        *status = Value(outcome).status_;
    }
    void XccyMonteCarloOutcome_Get_Report(const Handle_<StorableXccyMonteCarloOutcome_>& outcome, Handle_<CalibrationReport_>* report) {
        *report = Value(outcome).report_;
    }
    void XccyMonteCarloOutcome_Get_Mean(const Handle_<StorableXccyMonteCarloOutcome_>& outcome, double* mean) { *mean = Result(outcome).mean_; }
    void XccyMonteCarloOutcome_Get_StandardError(const Handle_<StorableXccyMonteCarloOutcome_>& outcome, double* error) {
        *error = *Result(outcome).standardError_;
    }
    void XccyMonteCarloOutcome_Get_Model(const Handle_<StorableXccyMonteCarloOutcome_>& outcome, Handle_<XccyGaussianModelData_>* model) {
        *model = handle_cast<XccyGaussianModelData_>(Value(outcome).candidate_);
        if (!*model)
            CalibrationInputFailure("CalibrationCandidateUnavailable", "/outcome/model", "validated XCCY Gaussian model");
    }
    void XccyMonteCarloOutcome_Get_Partials(const Handle_<StorableXccyMonteCarloOutcome_>& outcome, Matrix_<Cell_>* partials) {
        const auto& estimate = XccyMonteCarloDetails(Result(outcome)).estimate_;
        if (estimate.gradient_.empty())
            CalibrationInputFailure("DerivativeUnavailable", "/outcome/partials", "explicit qualified native AAD labels");
        *partials = Matrix_<Cell_>(static_cast<int>(estimate.gradient_.size()) + 1, 3);
        (*partials)(0, 0) = "parameter_label";
        (*partials)(0, 1) = "native_partial";
        (*partials)(0, 2) = "standard_error";
        for (size_t i = 0; i < estimate.gradient_.size(); ++i) {
            (*partials)(static_cast<int>(i) + 1, 0) = estimate.parameterLabels_[i];
            (*partials)(static_cast<int>(i) + 1, 1) = estimate.gradient_[i];
            (*partials)(static_cast<int>(i) + 1, 2) = estimate.gradientStandardError_[i];
        }
    }
    void XccyMonteCarloReference_New(const Matrix_<Cell_>& settings, Handle_<StorableXccyMonteCarloReference_>* value) {
        XccyFields_ f(settings, "XccyMonteCarloReference_New",
                      {"value", "numerical_error", "standard_error", "gradient", "gradient_standard_error", "finite_difference_error"}, true);
        CalibrationMcReference_ v;
        v.value_ = f.Required<double>("value");
        v.numericalError_ = f.Required<double>("numerical_error");
        v.standardError_ = f.Default<double>("standard_error", 0.);
        v.gradient_ = f.Default<Vector_<>>("gradient", {});
        v.gradientStandardError_ = f.Default<Vector_<>>("gradient_standard_error", {});
        v.finiteDifferenceError_ = f.Default<Vector_<>>("finite_difference_error", {});
        f.Finish();
        value->reset(new StorableXccyMonteCarloReference_(v));
    }
    void XccyMonteCarloPrecision_New(const Matrix_<Cell_>& settings, Handle_<StorableXccyMonteCarloPrecision_>* value) {
        XccyFields_ f(
            settings, "XccyMonteCarloPrecision_New",
            {"error_scale", "sigma", "numerical_error_fraction", "epsilon_value", "gradient_error_fraction", "parameter_scales", "epsilon_gradient"},
            true);
        CalibrationMcPrecision_ v;
        v.errorScale_ = f.Required<double>("error_scale");
        v.sigma_ = f.Default<double>("sigma", v.sigma_);
        v.numericalErrorFraction_ = f.Default<double>("numerical_error_fraction", v.numericalErrorFraction_);
        v.epsilonValue_ = f.Default<double>("epsilon_value", v.epsilonValue_);
        v.gradientErrorFraction_ = f.Default<double>("gradient_error_fraction", v.gradientErrorFraction_);
        v.parameterScales_ = f.Default<Vector_<>>("parameter_scales", {});
        v.epsilonGradient_ = f.Default<Vector_<>>("epsilon_gradient", {});
        f.Finish();
        value->reset(new StorableXccyMonteCarloPrecision_(v));
    }
    void XccyMonteCarloOutcome_Validate(const Handle_<StorableXccyMonteCarloOutcome_>& outcome,
                                        const Handle_<StorableXccyMonteCarloReference_>& reference,
                                        const Handle_<StorableXccyMonteCarloPrecision_>& precision,
                                        Handle_<StorableXccyMonteCarloValidation_>* value) {
        value->reset(new StorableXccyMonteCarloValidation_(ValidateXccyMonteCarlo(Result(outcome), Value(reference), Value(precision))));
    }
    void XccyMonteCarloValidation_Get_Report(const Handle_<StorableXccyMonteCarloValidation_>& value, Handle_<CalibrationReport_>* report) {
        *report = Value(value).validation_.report_;
    }
    void XccyMonteCarloValidation_Get_Flags(const Handle_<StorableXccyMonteCarloValidation_>& value, Matrix_<Cell_>* flags) {
        const auto& result = Value(value);
        const auto& v = result.validation_;
        const Vector_<String_> keys{"schema",
                                    "parity_passed",
                                    "precision_passed",
                                    "value_error_passed",
                                    "gradient_parity_passed",
                                    "gradient_precision_passed",
                                    "gradient_combined_passed",
                                    "numerical_validation_passed",
                                    "budget_exhausted"};
        const Vector_<Cell_> values{Cell_("dal.xccy-gaussian-validation-flags/1"),
                                    Cell_(v.parityPassed_),
                                    Cell_(v.precisionPassed_),
                                    Cell_(v.valueErrorPassed_),
                                    v.gradientValidationRequested_ ? Cell_(v.gradientParityPassed_) : Cell_(),
                                    v.gradientValidationRequested_ ? Cell_(v.gradientPrecisionPassed_) : Cell_(),
                                    v.gradientValidationRequested_ ? Cell_(result.gradientCombinedPassed_) : Cell_(),
                                    Cell_(v.numericalValidationPassed_),
                                    Cell_(v.budgetExhausted_)};
        *flags = Matrix_<Cell_>(static_cast<int>(keys.size()) + 1, 2);
        (*flags)(0, 0) = "key";
        (*flags)(0, 1) = "value";
        for (size_t i = 0; i < keys.size(); ++i) {
            (*flags)(static_cast<int>(i) + 1, 0) = keys[i];
            (*flags)(static_cast<int>(i) + 1, 1) = values[i];
        }
    }
    void CalibrationInstrument_Get_XccyGaussianCapabilities(const Handle_<CalibrationInstrument_>& instrument,
                                                            const Handle_<XccyGaussianModelData_>& model,
                                                            const Handle_<StorableXccyMonteCarloRequest_>& request,
                                                            Handle_<CalibrationReport_>* report) {
        *report = XccyGaussianInstrumentCapabilities(instrument, model, Value(request));
    }
} // namespace Dal

#ifdef _WIN32
using namespace Dal;
#include <dal-excel/auto/MG_CalibrationInstrument_Get_XccyGaussianCapabilities_public.inc>
#include <dal-excel/auto/MG_XccyGaussianDynamicsSpec_New_public.inc>
#include <dal-excel/auto/MG_XccyGaussianDynamics_New_public.inc>
#include <dal-excel/auto/MG_XccyGaussianMarketSpec_New_public.inc>
#include <dal-excel/auto/MG_XccyGaussianModelSpec_New_public.inc>
#include <dal-excel/auto/MG_XccyGaussianModel_New_public.inc>
#include <dal-excel/auto/MG_XccyInstrument_ValueByGaussian_public.inc>
#include <dal-excel/auto/MG_XccyInstrument_ValueByMonteCarlo_public.inc>
#include <dal-excel/auto/MG_XccyMonteCarloOutcome_Get_Json_public.inc>
#include <dal-excel/auto/MG_XccyMonteCarloOutcome_Get_Mean_public.inc>
#include <dal-excel/auto/MG_XccyMonteCarloOutcome_Get_Model_public.inc>
#include <dal-excel/auto/MG_XccyMonteCarloOutcome_Get_Partials_public.inc>
#include <dal-excel/auto/MG_XccyMonteCarloOutcome_Get_Report_public.inc>
#include <dal-excel/auto/MG_XccyMonteCarloOutcome_Get_StandardError_public.inc>
#include <dal-excel/auto/MG_XccyMonteCarloOutcome_Get_Status_public.inc>
#include <dal-excel/auto/MG_XccyMonteCarloOutcome_Validate_public.inc>
#include <dal-excel/auto/MG_XccyMonteCarloPrecision_New_public.inc>
#include <dal-excel/auto/MG_XccyMonteCarloReference_New_public.inc>
#include <dal-excel/auto/MG_XccyMonteCarloRequest_New_public.inc>
#include <dal-excel/auto/MG_XccyMonteCarloValidation_Get_Flags_public.inc>
#include <dal-excel/auto/MG_XccyMonteCarloValidation_Get_Report_public.inc>
#include <dal-excel/auto/MG_XccyParameterLabels_New_public.inc>
#endif

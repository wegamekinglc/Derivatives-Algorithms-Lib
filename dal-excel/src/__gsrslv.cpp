//
// Created by Codex on 2026/10/2.
//

#include "__gsr_helpers.hpp"
#include "__gsrslv_test_api.hpp"

#include <set>

// clang-format off
/*IF--------------------------------------------------------------------------
public GSRSLV_EuropeanOptionPrices
    Price SLV European options on common antithetic paths
&inputs
model is handle ModelData
    GSRSLVModelData
options is handle[]
    European option handles
&optional
settings is cell[][]
    Key/value rows: paths (4096), seed (1729)
&outputs
result is cell[][]
    One row per option: price, pair standard error
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public Calibrate_GSRSLV
    Calibrate selected SLV parameters with independent validation
&inputs
initial is handle ModelData
    Initial GSRSLVModelData
quotes is handle[]
    Price quote handles
parameters is cell[][]
    Label, lower, upper, scale
&optional
settings is cell[][]
    Solver and Monte Carlo settings
heldOut is handle[]
    Optional held-out price quote handles
&outputs
result is handle StorableGSRSLVCalibrationResult
    Fitted model and diagnostics
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public GSRSLVCalibrationResult_Get
    Read an SLV calibration diagnostic
&inputs
result is handle StorableGSRSLVCalibrationResult
    Calibration result
attribute is string
    Diagnostic name
&outputs
value is cell[][]
    Scalar, column vector or quote Jacobian
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public GSRSLVCalibrationResult_Get_Model
    Extract a calibrated SLV model
&inputs
result is handle StorableGSRSLVCalibrationResult
    Calibration result
&outputs
model is handle ModelData
    Fitted GSRSLVModelData
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public GSRSLV_QuoteRisk
    Bump price quotes and recalibrate the regularized SLV objective
&inputs
initial is handle ModelData
    Original GSRSLVModelData
quotes is handle[]
    Price quote handles
parameters is cell[][]
    Label, lower, upper, scale
targets is handle[]
    European target option handles
&optional
settings is cell[][]
    Calibration settings
riskSettings is cell[][]
    relativeBump (0.01), absoluteBump (1e-6), stabilityTolerance (0.05)
curveRisk is handle StorableGSRCurveQuoteRisk
    Optional curve quote provenance bridge
&outputs
result is handle StorableGSRSLVQuoteRiskResult
    Quote sensitivities and stability diagnostics
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public GSRSLVQuoteRiskResult_Get
    Read a calibrated SLV quote risk diagnostic
&inputs
result is handle StorableGSRSLVQuoteRiskResult
    Quote risk result
attribute is string
    prices, quoteNames, quoteUnits, sensitivities, refinementErrors, stable, activeSetStable or curveRiskIncluded
&outputs
value is cell[][]
    Scalar, column vector or target-by-quote matrix
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public GSRSLVQuoteRiskResult_Get_Calibration
    Extract the base SLV calibration from quote risk
&inputs
result is handle StorableGSRSLVQuoteRiskResult
    Quote risk result
&outputs
calibration is handle StorableGSRSLVCalibrationResult
    Base calibration and validation diagnostics
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public GSRCurveQuoteRisk_New
    Connect a GSR curve snapshot to validated market quote provenance
&inputs
snapshot is handle GSRCurveData
    Curve snapshot matching the bound market
market is handle StorableRatePricingMarket
    Bound curve market
provenance is handle StorableRateQuoteRiskProvenance
    Available curve calibration provenance
discountComponent is string
    Discount component key
&optional
projectionComponents is string[]
    One component key per snapshot projection tenor
&outputs
result is handle StorableGSRCurveQuoteRisk
    Immutable curve quote bridge
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public GSRCurveQuoteRisk_Get
    Read a GSR curve quote bridge diagnostic
&inputs
result is handle StorableGSRCurveQuoteRisk
    Curve quote bridge
attribute is string
    quoteNames, quoteUnits or logDFQuoteJacobian
&outputs
value is cell[][]
    Column vector or snapshot-node-by-quote matrix
-IF-------------------------------------------------------------------------*/

// clang-format on

namespace Dal {
    namespace {
        using namespace GSRWorksheet;

        Dictionary_ SettingsDictionary(const Matrix_<Cell_>& cells) {
            REQUIRE(cells.Empty() || cells.Cols() == 2, "InvalidGSRWorksheet: settings must have two columns");
            Dictionary_ result;
            for (int row = 0; row < cells.Rows(); ++row)
                result.Insert(Cell::ToString(cells(row, 0)), cells(row, 1));
            return result;
        }

        GSRMonteCarloSettings_ Pricing(const Matrix_<Cell_>& cells) {
            GSRMonteCarloSettings_ settings;
            for (const auto& [key, value] : SettingsDictionary(cells)) {
                if (key == "paths")
                    settings.paths_ = Integer(Cell::ToDouble(value));
                else if (key == "seed")
                    settings.seed_ = Integer(Cell::ToDouble(value));
                else
                    THROW("InvalidGSRWorksheet: unknown pricing setting " + key);
            }
            return settings;
        }

        GSRSLVCalibrationSettings_ Settings(const Matrix_<Cell_>& cells) {
            GSRSLVCalibrationSettings_ settings;
            for (const auto& [key, value] : SettingsDictionary(cells)) {
                if (AssignSolver(&settings.solver_, key, value))
                    continue;
                if (key == "paths")
                    settings.pricing_.paths_ = Integer(Cell::ToDouble(value));
                else if (key == "seed")
                    settings.pricing_.seed_ = Integer(Cell::ToDouble(value));
                else if (key == "validationPaths")
                    settings.validation_.paths_ = Integer(Cell::ToDouble(value));
                else if (key == "validationSeed")
                    settings.validation_.seed_ = Integer(Cell::ToDouble(value));
                else if (key == "validationSigma")
                    settings.validationSigma_ = Cell::ToDouble(value);
                else if (key == "staged") {
                    REQUIRE(Cell::IsBool(value), "InvalidGSRWorksheet: staged must be boolean");
                    settings.staged_ = Cell::ToBool(value);
                } else
                    THROW("InvalidGSRWorksheet: unknown calibration setting " + key);
            }
            return settings;
        }

        GSRSLVQuoteRiskSettings_ RiskSettings(const Matrix_<Cell_>& cells) {
            GSRSLVQuoteRiskSettings_ settings;
            for (const auto& [key, value] : SettingsDictionary(cells)) {
                if (key == "relativeBump")
                    settings.relativeBump_ = Cell::ToDouble(value);
                else if (key == "absoluteBump")
                    settings.absoluteBump_ = Cell::ToDouble(value);
                else if (key == "stabilityTolerance")
                    settings.stabilityTolerance_ = Cell::ToDouble(value);
                else
                    THROW("InvalidGSRWorksheet: unknown quote risk setting " + key);
            }
            return settings;
        }

        Vector_<GSRSLVCalibrationParameter_> Parameters(const Matrix_<Cell_>& cells) {
            REQUIRE(cells.Cols() == 4, "InvalidGSRWorksheet: parameters must have label, lower, upper, scale columns");
            Vector_<GSRSLVCalibrationParameter_> result;
            for (int row = 0; row < cells.Rows(); ++row)
                result.push_back(
                    {Cell::ToString(cells(row, 0)), Cell::ToDouble(cells(row, 1)), Cell::ToDouble(cells(row, 2)), Cell::ToDouble(cells(row, 3))});
            return result;
        }

        Vector_<GSREuropeanOption_> Options(const Vector_<Handle_<Storable_>>& handles) {
            Vector_<GSREuropeanOption_> result;
            for (const auto& handle : handles) {
                const auto option = handle_cast<StorableGSREuropeanOption_>(handle);
                REQUIRE(option, "InvalidGSRWorksheet: European option handle required");
                result.push_back(option->option_);
            }
            return result;
        }
    } // namespace

    void GSRSLV_EuropeanOptionPrices(const Handle_<ModelData_>& model,
                                     const Vector_<Handle_<Storable_>>& options,
                                     const Matrix_<Cell_>& settings,
                                     Matrix_<Cell_>* result) {
        const auto prices = PriceGSRSLVEuropeanOptions(model, Options(options), Pricing(settings));
        *result = Matrix_<Cell_>(prices.size(), 2);
        for (size_t i = 0; i < prices.size(); ++i) {
            (*result)(i, 0) = Cell_(prices[i].price_);
            (*result)(i, 1) = Cell_(prices[i].standardError_);
        }
    }

    void Calibrate_GSRSLV(const Handle_<ModelData_>& initial,
                          const Vector_<Handle_<Storable_>>& quotes,
                          const Matrix_<Cell_>& parameters,
                          const Matrix_<Cell_>& settings,
                          const Vector_<Handle_<Storable_>>& heldOut,
                          Handle_<StorableGSRSLVCalibrationResult_>* result) {
        *result = Handle_<StorableGSRSLVCalibrationResult_>(new StorableGSRSLVCalibrationResult_(
            "GSRSLVCalibrationResult", CalibrateGSRSLV(initial, Values<GSRCalibrationQuote_>(quotes), Parameters(parameters), Settings(settings),
                                                       Values<GSRCalibrationQuote_>(heldOut))));
    }

    void GSRSLVCalibrationResult_Get(const Handle_<StorableGSRSLVCalibrationResult_>& result, const String_& attribute, Matrix_<Cell_>* value) {
        REQUIRE(result, "InvalidGSRWorksheet: calibration result required");
        const auto& r = result->value_;
        const std::map<String_, const Vector_<>*> vectors{
            {"modelPrices", &r.modelPrices_},           {"residuals", &r.residuals_},
            {"standardErrors", &r.standardErrors_},     {"parameters", &r.parameters_},
            {"validationPrices", &r.validationPrices_}, {"validationStandardErrors", &r.validationStandardErrors_},
            {"numericalErrors", &r.numericalErrors_},   {"heldOutPrices", &r.heldOutPrices_},
            {"heldOutResiduals", &r.heldOutResiduals_}, {"heldOutStandardErrors", &r.heldOutStandardErrors_}};
        const auto vector = vectors.find(attribute);
        if (vector != vectors.end()) {
            *value = Column(*vector->second);
            return;
        }
        if (attribute == "activeBounds") {
            *value = Column(r.activeBounds_);
            return;
        }
        if (attribute == "quoteJacobian") {
            *value = Cells(r.quoteJacobian_);
            return;
        }
        const std::map<String_, Cell_> scalars{{"converged", Cell_(r.converged_)},
                                               {"fitWithinTolerance", Cell_(r.fitWithinTolerance_)},
                                               {"numericalValidationPassed", Cell_(r.numericalValidationPassed_)},
                                               {"heldOutWithinTolerance", Cell_(r.heldOutWithinTolerance_)},
                                               {"iterations", Cell_(double(r.iterations_))},
                                               {"evaluations", Cell_(double(r.evaluations_))},
                                               {"objective", Cell_(r.objective_)},
                                               {"jacobianRank", Cell_(double(r.jacobianRank_))},
                                               {"jacobianConditionEstimate", Cell_(r.jacobianConditionEstimate_)},
                                               {"terminationReason", Cell_(r.terminationReason_)}};
        const auto scalar = scalars.find(attribute);
        REQUIRE(scalar != scalars.end(), "InvalidGSRWorksheet: unknown calibration attribute " + attribute);
        *value = Matrix_<Cell_>(1, 1, scalar->second);
    }

    void GSRSLVCalibrationResult_Get_Model(const Handle_<StorableGSRSLVCalibrationResult_>& result, Handle_<ModelData_>* model) {
        REQUIRE(result, "InvalidGSRWorksheet: calibration result required");
        *model = Handle_<ModelData_>(result->value_.model_);
    }

    void GSRSLV_QuoteRisk(const Handle_<ModelData_>& initial,
                          const Vector_<Handle_<Storable_>>& quotes,
                          const Matrix_<Cell_>& parameters,
                          const Vector_<Handle_<Storable_>>& targets,
                          const Matrix_<Cell_>& settings,
                          const Matrix_<Cell_>& riskSettings,
                          const Handle_<StorableGSRCurveQuoteRisk_>& curveRisk,
                          Handle_<StorableGSRSLVQuoteRiskResult_>* result) {
        *result = Handle_<StorableGSRSLVQuoteRiskResult_>(new StorableGSRSLVQuoteRiskResult_(
            "GSRSLVQuoteRiskResult", GSRSLVQuoteRisk(initial, Values<GSRCalibrationQuote_>(quotes), Parameters(parameters), Options(targets),
                                                     Settings(settings), RiskSettings(riskSettings), curveRisk ? &curveRisk->value_ : nullptr)));
    }

    void GSRSLVQuoteRiskResult_Get(const Handle_<StorableGSRSLVQuoteRiskResult_>& result, const String_& attribute, Matrix_<Cell_>* value) {
        REQUIRE(result, "InvalidGSRWorksheet: quote risk result required");
        const auto& r = result->value_;
        if (attribute == "sensitivities")
            *value = Cells(r.sensitivities_);
        else if (attribute == "refinementErrors")
            *value = Cells(r.refinementErrors_);
        else if (attribute == "prices")
            *value = Column(r.prices_);
        else if (attribute == "quoteNames")
            *value = Column(r.quoteNames_);
        else if (attribute == "quoteUnits")
            *value = Column(r.quoteUnits_);
        else if (attribute == "stable")
            *value = Column(r.stable_);
        else if (attribute == "activeSetStable")
            *value = Column(r.activeSetStable_);
        else if (attribute == "curveRiskIncluded")
            *value = Matrix_<Cell_>(1, 1, Cell_(r.curveRiskIncluded_));
        else
            THROW("InvalidGSRWorksheet: unknown quote risk attribute " + attribute);
    }

    void GSRSLVQuoteRiskResult_Get_Calibration(const Handle_<StorableGSRSLVQuoteRiskResult_>& result,
                                               Handle_<StorableGSRSLVCalibrationResult_>* calibration) {
        REQUIRE(result, "InvalidGSRWorksheet: quote risk result required");
        *calibration =
            Handle_<StorableGSRSLVCalibrationResult_>(new StorableGSRSLVCalibrationResult_("GSRSLVCalibrationResult", result->value_.calibration_));
    }

    void GSRCurveQuoteRisk_New(const Handle_<GSRCurveData_>& snapshot,
                               const Handle_<StorableRatePricingMarket_>& market,
                               const Handle_<StorableRateQuoteRiskProvenance_>& provenance,
                               const String_& discountComponent,
                               const Vector_<String_>& projectionComponents,
                               Handle_<StorableGSRCurveQuoteRisk_>* result) {
        REQUIRE(market && provenance && provenance->val_, "InvalidGSRWorksheet: native market and quote provenance required");
        *result = Handle_<StorableGSRCurveQuoteRisk_>(
            new StorableGSRCurveQuoteRisk_("GSRCurveQuoteRisk", BuildGSRCurveQuoteRisk(Handle_<Storable_>(snapshot), market->val_, *provenance->val_,
                                                                                       discountComponent, projectionComponents)));
    }

    void GSRCurveQuoteRisk_Get(const Handle_<StorableGSRCurveQuoteRisk_>& result, const String_& attribute, Matrix_<Cell_>* value) {
        REQUIRE(result, "InvalidGSRWorksheet: curve quote risk required");
        if (attribute == "quoteNames")
            *value = Column(result->value_.QuoteNames());
        else if (attribute == "quoteUnits")
            *value = Column(result->value_.QuoteUnits());
        else if (attribute == "logDFQuoteJacobian")
            *value = Cells(result->value_.LogDFQuoteJacobian());
        else
            THROW("InvalidGSRWorksheet: unknown curve risk attribute " + attribute);
    }

#ifdef _WIN32
#include <dal-excel/auto/MG_Calibrate_GSRSLV_public.inc>
#include <dal-excel/auto/MG_GSRCurveQuoteRisk_Get_public.inc>
#include <dal-excel/auto/MG_GSRCurveQuoteRisk_New_public.inc>
#include <dal-excel/auto/MG_GSRSLVCalibrationResult_Get_Model_public.inc>
#include <dal-excel/auto/MG_GSRSLVCalibrationResult_Get_public.inc>
#include <dal-excel/auto/MG_GSRSLVQuoteRiskResult_Get_Calibration_public.inc>
#include <dal-excel/auto/MG_GSRSLVQuoteRiskResult_Get_public.inc>
#include <dal-excel/auto/MG_GSRSLV_EuropeanOptionPrices_public.inc>
#include <dal-excel/auto/MG_GSRSLV_QuoteRisk_public.inc>
#endif
} // namespace Dal

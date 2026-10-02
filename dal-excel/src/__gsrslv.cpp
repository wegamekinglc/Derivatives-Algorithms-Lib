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
    Key/value rows: paths (4096), seed (1729), conditionalPaths (64)
&outputs
result is cell[][]
    One row per option: price, pair standard error, conditional refinement difference
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

/*IF--------------------------------------------------------------------------
public VolQuote_New
    Create a market volatility quote for a caplet or physical swaption
&inputs
name is string
    Unique quote name
option is handle StorableEuropeanRateOption
    Caplet or swaption
volatility is number
    Annualized decimal volatility
priceScale is number
    Positive price residual scale
&optional
convention is string
    NORMAL (default), BLACK or SHIFTED_BLACK
shift is number (0.0)
    Shift for SHIFTED_BLACK
&outputs
quote is handle StorableVolQuote
    Market quote handle
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public VolQuotes_Get_Prices
    Convert market volatility quotes on a curve snapshot
&inputs
snapshot is handle GSRCurveData
    Discount and projection curve snapshot
quotes is handle[]
    Market quote handles
&outputs
result is cell[][]
    One row per quote: forward, annuity, price, annualized-volatility vega
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public Calibrate_GSRSLVMarket
    Calibrate SLV to market volatility quotes
&inputs
initial is handle ModelData
    Initial GSRSLVModelData
quotes is handle[]
    Market quote handles
parameters is cell[][]
    Label, lower, upper, scale
&optional
settings is cell[][]
    Solver and Monte Carlo settings
heldOut is handle[]
    Held-out market quote handles
&outputs
result is handle StorableGSRSLVCalibrationResult
    Fitted model and diagnostics
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public GSRSLV_MarketQuoteRisk
    Recalibrate market volatility and curve quote bumps
&inputs
initial is handle ModelData
    Original GSRSLVModelData
quotes is handle[]
    Market quote handles
parameters is cell[][]
    Label, lower, upper, scale
targets is handle[]
    European target option handles
&optional
settings is cell[][]
    Calibration settings
riskSettings is cell[][]
    relativeBump, absoluteBump, stabilityTolerance
curveRisk is handle StorableGSRCurveQuoteRisk
    Curve quote provenance bridge
&outputs
result is handle StorableGSRSLVQuoteRiskResult
    Quote risk and stability diagnostics
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
                else if (key == "conditionalPaths")
                    settings.conditionalPaths_ = Integer(Cell::ToDouble(value));
                else
                    THROW("InvalidGSRWorksheet: unknown pricing setting " + key);
            }
            return settings;
        }

        GSRSLVCalibrationSettings_ Settings(const Matrix_<Cell_>& cells) {
            GSRSLVCalibrationSettings_ settings;
            const std::map<String_, int*> integers{{"paths", &settings.pricing_.paths_},
                                                   {"seed", &settings.pricing_.seed_},
                                                   {"conditionalPaths", &settings.pricing_.conditionalPaths_},
                                                   {"validationPaths", &settings.validation_.paths_},
                                                   {"validationSeed", &settings.validation_.seed_},
                                                   {"validationConditionalPaths", &settings.validation_.conditionalPaths_}};
            const std::map<String_, bool*> booleans{{"staged", &settings.staged_}, {"useAADJacobian", &settings.useAADJacobian_}};
            for (const auto& [key, value] : SettingsDictionary(cells)) {
                if (AssignSolver(&settings.solver_, key, value))
                    continue;
                const auto integer = integers.find(key);
                if (integer != integers.end())
                    *integer->second = Integer(Cell::ToDouble(value));
                else if (key == "validationSigma")
                    settings.validationSigma_ = Cell::ToDouble(value);
                else if (booleans.count(key)) {
                    REQUIRE(Cell::IsBool(value), "InvalidGSRWorksheet: " + key + " must be boolean");
                    *booleans.at(key) = Cell::ToBool(value);
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

        Vector_<EuropeanRateOption_> Options(const Vector_<Handle_<Storable_>>& handles) {
            Vector_<EuropeanRateOption_> result;
            for (const auto& handle : handles) {
                const auto option = handle_cast<StorableEuropeanRateOption_>(handle);
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
        *result = Matrix_<Cell_>(prices.size(), 3);
        for (size_t i = 0; i < prices.size(); ++i) {
            (*result)(i, 0) = Cell_(prices[i].price_);
            (*result)(i, 1) = Cell_(prices[i].standardError_);
            (*result)(i, 2) = Cell_(prices[i].conditionalError_);
        }
    }

    void Calibrate_GSRSLV(const Handle_<ModelData_>& initial,
                          const Vector_<Handle_<Storable_>>& quotes,
                          const Matrix_<Cell_>& parameters,
                          const Matrix_<Cell_>& settings,
                          const Vector_<Handle_<Storable_>>& heldOut,
                          Handle_<StorableGSRSLVCalibrationResult_>* result) {
        *result = Handle_<StorableGSRSLVCalibrationResult_>(new StorableGSRSLVCalibrationResult_(
            "GSRSLVCalibrationResult", CalibrateGSRSLV(initial, Values<CalibrationQuote_>(quotes), Parameters(parameters), Settings(settings),
                                                       Values<CalibrationQuote_>(heldOut))));
    }

    void GSRSLVCalibrationResult_Get(const Handle_<StorableGSRSLVCalibrationResult_>& result, const String_& attribute, Matrix_<Cell_>* value) {
        REQUIRE(result, "InvalidGSRWorksheet: calibration result required");
        const auto& r = result->value_;
        const std::map<String_, const Vector_<>*> vectors{{"modelPrices", &r.modelPrices_},
                                                          {"residuals", &r.residuals_},
                                                          {"standardErrors", &r.standardErrors_},
                                                          {"parameters", &r.parameters_},
                                                          {"validationPrices", &r.validationPrices_},
                                                          {"validationStandardErrors", &r.validationStandardErrors_},
                                                          {"numericalErrors", &r.numericalErrors_},
                                                          {"heldOutPrices", &r.heldOutPrices_},
                                                          {"heldOutResiduals", &r.heldOutResiduals_},
                                                          {"heldOutStandardErrors", &r.heldOutStandardErrors_},
                                                          {"conditionalErrors", &r.conditionalErrors_},
                                                          {"validationConditionalErrors", &r.validationConditionalErrors_},
                                                          {"heldOutConditionalErrors", &r.heldOutConditionalErrors_}};
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
            "GSRSLVQuoteRiskResult", GSRSLVQuoteRisk(initial, Values<CalibrationQuote_>(quotes), Parameters(parameters), Options(targets),
                                                     Settings(settings), RiskSettings(riskSettings), curveRisk ? &curveRisk->value_ : nullptr)));
    }

    void GSRSLVQuoteRiskResult_Get(const Handle_<StorableGSRSLVQuoteRiskResult_>& result, const String_& attribute, Matrix_<Cell_>* value) {
        REQUIRE(result, "InvalidGSRWorksheet: quote risk result required");
        const auto& r = result->value_;
        const std::map<String_, const Matrix_<>*> matrices{{"sensitivities", &r.sensitivities_}, {"refinementErrors", &r.refinementErrors_}};
        const auto matrix = matrices.find(attribute);
        if (matrix != matrices.end())
            *value = Cells(*matrix->second);
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

    void VolQuote_New(const String_& name,
                      const Handle_<StorableEuropeanRateOption_>& option,
                      double volatility,
                      double priceScale,
                      const String_& convention,
                      double shift,
                      Handle_<StorableVolQuote_>* quote) {
        REQUIRE(option, "InvalidGSRWorksheet: European option required");
        *quote = Handle_<StorableVolQuote_>(new StorableVolQuote_(
            "VolQuote", {name, option->option_, volatility, priceScale, VolConvention_(convention.empty() ? String_("NORMAL") : convention), shift}));
    }

    void VolQuotes_Get_Prices(const Handle_<GSRCurveData_>& snapshot, const Vector_<Handle_<Storable_>>& quotes, Matrix_<Cell_>* result) {
        const auto values = ConvertVolQuotes(Handle_<Storable_>(snapshot), Values<VolQuote_>(quotes));
        *result = Matrix_<Cell_>(values.size(), 4);
        for (size_t i = 0; i < values.size(); ++i) {
            (*result)(i, 0) = Cell_(values[i].forward_);
            (*result)(i, 1) = Cell_(values[i].annuity_);
            (*result)(i, 2) = Cell_(values[i].price_);
            (*result)(i, 3) = Cell_(values[i].vega_);
        }
    }

    void Calibrate_GSRSLVMarket(const Handle_<ModelData_>& initial,
                                const Vector_<Handle_<Storable_>>& quotes,
                                const Matrix_<Cell_>& parameters,
                                const Matrix_<Cell_>& settings,
                                const Vector_<Handle_<Storable_>>& heldOut,
                                Handle_<StorableGSRSLVCalibrationResult_>* result) {
        *result = Handle_<StorableGSRSLVCalibrationResult_>(new StorableGSRSLVCalibrationResult_(
            "GSRSLVCalibrationResult",
            CalibrateGSRSLVMarket(initial, Values<VolQuote_>(quotes), Parameters(parameters), Settings(settings), Values<VolQuote_>(heldOut))));
    }

    void GSRSLV_MarketQuoteRisk(const Handle_<ModelData_>& initial,
                                const Vector_<Handle_<Storable_>>& quotes,
                                const Matrix_<Cell_>& parameters,
                                const Vector_<Handle_<Storable_>>& targets,
                                const Matrix_<Cell_>& settings,
                                const Matrix_<Cell_>& riskSettings,
                                const Handle_<StorableGSRCurveQuoteRisk_>& curveRisk,
                                Handle_<StorableGSRSLVQuoteRiskResult_>* result) {
        *result = Handle_<StorableGSRSLVQuoteRiskResult_>(new StorableGSRSLVQuoteRiskResult_(
            "GSRSLVQuoteRiskResult",
            GSRSLVMarketQuoteRisk(initial, Values<VolQuote_>(quotes), Parameters(parameters), Options(targets), Settings(settings),
                                  RiskSettings(riskSettings), curveRisk ? &curveRisk->value_ : nullptr)));
    }

#ifdef _WIN32
#include <dal-excel/auto/MG_Calibrate_GSRSLVMarket_public.inc>
#include <dal-excel/auto/MG_Calibrate_GSRSLV_public.inc>
#include <dal-excel/auto/MG_GSRCurveQuoteRisk_Get_public.inc>
#include <dal-excel/auto/MG_GSRCurveQuoteRisk_New_public.inc>
#include <dal-excel/auto/MG_GSRSLVCalibrationResult_Get_Model_public.inc>
#include <dal-excel/auto/MG_GSRSLVCalibrationResult_Get_public.inc>
#include <dal-excel/auto/MG_GSRSLVQuoteRiskResult_Get_Calibration_public.inc>
#include <dal-excel/auto/MG_GSRSLVQuoteRiskResult_Get_public.inc>
#include <dal-excel/auto/MG_GSRSLV_EuropeanOptionPrices_public.inc>
#include <dal-excel/auto/MG_GSRSLV_MarketQuoteRisk_public.inc>
#include <dal-excel/auto/MG_GSRSLV_QuoteRisk_public.inc>
#include <dal-excel/auto/MG_VolQuote_New_public.inc>
#include <dal-excel/auto/MG_VolQuotes_Get_Prices_public.inc>
#endif
} // namespace Dal

//
// Created by wegam on 2026/6/20.
//

#include "__curvepricing_test_api.hpp"
#include "__platform.hpp"
#include "__settingskeys.hpp"
#include <dal-public/src/curveprotocol.hpp>
#include <dal-public/src/curvespec.hpp>
#include <dal-public/src/types.hpp>

/*IF--------------------------------------------------------------------------
public Calibrate_SingleCurve
    Calibrate a single yield curve from instruments and settings.
    Returns a result handle bundling the calibrated curve and the fit diagnostics.
&inputs
today is date
    The valuation/trade date
ccy is string
    Currency code (e.g. "USD")
instruments is handle[]
    Array of instrument handles (created with DEPOSIT.NEW, SWAP.NEW, etc.)
knotDates is date[]
    Knot dates (can be empty for auto-detection)
&optional
settings is cell[][]
    &$.Cols() == 2 || $.Empty()\must have two columns (key, value)
    Optional (key,value) settings. Keys: curveName, calibrateDiscountCurve, solveMode, parameterization, logDfScheme, smoothingWeight, tolerance, fitTolerance, initialGuess, maxEvaluations, maxRestarts, targetCollateral, targetTenor, liborBasis
discountCurve is handle StorableDiscountCurve
    Optional discount curve (OIS-collateralized) needed when calibrating a forward curve (calibrateDiscountCurve=FALSE)
baseCurve is handle StorableDiscountCurve
    Optional base curve layered under the calibrated curve; distinct from the forward-calibration pricing discount curve
&outputs
result is handle StorableCurveCalibrationResult
    The calibration result (curve + diagnostics)
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CalibrationResult_Get_Curve
    Extract the calibrated discount curve from a curve calibration result
&inputs
result is handle StorableCurveCalibrationResult
    The calibration result handle (from CALIBRATE.SINGLECURVE)
&outputs
curve is handle StorableDiscountCurve
    The calibrated discount curve
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CalibrationResult_Get
    Extract a diagnostic attribute from a curve calibration result.
    Use CALIBRATIONRESULT.GET.CURVE for the curve itself.
&inputs
result is handle StorableCurveCalibrationResult
    The calibration result handle (from CALIBRATE.SINGLECURVE)
attribute is string
    Attribute name: marketRates, modelRates, residuals, maxAbsResidual, rmsResidual
&outputs
value is cell[][]
    The requested diagnostic. Rate vectors (marketRates/modelRates/residuals) return
    as an Nx1 column; scalar stats (maxAbsResidual/rmsResidual) return as 1x1.
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CurveCalibrationStage_New
    Define one stage of a sequential same-currency curve calibration
&inputs
today is date
    Calibration date
currency is string
    Currency of the stage
instruments is handle[]
    Market instruments in original quote order
&optional
knotDates is date[]
    Explicit knots. When omitted the stage uses instrument maturities.
settings is cell[][]
    &$.Cols() == 2 || $.Empty()\must have two columns (key, value)
    Stage settings as in CALIBRATE.SINGLECURVE, including curveName, calibrateDiscountCurve, targetTenor, parameterization and tolerance
&outputs
stage is handle StorableCurveCalibrationSpec
    Input stage for CALIBRATE.MULTICURVE
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public Calibrate_MultiCurve
    Fit a sequential same-currency curve bundle
&inputs
stages is handle[]
    CURVECALIBRATIONSTAGE.NEW handles in calibration order
&optional
recordQuoteRisk is boolean (false)
    TRUE captures the discount and forward stages for quote risk
&outputs
result is handle StorableMultiCurveCalibrationResult
    The fitted curves and requested risk record
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public MultiCurveCalibrationResult_Get_Curve
    Extract a fitted curve for use in RATEPRICINGMARKET.NEW
&inputs
result is handle StorableMultiCurveCalibrationResult
    Result of CALIBRATE.MULTICURVE
role is string
    Discount or Forward
route is string
    Collateral type for Discount (e.g. OIS) or tenor for Forward (e.g. 6M)
&outputs
curve is handle StorableDiscountCurve
    Fitted discount or forward curve
-IF-------------------------------------------------------------------------*/

namespace Dal {
    namespace {
        bool IsStringSetting(const String_& key) {
            return key == "curveName" || key == "solveMode" || key == "parameterization" || key == "logDfScheme" || key == "liborBasis" ||
                   key == "targetCollateral" || key == "targetTenor";
        }

        bool IsDoubleSetting(const String_& key) {
            return key == "smoothingWeight" || key == "tolerance" || key == "fitTolerance" || key == "initialGuess";
        }

        bool IsIntegerSetting(const String_& key) { return key == "maxEvaluations" || key == "maxRestarts"; }

        // Helper: apply string-based settings
        void ApplyStringSettings(const String_& key, const Cell_& val, CurveCalibrationSpecBuilder_& b) {
            if (!IsStringSetting(key))
                return;
            REQUIRE(Cell::IsString(val), "InvalidSetting: " + key + " expects a string value");
            if (key == "curveName")
                b.curveName_ = Cell::ToString(val);
            else if (key == "solveMode")
                b.solveMode_ = CurveSolveMode_(Cell::ToString(val));
            else if (key == "parameterization")
                b.parameterization_ = CurveParameterization_(Cell::ToString(val));
            else if (key == "logDfScheme")
                b.logDfScheme_ = LogDfScheme_(Cell::ToString(val));
            else if (key == "liborBasis")
                b.liborBasis_ = DayBasis_(Cell::ToString(val));
            else if (key == "targetCollateral")
                b.targetCollateral_ = CollateralType_(Cell::ToString(val));
            else if (key == "targetTenor")
                b.targetTenor_ = PeriodLength_(Cell::ToString(val));
        }

        // Helper: apply double-valued settings (type check once, then dispatch by key)
        void ApplyDoubleSettings(const String_& key, const Cell_& val, CurveCalibrationSpecBuilder_& b) {
            if (!IsDoubleSetting(key))
                return;
            REQUIRE(Cell::IsDouble(val), "InvalidSetting: " + key + " expects a numeric value");
            double d = Cell::ToDouble(val);
            if (key == "smoothingWeight")
                b.smoothingWeight_ = d;
            else if (key == "tolerance")
                b.tolerance_ = d;
            else if (key == "fitTolerance")
                b.fitTolerance_ = d;
            else if (key == "initialGuess")
                b.initialGuess_ = d;
        }

        // Helper: apply int-valued settings
        void ApplyIntSettings(const String_& key, const Cell_& val, CurveCalibrationSpecBuilder_& b) {
            if (!IsIntegerSetting(key))
                return;
            REQUIRE(Cell::IsInt(val), "InvalidSetting: " + key + " expects an integer value");
            int i = Cell::ToInt(val);
            if (key == "maxEvaluations")
                b.maxEvaluations_ = i;
            else if (key == "maxRestarts")
                b.maxRestarts_ = i;
        }

        // Helper: apply dictionary settings to a CurveCalibrationSpecBuilder_
        void ApplySettings(const Dictionary_& settings, CurveCalibrationSpecBuilder_& b) {
            static const Vector_<String_> validKeys = {"curveName",      "calibrateDiscountCurve", "solveMode",      "parameterization",
                                                       "logDfScheme",    "smoothingWeight",        "tolerance",      "fitTolerance",
                                                       "initialGuess",   "maxEvaluations",         "maxRestarts",    "targetCollateral",
                                                       "targetTenor",    "liborBasis"};
            for (const auto& kv : settings) {
                const String_& key = kv.first;
                const Cell_& val = kv.second;
                RequireKnownSettingsKey(key, validKeys);
                if (key == "calibrateDiscountCurve") {
                    REQUIRE(Cell::IsBool(val), "InvalidSetting: calibrateDiscountCurve expects a boolean value");
                    b.calibrateDiscountCurve_ = Cell::ToBool(val);
                } else {
                    ApplyStringSettings(key, val, b);
                    ApplyDoubleSettings(key, val, b);
                    ApplyIntSettings(key, val, b);
                }
            }
        }

        CurveCalibrationSpecBuilder_ StageBuilder(const Date_& today,
                                                  const String_& ccy,
                                                  const Vector_<Handle_<Storable_>>& instrumentWrappers,
                                                  const Vector_<Date_>& knotDates,
                                                  const Matrix_<Cell_>& settings) {
            CurveCalibrationSpecBuilder_ builder;
            builder.today_ = today;
            builder.ccy_ = ccy;

            for (const auto& w : instrumentWrappers) {
                REQUIRE(w, "Invalid instrument handle");
                auto ycInst = handle_cast<StorableYCInstrument_>(w);
                REQUIRE(ycInst, "Instrument must be a YC instrument");
                builder.instruments_.push_back(ycInst->val_);
            }

            builder.knotDates_ = knotDates;

            // Convert Matrix_<Cell_> to Dictionary_ and apply optional settings
            if (!settings.Empty()) {
                Dictionary_ dict;
                for (int i = 0; i < settings.Rows(); ++i) {
                    if (Cell::IsEmpty(settings(i, 0)))
                        break;
                    dict.Insert(Cell::ToString(settings(i, 0)), settings(i, 1));
                }
                ApplySettings(dict, builder);
            }
            return builder;
        }

        void CurveCalibrationStage_New(const Date_& today,
                                       const String_& currency,
                                       const Vector_<Handle_<Storable_>>& instruments,
                                       const Vector_<Date_>& knotDates,
                                       const Matrix_<Cell_>& settings,
                                       Handle_<StorableCurveCalibrationSpec_>* stage) {
            auto builder = StageBuilder(today, currency, instruments, knotDates, settings);
            if (knotDates.empty())
                builder.knotPolicy_ = CurveKnotPolicy_::Value_::INSTRUMENTS;
            stage->reset(new StorableCurveCalibrationSpec_(builder.Build()));
        }

        void Calibrate_SingleCurve(const Date_& today,
                                   const String_& ccy,
                                   const Vector_<Handle_<Storable_>>& instrumentWrappers,
                                   const Vector_<Date_>& knotDates,
                                   const Matrix_<Cell_>& settings,
                                   const Handle_<StorableDiscountCurve_>& discountCurve,
                                   const Handle_<StorableDiscountCurve_>& baseCurve,
                                   Handle_<StorableCurveCalibrationResult_>* result) {
            auto builder = StageBuilder(today, ccy, instrumentWrappers, knotDates, settings);

            // Optional discount curve for forward-curve calibration, keyed by the
            // calibration's target collateral (defaults to OIS). Forward calibration
            // (calibrateDiscountCurve=FALSE) requires it; discount calibration does not.
            if (discountCurve)
                builder.discountCurves_[builder.targetCollateral_] = discountCurve->val_;
            else
                REQUIRE(builder.calibrateDiscountCurve_,
                        "Forward-curve calibration (calibrateDiscountCurve=FALSE) requires a discountCurve input");

            if (baseCurve)
                builder.baseCurve_ = baseCurve->val_;

            // Build spec and calibrate via the dal-public interface
            auto spec = builder.Build();
            CurveCalibrationOptions_ options;
            auto calibrated = Dal::CalibrateSingleCurve(spec, options);

            // Bundle curve + diagnostics into a single storable result handle
            result->reset(new StorableCurveCalibrationResult_(calibrated, spec, options));
        }

        void CalibrationResult_Get_Curve(const Handle_<StorableCurveCalibrationResult_>& result,
                                          Handle_<StorableDiscountCurve_>* curve) {
            REQUIRE(result, "Invalid calibration result handle");
            curve->reset(new StorableDiscountCurve_(result->val_.curve_));
        }

        void CalibrationResult_Get(const Handle_<StorableCurveCalibrationResult_>& result,
                                    const String_& attribute,
                                    Matrix_<Cell_>* value) {
            REQUIRE(result, "Invalid calibration result handle");
            const auto& diag = result->val_.diagnostics_;
            if (attribute == "marketRates")
                *value = AsCellColumn(diag.marketRates_);
            else if (attribute == "modelRates")
                *value = AsCellColumn(diag.modelRates_);
            else if (attribute == "residuals")
                *value = AsCellColumn(diag.residuals_);
            else if (attribute == "maxAbsResidual")
                *value = Matrix_<Cell_>(1, 1, Cell_(diag.maxAbsResidual_));
            else if (attribute == "rmsResidual")
                *value = Matrix_<Cell_>(1, 1, Cell_(diag.rmsResidual_));
            else
                THROW("Unknown calibration attribute: " + attribute
                      + " (expected marketRates, modelRates, residuals, maxAbsResidual, or rmsResidual)");
        }
        template <class K_> Handle_<DiscountCurve_> FittedCurve(const std::map<K_, Handle_<DiscountCurve_>>& curves, const K_& key) {
            const auto found = curves.find(key);
            REQUIRE(found != curves.end(), "Requested route is absent from the calibrated bundle");
            return found->second;
        }
    } // namespace

    void
    Calibrate_MultiCurve(const Vector_<Handle_<Storable_>>& stages, bool recordQuoteRisk, Handle_<StorableMultiCurveCalibrationResult_>* result) {
        REQUIRE(!stages.empty(), "At least one calibration stage is required");
        MultiCurveCalibrationSpec_ spec;
        for (const auto& input : stages) {
            const auto stage = handle_cast<StorableCurveCalibrationSpec_>(input);
            REQUIRE(stage, "Expected a CURVECALIBRATIONSTAGE.NEW handle");
            spec.stages_.push_back(stage->val_);
        }
        spec.ccy_ = spec.stages_.front().ccy_;
        spec.liborBasis_ = spec.stages_.front().liborBasis_;
        result->reset(new StorableMultiCurveCalibrationResult_(CalibrateMultiCurveBundle(spec, recordQuoteRisk)));
    }

    void MultiCurveCalibrationResult_Get_Curve(const Handle_<StorableMultiCurveCalibrationResult_>& result,
                                               const String_& role,
                                               const String_& route,
                                               Handle_<StorableDiscountCurve_>* curve) {
        REQUIRE(result, "Invalid multi-curve calibration result handle");
        REQUIRE(role == "Discount" || role == "Forward", "Expected Discount or Forward curve role");
        const auto fitted = role == "Discount" ? FittedCurve(result->val_.discountCurves_, CollateralType_(route))
                                               : FittedCurve(result->val_.forwardCurves_, PeriodLength_(route));
        curve->reset(new StorableDiscountCurve_(fitted));
    }
#ifdef _WIN32
#include <dal-excel/auto/MG_Calibrate_MultiCurve_public.inc>
#include <dal-excel/auto/MG_Calibrate_SingleCurve_public.inc>
#include <dal-excel/auto/MG_CalibrationResult_Get_Curve_public.inc>
#include <dal-excel/auto/MG_CalibrationResult_Get_public.inc>
#include <dal-excel/auto/MG_CurveCalibrationStage_New_public.inc>
#include <dal-excel/auto/MG_MultiCurveCalibrationResult_Get_Curve_public.inc>
#endif
} // namespace Dal

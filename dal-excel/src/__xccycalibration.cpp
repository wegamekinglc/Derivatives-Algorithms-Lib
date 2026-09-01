//
// Created by wegam on 2026/6/20.
//

#pragma once

#include "__platform.hpp"
#include "__curve_storable.hpp"
#include "__settingskeys.hpp"
#include "__xccy_test_api.hpp"
#include <cmath>
#include <dal-public/src/xccycalibration.hpp>
#include <dal/math/cell.hpp>
#include <dal/utilities/dictionary.hpp>

// clang-format off
/*IF--------------------------------------------------------------------------
public Calibrate_XccyMarket
    Calibrate a cross-currency basis market from instruments and settings.
    Returns a result handle bundling the basis curve and the fit diagnostics.
&inputs
today is date
    The valuation/trade date
domesticCcy is string
    Domestic currency code (e.g. "USD")
foreignCcy is string
    Foreign currency code (e.g. "EUR")
domesticBlock is handle StorableCurveBlock
    The pre-calibrated domestic curve block
foreignBlock is handle StorableCurveBlock
    The pre-calibrated foreign curve block
instruments is handle[]
    Array of cross-currency swap instrument handles (created with CROSSCURRENCYSWAP.NEW)
knotDates is date[]
    Knot dates for the basis curve
&optional
settings is cell[][]
    &$.Cols() == 2 || $.Empty()\must have two columns (key, value)
    Optional two-column (key,value) settings. Keys: fxSpot, fxForwardCollateral, smoothingWeight, tolerance, fitTolerance, initialGuess, maxEvaluations, maxRestarts, solveMode (EXACT|APPROXIMATE), jacobianMode (ANALYTIC|BUMPED), computeEffJacobianInverse, computeForwardJacobian
&outputs
result is handle StorableCrossCurrencyCalibrationResult
    The cross-currency calibration result (basis curve + diagnostics)
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public XccyCalibrationResult_Get_BasisCurve
    Extract the basis discount curve from a cross-currency calibration result
&inputs
result is handle StorableCrossCurrencyCalibrationResult
    The cross-currency calibration result handle (from CALIBRATE.XCCYMARKET)
&outputs
curve is handle StorableDiscountCurve
    The calibrated basis discount curve
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public XccyCalibrationResult_Get
    Extract a diagnostic attribute from a cross-currency calibration result.
    Use XCCYCALIBRATIONRESULT.GET.BASISCURVE for the basis curve itself.
&inputs
result is handle StorableCrossCurrencyCalibrationResult
    The cross-currency calibration result handle (from CALIBRATE.XCCYMARKET)
attribute is string
    Attribute name: marketRates, modelRates, residuals, maxAbsResidual, rmsResidual, instrumentNames, parameterKnotDates, jacobian, effJacobianInverse, residualTolerance, jacobianScaling, effJacobianInverseScaling, jacobianAvailability, effJacobianInverseAvailability
&outputs
value is cell[][]
    The requested diagnostic. Rate vectors (marketRates/modelRates/residuals) return
    as an Nx1 column; matrices retain their diagnostic axes; scalar and metadata values return as 1x1.
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public Calibrate_JointXccy
    Jointly calibrate one domestic discount curve, one foreign discount curve, and one cross-currency basis curve
&inputs
valuationTime is cell
    Valuation timestamp as an Excel date-time cell
currencies is handle StorableCurrencyPair
    The domestic and foreign currency pair
collateralCurrency is string
    Collateral currency code (currently the domestic currency)
fxSpot is number
    Positive domestic-per-foreign FX spot
domesticInstruments is handle[]
    Domestic yield-curve calibration instrument handles
domesticKnotDates is date[]
    Domestic discount-curve knot dates
foreignInstruments is handle[]
    Foreign yield-curve calibration instrument handles
foreignKnotDates is date[]
    Foreign discount-curve knot dates
basisInstruments is handle[]
    Cross-currency swap instrument handles
basisKnotDates is date[]
    Cross-currency basis-curve knot dates
&optional
fixings is handle StorableMarketFixingSnapshot
    Immutable fixing snapshot (created with MARKETFIXINGSNAPSHOT.NEW)
settings is cell[][]
    &$.Cols() == 2 || $.Empty()\must have two columns (key, value)
    Optional two-column (key,value) settings. Keys: domesticCurveName, foreignCurveName, basisCurveName, domesticLiborBasis, foreignLiborBasis, domesticParameterization, foreignParameterization, basisParameterization, domesticLogDfScheme, foreignLogDfScheme, domesticSmoothingWeight, foreignSmoothingWeight, basisSmoothingWeight, tolerance, fitTolerance, initialGuess, maxEvaluations, maxRestarts, solveMode, jacobianMode, computeEffJacobianInverse, computeForwardJacobian
&outputs
result is handle StorableJointXccyCalibrationResult
    The joint calibration result
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public JointXccyCalibrationResult_Get_DomesticBlock
    Extract the solved domestic curve block from a joint XCCY calibration result
&inputs
result is handle StorableJointXccyCalibrationResult
    The joint calibration result
&outputs
block is handle StorableCurveBlock
    The solved domestic curve block
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public JointXccyCalibrationResult_Get_ForeignBlock
    Extract the solved foreign curve block from a joint XCCY calibration result
&inputs
result is handle StorableJointXccyCalibrationResult
    The joint calibration result
&outputs
block is handle StorableCurveBlock
    The solved foreign curve block
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public JointXccyCalibrationResult_Get_BasisCurve
    Extract the solved cross-currency basis curve from a joint XCCY calibration result
&inputs
result is handle StorableJointXccyCalibrationResult
    The joint calibration result
&outputs
curve is handle StorableDiscountCurve
    The solved basis discount curve
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public JointXccyCalibrationResult_Get
    Extract a matrix-valued view from a joint XCCY calibration result. Use the dedicated getter functions for curve handles.
&inputs
result is handle StorableJointXccyCalibrationResult
    The joint calibration result
attribute is string
    Attribute: fxForwards, marketRates, modelRates, residuals, jacobian, effJacobianInverse, parameterRanges, or residualRanges. Handle views domesticBlock, foreignBlock, and basisCurve use their dedicated getter functions.
&outputs
value is cell[][]
    The requested joint result view
-IF-------------------------------------------------------------------------*/

// clang-format on
namespace Dal {
    namespace {
        void ApplyXccyStringSettings(const String_& key, const Cell_& val, CrossCurrencyCalibrationSpecBuilder_& b) {
            if (key == "fxForwardCollateral")
                b.fxForwardCollateral_ = CollateralType_(Cell::ToString(val));
            else if (key == "solveMode")
                b.solveMode_ = CurveSolveMode_(Cell::ToString(val));
        }

        void ApplyXccyDoubleSettings(const String_& key, const Cell_& val, CrossCurrencyCalibrationSpecBuilder_& b) {
            if (!Cell::IsDouble(val))
                return;
            double d = Cell::ToDouble(val);
            if (key == "fxSpot")
                b.fxSpot_ = d;
            else if (key == "smoothingWeight")
                b.smoothingWeight_ = d;
            else if (key == "tolerance")
                b.tolerance_ = d;
            else if (key == "fitTolerance")
                b.fitTolerance_ = d;
            else if (key == "initialGuess")
                b.initialGuess_ = d;
        }

        void ApplyXccyIntSettings(const String_& key, const Cell_& val, CrossCurrencyCalibrationSpecBuilder_& b) {
            if (!Cell::IsInt(val))
                return;
            int i = Cell::ToInt(val);
            if (key == "maxEvaluations")
                b.maxEvaluations_ = i;
            else if (key == "maxRestarts")
                b.maxRestarts_ = i;
        }

        String_ CellTypeName(const Cell_& value) {
            if (Cell::IsBool(value))
                return "boolean";
            if (Cell::IsDouble(value))
                return "number";
            if (Cell::IsDate(value))
                return "date";
            if (Cell::IsDateTime(value))
                return "date-time";
            if (Cell::IsString(value))
                return "string";
            return "empty";
        }

        void ApplyXccyOptionSetting(const String_& key, const Cell_& value, CrossCurrencyCalibrationOptions_& options) {
            if (key == "jacobianMode") {
                REQUIRE(Cell::IsString(value), "XCCY setting '" + key + "' received " + CellTypeName(value) + "; expected string ANALYTIC or BUMPED");
                options.jacobianMode_ = CurveJacobianMode_(Cell::ToString(value));
            } else if (key == "computeEffJacobianInverse") {
                REQUIRE(Cell::IsBool(value), "XCCY setting '" + key + "' received " + CellTypeName(value) + "; expected boolean true or false");
                options.computeEffJacobianInverse_ = Cell::ToBool(value);
            } else if (key == "computeForwardJacobian") {
                REQUIRE(Cell::IsBool(value), "XCCY setting '" + key + "' received " + CellTypeName(value) + "; expected boolean true or false");
                options.computeForwardJacobian_ = Cell::ToBool(value);
            }
        }

        void ApplyXccySettings(const Dictionary_& settings, CrossCurrencyCalibrationSpecBuilder_& b, CrossCurrencyCalibrationOptions_& options) {
            static const Vector_<String_> validKeys = {"fxSpot",
                                                       "fxForwardCollateral",
                                                       "smoothingWeight",
                                                       "tolerance",
                                                       "fitTolerance",
                                                       "initialGuess",
                                                       "maxEvaluations",
                                                       "maxRestarts",
                                                       "solveMode",
                                                       "jacobianMode",
                                                       "computeEffJacobianInverse",
                                                       "computeForwardJacobian"};
            for (const auto& kv : settings) {
                const String_& key = kv.first;
                const Cell_& val = kv.second;
                RequireKnownSettingsKey(key, validKeys);
                ApplyXccyStringSettings(key, val, b);
                ApplyXccyDoubleSettings(key, val, b);
                ApplyXccyIntSettings(key, val, b);
                ApplyXccyOptionSetting(key, val, options);
            }
        }

        DateTime_ JointValuationTime(const Cell_& value) {
            if (Cell::IsDouble(value)) {
                const double serial = Cell::ToDouble(value);
                REQUIRE(std::isfinite(serial), "Joint XCCY valuation time must be a finite Excel serial date");
                const int dateSerial = static_cast<int>(std::floor(serial));
                return DateTime_(Date::FromExcel(dateSerial), serial - dateSerial);
            }
            if (Cell::IsDate(value))
                return DateTime_(Cell::ToDate(value));
            return Cell::ToDateTime(value);
        }

        Dictionary_ SettingsDictionary(const Matrix_<Cell_>& settings) {
            Dictionary_ result;
            for (int i = 0; i < settings.Rows(); ++i) {
                if (Cell::IsEmpty(settings(i, 0)))
                    break;
                result.Insert(Cell::ToString(settings(i, 0)), settings(i, 1));
            }
            return result;
        }

        bool ApplyJointCurveNameSetting(const String_& key, const Cell_& value, JointXccyCalibrationSpecBuilder_& builder) {
            if (key == "domesticCurveName")
                builder.domestic_.curves_.front().curveName_ = Cell::ToString(value);
            else if (key == "foreignCurveName")
                builder.foreign_.curves_.front().curveName_ = Cell::ToString(value);
            else if (key == "basisCurveName")
                builder.basis_.curveName_ = Cell::ToString(value);
            else
                return false;
            return true;
        }

        bool ApplyJointLiborBasisSetting(const String_& key, const Cell_& value, JointXccyCalibrationSpecBuilder_& builder) {
            if (key == "domesticLiborBasis")
                builder.domestic_.liborBasis_ = DayBasis_(Cell::ToString(value));
            else if (key == "foreignLiborBasis")
                builder.foreign_.liborBasis_ = DayBasis_(Cell::ToString(value));
            else
                return false;
            return true;
        }

        bool ApplyJointParameterizationSetting(const String_& key, const Cell_& value, JointXccyCalibrationSpecBuilder_& builder) {
            if (key == "domesticParameterization")
                builder.domestic_.curves_.front().parameterization_ = CurveParameterization_(Cell::ToString(value));
            else if (key == "foreignParameterization")
                builder.foreign_.curves_.front().parameterization_ = CurveParameterization_(Cell::ToString(value));
            else if (key == "basisParameterization")
                builder.basis_.parameterization_ = CurveParameterization_(Cell::ToString(value));
            else
                return false;
            return true;
        }

        bool ApplyJointLogDfSetting(const String_& key, const Cell_& value, JointXccyCalibrationSpecBuilder_& builder) {
            if (key == "domesticLogDfScheme")
                builder.domestic_.curves_.front().logDfScheme_ = LogDfScheme_(Cell::ToString(value));
            else if (key == "foreignLogDfScheme")
                builder.foreign_.curves_.front().logDfScheme_ = LogDfScheme_(Cell::ToString(value));
            else
                return false;
            return true;
        }

        bool ApplyJointModeSetting(const String_& key,
                                   const Cell_& value,
                                   JointXccyCalibrationSpecBuilder_& builder,
                                   JointXccyCalibrationOptions_& options) {
            if (key == "solveMode")
                builder.solverOptions_.solveMode_ = CurveSolveMode_(Cell::ToString(value));
            else if (key == "jacobianMode")
                options.jacobianMode_ = CurveJacobianMode_(Cell::ToString(value));
            else
                return false;
            return true;
        }

        void ApplyJointStringSetting(const String_& key,
                                     const Cell_& value,
                                     JointXccyCalibrationSpecBuilder_& builder,
                                     JointXccyCalibrationOptions_& options) {
            if (ApplyJointCurveNameSetting(key, value, builder))
                return;
            if (ApplyJointLiborBasisSetting(key, value, builder))
                return;
            if (ApplyJointParameterizationSetting(key, value, builder))
                return;
            if (ApplyJointLogDfSetting(key, value, builder))
                return;
            ApplyJointModeSetting(key, value, builder, options);
        }

        void ApplyJointDoubleSetting(const String_& key, const Cell_& value, JointXccyCalibrationSpecBuilder_& builder) {
            if (!Cell::IsDouble(value))
                return;
            const double number = Cell::ToDouble(value);
            if (key == "domesticSmoothingWeight")
                builder.domestic_.curves_.front().smoothingWeight_ = number;
            else if (key == "foreignSmoothingWeight")
                builder.foreign_.curves_.front().smoothingWeight_ = number;
            else if (key == "basisSmoothingWeight")
                builder.basis_.smoothingWeight_ = number;
            else if (key == "tolerance")
                builder.solverOptions_.tolerance_ = number;
            else if (key == "fitTolerance")
                builder.solverOptions_.fitTolerance_ = number;
            else if (key == "initialGuess")
                builder.solverOptions_.initialGuess_ = number;
        }

        void ApplyJointIntSetting(const String_& key, const Cell_& value, JointXccyCalibrationSpecBuilder_& builder) {
            if (!Cell::IsInt(value))
                return;
            const int number = Cell::ToInt(value);
            if (key == "maxEvaluations")
                builder.solverOptions_.maxEvaluations_ = number;
            else if (key == "maxRestarts")
                builder.solverOptions_.maxRestarts_ = number;
        }

        void ApplyJointBoolSetting(const String_& key, const Cell_& value, JointXccyCalibrationOptions_& options) {
            if (!Cell::IsBool(value))
                return;
            if (key == "computeEffJacobianInverse")
                options.computeEffJacobianInverse_ = Cell::ToBool(value);
            else if (key == "computeForwardJacobian")
                options.computeForwardJacobian_ = Cell::ToBool(value);
        }

        void ApplyJointSettings(const Dictionary_& settings, JointXccyCalibrationSpecBuilder_& builder, JointXccyCalibrationOptions_& options) {
            static const Vector_<String_> validKeys = {"domesticCurveName",      "foreignCurveName",         "basisCurveName",
                                                       "domesticLiborBasis",     "foreignLiborBasis",        "domesticParameterization",
                                                       "foreignParameterization", "basisParameterization",    "domesticLogDfScheme",
                                                       "foreignLogDfScheme",     "domesticSmoothingWeight",   "foreignSmoothingWeight",
                                                       "basisSmoothingWeight",   "tolerance",                 "fitTolerance",
                                                       "initialGuess",           "maxEvaluations",            "maxRestarts",
                                                       "solveMode",              "jacobianMode",              "computeEffJacobianInverse",
                                                       "computeForwardJacobian"};
            for (const auto& setting : settings) {
                RequireKnownSettingsKey(setting.first, validKeys);
                ApplyJointStringSetting(setting.first, setting.second, builder, options);
                ApplyJointDoubleSetting(setting.first, setting.second, builder);
                ApplyJointIntSetting(setting.first, setting.second, builder);
                ApplyJointBoolSetting(setting.first, setting.second, options);
            }
        }

        void AddJointCurveInstruments(const Vector_<Handle_<Storable_>>& wrappers, Vector_<Handle_<YCInstrument_>>* instruments) {
            for (const auto& wrapper : wrappers) {
                REQUIRE(wrapper, "Invalid joint yield-curve instrument handle");
                const auto instrument = handle_cast<StorableYCInstrument_>(wrapper);
                REQUIRE(instrument, "Joint domestic and foreign instruments must be yield-curve instruments");
                instruments->push_back(instrument->val_);
            }
        }

        void AddJointBasisInstruments(const Vector_<Handle_<Storable_>>& wrappers, Vector_<Handle_<CrossCurrencySwap_>>* instruments) {
            for (const auto& wrapper : wrappers) {
                REQUIRE(wrapper, "Invalid joint basis instrument handle");
                const auto instrument = handle_cast<StorableCrossCurrencySwap_>(wrapper);
                REQUIRE(instrument, "Joint basis instruments must be cross-currency swaps");
                instruments->push_back(instrument->val_);
            }
        }

        Matrix_<Cell_> AsCellMatrix(const Matrix_<>& source) {
            Matrix_<Cell_> result(source.Rows(), source.Cols());
            for (int row = 0; row < source.Rows(); ++row)
                for (int col = 0; col < source.Cols(); ++col)
                    result(row, col) = Cell_(source(row, col));
            return result;
        }

        Matrix_<Cell_> StringsAsCells(const Vector_<String_>& source) {
            Matrix_<Cell_> result(source.size(), 1);
            for (int row = 0; row < source.size(); ++row)
                result(row, 0) = Cell_(source[row]);
            return result;
        }

        Matrix_<Cell_> DatesAsCells(const Vector_<Date_>& source) {
            Matrix_<Cell_> result(source.size(), 1);
            for (int row = 0; row < source.size(); ++row)
                result(row, 0) = Cell_(source[row]);
            return result;
        }

        Matrix_<Cell_> FxForwardsAsCells(const CrossCurrencyFxForwardCurve_& forwards) {
            REQUIRE(forwards.dates_.size() == forwards.forwards_.size(), "Joint XCCY FX-forward dates and values have inconsistent lengths");
            Matrix_<Cell_> result(forwards.dates_.size(), 2);
            for (int i = 0; i < forwards.dates_.size(); ++i) {
                result(i, 0) = Cell_(forwards.dates_[i]);
                result(i, 1) = Cell_(forwards.forwards_[i]);
            }
            return result;
        }

        Matrix_<Cell_> RangesAsCells(const Vector_<CalibrationBlockRange_>& ranges) {
            Matrix_<Cell_> result(ranges.size(), 3);
            for (int i = 0; i < ranges.size(); ++i) {
                result(i, 0) = Cell_(ranges[i].name_);
                result(i, 1) = Cell_(static_cast<double>(ranges[i].offset_));
                result(i, 2) = Cell_(static_cast<double>(ranges[i].size_));
            }
            return result;
        }

        using JointXccyResultViewGetter_ = Matrix_<Cell_> (*)(const JointXccyCalibrationResult_&);

        struct JointXccyResultView_ {
            const char* name_;
            JointXccyResultViewGetter_ getter_;
        };

        const JointXccyResultView_ JOINT_XCCY_RESULT_VIEWS[] = {
            {"fxForwards", [](const JointXccyCalibrationResult_& result) {
                 return FxForwardsAsCells(JointXccyResultFxForwards(result));
             }},
            {"marketRates", [](const JointXccyCalibrationResult_& result) {
                 return AsCellColumn(JointXccyResultMarketRates(result));
             }},
            {"modelRates", [](const JointXccyCalibrationResult_& result) {
                 return AsCellColumn(JointXccyResultModelRates(result));
             }},
            {"residuals", [](const JointXccyCalibrationResult_& result) {
                 return AsCellColumn(JointXccyResultResiduals(result));
             }},
            {"jacobian", [](const JointXccyCalibrationResult_& result) {
                 return AsCellMatrix(JointXccyResultJacobian(result));
             }},
            {"effJacobianInverse", [](const JointXccyCalibrationResult_& result) {
                 return AsCellMatrix(JointXccyResultEffJacobianInverse(result));
             }},
            {"parameterRanges", [](const JointXccyCalibrationResult_& result) {
                 return RangesAsCells(JointXccyResultParameterRanges(result));
             }},
            {"residualRanges", [](const JointXccyCalibrationResult_& result) {
                 return RangesAsCells(JointXccyResultResidualRanges(result));
             }},
        };

        using XccyResultViewGetter_ = Matrix_<Cell_> (*)(const CrossCurrencyCalibrationResult_&, const CrossCurrencyCalibrationDiagnostics_&);

        struct XccyResultView_ {
            const char* name_;
            XccyResultViewGetter_ getter_;
        };

        const XccyResultView_ XCCY_RESULT_VIEWS[] = {
            {"marketRates", [](const CrossCurrencyCalibrationResult_&,
                               const CrossCurrencyCalibrationDiagnostics_& diag) { return AsCellColumn(diag.marketRates_); }},
            {"modelRates",
             [](const CrossCurrencyCalibrationResult_&, const CrossCurrencyCalibrationDiagnostics_& diag) { return AsCellColumn(diag.modelRates_); }},
            {"residuals",
             [](const CrossCurrencyCalibrationResult_&, const CrossCurrencyCalibrationDiagnostics_& diag) { return AsCellColumn(diag.residuals_); }},
            {"maxAbsResidual", [](const CrossCurrencyCalibrationResult_&,
                                  const CrossCurrencyCalibrationDiagnostics_& diag) { return Matrix_<Cell_>(1, 1, Cell_(diag.maxAbsResidual_)); }},
            {"rmsResidual", [](const CrossCurrencyCalibrationResult_&,
                               const CrossCurrencyCalibrationDiagnostics_& diag) { return Matrix_<Cell_>(1, 1, Cell_(diag.rmsResidual_)); }},
            {"instrumentNames", [](const CrossCurrencyCalibrationResult_&,
                                   const CrossCurrencyCalibrationDiagnostics_& diag) { return StringsAsCells(diag.instrumentNames_); }},
            {"parameterKnotDates", [](const CrossCurrencyCalibrationResult_&,
                                      const CrossCurrencyCalibrationDiagnostics_& diag) { return DatesAsCells(diag.parameterKnotDates_); }},
            {"jacobian", [](const CrossCurrencyCalibrationResult_& result,
                            const CrossCurrencyCalibrationDiagnostics_&) { return AsCellMatrix(XccyResultJacobian(result)); }},
            {"effJacobianInverse", [](const CrossCurrencyCalibrationResult_& result,
                                      const CrossCurrencyCalibrationDiagnostics_&) { return AsCellMatrix(XccyResultEffJacobianInverse(result)); }},
            {"residualTolerance",
             [](const CrossCurrencyCalibrationResult_&, const CrossCurrencyCalibrationDiagnostics_& diag) {
                 return Matrix_<Cell_>(1, 1, Cell_(diag.residualTolerance_));
             }},
            {"jacobianScaling", [](const CrossCurrencyCalibrationResult_&,
                                   const CrossCurrencyCalibrationDiagnostics_& diag) { return Matrix_<Cell_>(1, 1, Cell_(diag.jacobianScaling_)); }},
            {"effJacobianInverseScaling",
             [](const CrossCurrencyCalibrationResult_&, const CrossCurrencyCalibrationDiagnostics_& diag) {
                 return Matrix_<Cell_>(1, 1, Cell_(diag.effJacobianInverseScaling_));
             }},
            {"jacobianAvailability",
             [](const CrossCurrencyCalibrationResult_&, const CrossCurrencyCalibrationDiagnostics_& diag) {
                 return Matrix_<Cell_>(1, 1, Cell_(diag.jacobianAvailability_));
             }},
            {"effJacobianInverseAvailability",
             [](const CrossCurrencyCalibrationResult_&, const CrossCurrencyCalibrationDiagnostics_& diag) {
                 return Matrix_<Cell_>(1, 1, Cell_(diag.effJacobianInverseAvailability_));
             }},
        };
    } // namespace

    void Calibrate_XccyMarket(const Date_& today,
                              const String_& domesticCcy,
                              const String_& foreignCcy,
                              const Handle_<StorableCurveBlock_>& domesticBlock,
                              const Handle_<StorableCurveBlock_>& foreignBlock,
                              const Vector_<Handle_<Storable_>>& instrumentWrappers,
                              const Vector_<Date_>& knotDates,
                              const Matrix_<Cell_>& settings,
                              Handle_<StorableCrossCurrencyCalibrationResult_>* result) {
        REQUIRE(domesticBlock, "Invalid domestic curve block handle");
        REQUIRE(foreignBlock, "Invalid foreign curve block handle");

        CrossCurrencyCalibrationSpecBuilder_ builder;
        builder.today_ = today;
        builder.basisPair_ = CurrencyPair_New(domesticCcy, foreignCcy);
        builder.domesticCurveBlock_ = domesticBlock->val_;
        builder.foreignCurveBlock_ = foreignBlock->val_;

        // Convert instrument wrappers to raw handles
        for (const auto& w : instrumentWrappers) {
            REQUIRE(w, "Invalid instrument handle");
            auto xccyInst = handle_cast<StorableCrossCurrencySwap_>(w);
            REQUIRE(xccyInst, "Instrument must be a cross-currency swap");
            builder.instruments_.push_back(xccyInst->val_);
        }

        builder.knotDates_ = knotDates;

        CrossCurrencyCalibrationOptions_ options;
        // Convert Matrix_<Cell_> to Dictionary_ and apply optional settings
        if (!settings.Empty()) {
            Dictionary_ dict;
            for (int i = 0; i < settings.Rows(); ++i) {
                if (Cell::IsEmpty(settings(i, 0)))
                    break;
                dict.Insert(Cell::ToString(settings(i, 0)), settings(i, 1));
            }
            ApplyXccySettings(dict, builder, options);
        }

        const CurrencyPair_ pair = builder.basisPair_;
        REQUIRE(builder.fxSpot_ > 0.0, "Cross-currency calibration requires an fxSpot setting (e.g. fxSpot=1.10)");
        auto spec = builder.Build();
        auto calibrated = Dal::CalibrateXccyMarket(spec, options);

        // Resolve the basis curve for the calibrated currency pair
        auto it = calibrated.basisCurves_.find(pair);
        REQUIRE(it != calibrated.basisCurves_.end(), "Basis curve not found for requested currency pair");
        result->reset(new StorableCrossCurrencyCalibrationResult_(calibrated, spec, options, it->second));
    }

    void XccyCalibrationResult_Get_BasisCurve(const Handle_<StorableCrossCurrencyCalibrationResult_>& result,
                                              Handle_<StorableDiscountCurve_>* curve) {
        REQUIRE(result, "Invalid XCCY calibration result handle");
        curve->reset(new StorableDiscountCurve_(result->basisCurve_));
    }

    void XccyCalibrationResult_Get(const Handle_<StorableCrossCurrencyCalibrationResult_>& result, const String_& attribute, Matrix_<Cell_>* value) {
        REQUIRE(result, "Invalid XCCY calibration result handle");
        const auto& calibration = result->val_;
        const auto& diag = XccyResultDiagnostics(calibration);
        for (const auto& view : XCCY_RESULT_VIEWS) {
            if (attribute == view.name_) {
                *value = view.getter_(calibration, diag);
                return;
            }
        }
        THROW("Unknown XCCY calibration attribute: " + attribute +
              " (accepted views: marketRates, modelRates, residuals, maxAbsResidual, rmsResidual, instrumentNames, "
              "parameterKnotDates, jacobian, effJacobianInverse, residualTolerance, jacobianScaling, effJacobianInverseScaling, "
              "jacobianAvailability, effJacobianInverseAvailability)");
    }

    void Calibrate_JointXccy(const Cell_& valuationTime,
                             const Handle_<StorableCurrencyPair_>& currencies,
                             const String_& collateralCurrency,
                             double fxSpot,
                             const Vector_<Handle_<Storable_>>& domesticInstruments,
                             const Vector_<Date_>& domesticKnotDates,
                             const Vector_<Handle_<Storable_>>& foreignInstruments,
                             const Vector_<Date_>& foreignKnotDates,
                             const Vector_<Handle_<Storable_>>& basisInstruments,
                             const Vector_<Date_>& basisKnotDates,
                             const Handle_<StorableMarketFixingSnapshot_>& fixings,
                             const Matrix_<Cell_>& settings,
                             Handle_<StorableJointXccyCalibrationResult_>* result) {
        REQUIRE(currencies, "Invalid currency pair handle");

        JointXccyCalibrationSpecBuilder_ builder;
        builder.valuationTime_ = JointValuationTime(valuationTime);
        builder.pair_ = currencies->val_;
        builder.collateralCurrency_ = Ccy_(collateralCurrency);
        builder.fxSpot_ = fxSpot;
        builder.domestic_.ccy_ = currencies->val_.domestic_;
        builder.foreign_.ccy_ = currencies->val_.foreign_;
        builder.domestic_.curves_.push_back(JointCurveDeclaration_());
        builder.foreign_.curves_.push_back(JointCurveDeclaration_());

        auto& domestic = builder.domestic_.curves_.front();
        domestic.curveName_ = String_(currencies->val_.domestic_.String()) + "_ois";
        domestic.knotDates_ = domesticKnotDates;
        domestic.targetCollateral_ = CollateralType_(CollateralType_::Value_::OIS);
        domestic.calibrateDiscountCurve_ = true;
        AddJointCurveInstruments(domesticInstruments, &domestic.instruments_);

        auto& foreign = builder.foreign_.curves_.front();
        foreign.curveName_ = String_(currencies->val_.foreign_.String()) + "_ois";
        foreign.knotDates_ = foreignKnotDates;
        foreign.targetCollateral_ = CollateralType_(CollateralType_::Value_::OIS);
        foreign.calibrateDiscountCurve_ = true;
        AddJointCurveInstruments(foreignInstruments, &foreign.instruments_);

        builder.basis_.curveName_ = String_(currencies->val_.domestic_.String()) + "_" + currencies->val_.foreign_.String() + "_basis";
        builder.basis_.knotDates_ = basisKnotDates;
        AddJointBasisInstruments(basisInstruments, &builder.basis_.instruments_);
        if (fixings)
            builder.fixings_ = fixings->val_;

        JointXccyCalibrationOptions_ options;
        if (!settings.Empty())
            ApplyJointSettings(SettingsDictionary(settings), builder, options);

        const auto spec = builder.Build();
        result->reset(new StorableJointXccyCalibrationResult_(Dal::CalibrateJointXccyMarket(spec, options), spec, options));
    }

    void JointXccyCalibrationResult_Get_DomesticBlock(const Handle_<StorableJointXccyCalibrationResult_>& result,
                                                      Handle_<StorableCurveBlock_>* block) {
        REQUIRE(result, "Invalid joint XCCY calibration result handle");
        block->reset(new StorableCurveBlock_(result->domesticBlock_));
    }

    void JointXccyCalibrationResult_Get_ForeignBlock(const Handle_<StorableJointXccyCalibrationResult_>& result,
                                                     Handle_<StorableCurveBlock_>* block) {
        REQUIRE(result, "Invalid joint XCCY calibration result handle");
        block->reset(new StorableCurveBlock_(result->foreignBlock_));
    }

    void JointXccyCalibrationResult_Get_BasisCurve(const Handle_<StorableJointXccyCalibrationResult_>& result,
                                                   Handle_<StorableDiscountCurve_>* curve) {
        REQUIRE(result, "Invalid joint XCCY calibration result handle");
        curve->reset(new StorableDiscountCurve_(result->basisCurve_));
    }

    void JointXccyCalibrationResult_Get(const Handle_<StorableJointXccyCalibrationResult_>& result, const String_& attribute, Matrix_<Cell_>* value) {
        REQUIRE(result, "Invalid joint XCCY calibration result handle");
        for (const auto& view : JOINT_XCCY_RESULT_VIEWS) {
            if (attribute == view.name_) {
                *value = view.getter_(result->val_);
                return;
            }
        }
        THROW("Unknown joint XCCY calibration attribute: " + attribute +
              " (accepted views: domesticBlock, foreignBlock, basisCurve, fxForwards, marketRates, modelRates, residuals, jacobian, "
              "effJacobianInverse, parameterRanges, residualRanges; use the dedicated DOMESTICBLOCK, FOREIGNBLOCK, and BASISCURVE getter "
              "functions for handle views)");
    }
    // clang-format off
#ifdef _WIN32
#include <dal-excel/auto/MG_Calibrate_XccyMarket_public.inc>
#include <dal-excel/auto/MG_XccyCalibrationResult_Get_BasisCurve_public.inc>
#include <dal-excel/auto/MG_XccyCalibrationResult_Get_public.inc>
#include <dal-excel/auto/MG_Calibrate_JointXccy_public.inc>
#include <dal-excel/auto/MG_JointXccyCalibrationResult_Get_DomesticBlock_public.inc>
#include <dal-excel/auto/MG_JointXccyCalibrationResult_Get_ForeignBlock_public.inc>
#include <dal-excel/auto/MG_JointXccyCalibrationResult_Get_BasisCurve_public.inc>
#include <dal-excel/auto/MG_JointXccyCalibrationResult_Get_public.inc>
#endif
    // clang-format on
} // namespace Dal

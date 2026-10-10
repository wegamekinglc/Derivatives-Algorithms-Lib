//
// Created by Codex on 2026/10/11.
//

#include "__ratecurvature.hpp"

#include <string>

#include "__curvaturerows.hpp"
#include "__ratecurvatureinput.hpp"

// clang-format off
/*IF--------------------------------------------------------------------------
public RateCalibration_New
    Seal a retained native rate calibration specification and solve anew
&inputs
name is string
    Object name
+argName = "name"; Excel::ValidateRiskRequestText(xl_name, "RateCalibration_New; name");
source is handle
    Existing single, joint, staged-XCCY or joint-XCCY result; native default solve options
&outputs
calibration is handle StorableRateCalibrationSnapshot
    Owning EXACT square snapshot; old parameters and option overrides are not replayed
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public RateCalibration_Recalibrate
    Rebuild a sealed native rate snapshot at complete raw quotes
&inputs
name is string
    Object name
+argName = "name"; Excel::ValidateRiskRequestText(xl_name, "RateCalibration_Recalibrate; name");
+argName = "quotes"; Excel::ValidateCurvatureNumbers(xl_quotes, "RateCalibration_Recalibrate", "quotes");
+const Excel::ScriptSettingsInput_ normalizedNumbers(xl_quotes); xl_quotes = normalizedNumbers.Get();
calibration is handle StorableRateCalibrationSnapshot
    Owning sealed calibration snapshot
quotes is cell[][]
    One finite numeric value per complete raw quote; row or column vector
&outputs
rebuilt is handle StorableRateCalibrationSnapshot
    New immutable snapshot with the same quote and parameter axes
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public RateCalibration_Get_Point
    Copy the complete raw decimal rate quote point
&inputs
calibration is handle StorableRateCalibrationSnapshot
    Owning sealed calibration snapshot
&outputs
point is cell[][]
    Complete raw quote column in native provenance order
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public RateCalibration_Get_Parameters
    Copy the solved native rate free parameters
&inputs
calibration is handle StorableRateCalibrationSnapshot
    Owning sealed calibration snapshot
&outputs
parameters is cell[][]
    Free parameter column in native provenance order
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public RateCalibration_Get_QuotePlan
    Copy the complete native rate quote metadata plan
&inputs
calibration is handle StorableRateCalibrationSnapshot
    Owning sealed calibration snapshot
&outputs
plan is handle StorableCalibrationRiskPlan
    Use CALIBRATIONRISKPLAN.GET.INPUTS with complete=true; Point supplies quote values
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public RateTradeQuoteCurvatureSettings_New
    Copy native rate portfolio weights and optional fixing observations
&inputs
name is string
    Object name
+argName = "name"; Excel::ValidateRiskRequestText(xl_name, "RateTradeQuoteCurvatureSettings_New; name");
+argName = "weights"; Excel::ValidateCurvatureNumbers(xl_weights, "RateTradeQuoteCurvatureSettings_New", "weights");
+const Excel::ScriptSettingsInput_ normalizedNumbers(xl_weights); xl_weights = normalizedNumbers.Get();
&optional
weights is cell[][]+
    Finite row or column weights; blank means unit weights; signed and zero weights allowed
fixings is handle StorableMarketFixingSnapshot
    Explicit historical observations; blank permits native capture of missing trade history
&outputs
settings is handle StorableRateTradeQuoteCurvatureSettings
    Owning copied weights and optional historical observations
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public RateTradeQuoteCurvatureSettings_Get_Weights
    Copy signed native rate portfolio weights
&inputs
settings is handle StorableRateTradeQuoteCurvatureSettings
    Owning portfolio settings
&outputs
weights is cell[][]
    Weight column; blank denotes unit weights
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public RateTradeQuoteCurvatureSettings_Get_Fixings
    Copy saved fixing records and explicit snapshot presence
&inputs
settings is handle StorableRateTradeQuoteCurvatureSettings
    Owning portfolio settings
&outputs
fixings is cell[][]
    Three columns: presence row then index, fixing time, value; absent differs from explicit empty
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public RateTradeQuoteCurvatureResult_New
    Evaluate signed rate quote Hessian products through full native recalibration
&inputs
name is string
    Object name
+argName = "name"; Excel::ValidateRiskRequestText(xl_name, "RateTradeQuoteCurvatureResult_New; name");
+argName = "trades"; Excel::ValidateRateTradeHandles(xl_trades);
trades is handle[]
    Native rate trade rows; every row is validated including zero-weight rows
calibration is handle StorableRateCalibrationSnapshot
    Owning EXACT square native calibration snapshot
bumps is handle StorableBumpOverAADRequest
    Full raw directions and positive steps; native numeric and recording caps remain separate
&optional
settings is handle StorableRateTradeQuoteCurvatureSettings
    Copied weights and history; blank uses unit weights and native history admission
&outputs
result is handle StorableRateTradeQuoteCurvatureResult
    Owning finite-step result; actual PV currencies must agree, with no FX conversion
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public RateTradeQuoteCurvatureResult_Get_Value
    Copy weighted native portfolio pv
&inputs
result is handle StorableRateTradeQuoteCurvatureResult
    Completed owning native rate result
&outputs
value is number
    Weighted native portfolio PV
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public RateTradeQuoteCurvatureResult_Get_Currency
    Copy actual common pv currency; no portfolio currency conversion
&inputs
result is handle StorableRateTradeQuoteCurvatureResult
    Completed owning native rate result
&outputs
currency is string
    Actual common PV currency; no portfolio currency conversion
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public RateTradeQuoteCurvatureResult_Get_Point
    Copy complete raw decimal quote column
&inputs
result is handle StorableRateTradeQuoteCurvatureResult
    Completed owning native rate result
&outputs
point is cell[][]
    Complete raw decimal quote column
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public RateTradeQuoteCurvatureResult_Get_Gradient
    Copy complete raw quote-gradient column; no dv01 scaling
&inputs
result is handle StorableRateTradeQuoteCurvatureResult
    Completed owning native rate result
&outputs
gradient is cell[][]
    Complete raw quote-gradient column; no DV01 scaling
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public RateTradeQuoteCurvatureResult_Get_Directions
    Copy signed full raw quote rows; empty products spill one blank cell
&inputs
result is handle StorableRateTradeQuoteCurvatureResult
    Completed owning native rate result
&outputs
directions is cell[][]
    Signed full raw quote rows; empty products spill one blank cell
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public RateTradeQuoteCurvatureResult_Get_Steps
    Copy positive finite-step column
&inputs
result is handle StorableRateTradeQuoteCurvatureResult
    Completed owning native rate result
&outputs
steps is cell[][]
    Positive finite-step column
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public RateTradeQuoteCurvatureResult_Get_HessianProducts
    Copy direction rows and full quote columns; zero directions spill one blank cell
&inputs
result is handle StorableRateTradeQuoteCurvatureResult
    Completed owning native rate result
&outputs
products is cell[][]
    Direction rows and full quote columns; zero directions spill one blank cell
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public RateTradeQuoteCurvatureResult_Get_Shape
    Copy one row: direction count then complete quote count
&inputs
result is handle StorableRateTradeQuoteCurvatureResult
    Completed owning native rate result
&outputs
shape is cell[][]
    One row: direction count then complete quote count
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public RateTradeQuoteCurvatureResult_Get_Execution
    Copy method, evaluations, calibrations, reverses, numeric bytes, peak tape bytes and cleanup reserve
&inputs
result is handle StorableRateTradeQuoteCurvatureResult
    Completed owning native rate result
&outputs
execution is cell[][]
    Method, evaluations, calibrations, reverses, numeric bytes, peak tape bytes and cleanup reserve
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public RateTradeQuoteCurvatureResult_Get_BaseCalibration
    Copy owning base snapshot; its quote plan supplies complete raw metadata
&inputs
result is handle StorableRateTradeQuoteCurvatureResult
    Completed owning native rate result
&outputs
calibration is handle StorableRateCalibrationSnapshot
    Owning base snapshot; its quote plan supplies complete raw metadata
-IF-------------------------------------------------------------------------*/

// clang-format on

namespace Dal {
    namespace {
        template <class H_, class... R_> RateCalibrationSnapshot_ CaptureRateSource(const Handle_<Storable_>& source) {
            if (const auto* value = dynamic_cast<const H_*>(source.get()))
                return NewRateCalibration(value->spec_);
            if constexpr (sizeof...(R_) > 0)
                return CaptureRateSource<R_...>(source);
            THROW("InvalidRateCurvatureRequest: RateCalibration_New; source; expected retained single/joint/staged-XCCY/joint-XCCY result");
        }

        Vector_<> RateNumbers(const Matrix_<Cell_>& cells, const String_& field) {
            if (Excel::BlankCurvatureRange(cells))
                return {};
            REQUIRE(cells.Rows() == 1 || cells.Cols() == 1, "InvalidRateCurvatureRequest: " + field + "; expected row or column vector");
            REQUIRE(cells.Rows() <= Excel::CURVATURE_MAX_ROWS && cells.Cols() <= Excel::CURVATURE_MAX_COLUMNS,
                    "InvalidRateCurvatureRequest: " + field + "; exceeds worksheet bounds");
            Vector_<> values;
            for (int row = 0; row < cells.Rows(); ++row)
                for (int column = 0; column < cells.Cols(); ++column)
                    values.push_back(Excel::CurvatureNumber(cells(row, column), "InvalidRateCurvatureRequest: " + field +
                                                                                    "; row=" + String_(std::to_string(row + 1)) +
                                                                                    "; column=" + String_(std::to_string(column + 1)) + "; "));
            return values;
        }

        Vector_<RateTradeDefinition_> RateTrades(const Vector_<Handle_<Storable_>>& handles) {
            Vector_<RateTradeDefinition_> trades;
            for (const auto& handle : handles) {
                const auto* trade = dynamic_cast<const StorableRateTradeDefinition_*>(handle.get());
                REQUIRE(trade, "InvalidRateCurvatureRequest: RateTradeQuoteCurvatureResult_New; trades; row=" +
                                   String_(std::to_string(trades.size() + 1)) + "; expected native rate trade handle");
                trades.push_back(trade->val_);
            }
            return trades;
        }

        const RateQuoteCurvatureResult_& RateCurvature(const Handle_<StorableRateTradeQuoteCurvatureResult_>& result, const char* field) {
            return Excel::CheckedCurvatureValue(result, field).Curvature();
        }
    } // namespace

    void RateCalibration_New(const String_& name, const Handle_<Storable_>& source, Handle_<StorableRateCalibrationSnapshot_>* result) {
        Excel::CheckCurvatureText(name, "RateCalibration_New; name");
        REQUIRE(source, "InvalidRateCurvatureRequest: RateCalibration_New; source; handle is null");
        auto value = CaptureRateSource<StorableCurveCalibrationResult_, StorableJointMultiCurveCalibrationResult_,
                                       StorableCrossCurrencyCalibrationResult_, StorableJointXccyCalibrationResult_>(source);
        result->reset(new StorableRateCalibrationSnapshot_(name, std::move(value)));
    }

    void RateCalibration_Recalibrate(const String_& name,
                                     const Handle_<StorableRateCalibrationSnapshot_>& calibration,
                                     const Matrix_<Cell_>& quotes,
                                     Handle_<StorableRateCalibrationSnapshot_>* result) {
        Excel::CheckCurvatureText(name, "RateCalibration_Recalibrate; name");
        const auto& value = Excel::CheckedCurvatureValue(calibration, "RateCalibration_Recalibrate; calibration");
        const auto point = RateNumbers(quotes, "RateCalibration_Recalibrate; quotes");
        result->reset(new StorableRateCalibrationSnapshot_(name, RecalibrateRateWithRisk(value, point)));
    }

    void RateCalibration_Get_Point(const Handle_<StorableRateCalibrationSnapshot_>& calibration, Matrix_<Cell_>* point) {
        *point = Excel::CurvatureVectorCells(Excel::CheckedCurvatureValue(calibration, "RateCalibration_Get_Point; calibration").Point());
    }

    void RateCalibration_Get_Parameters(const Handle_<StorableRateCalibrationSnapshot_>& calibration, Matrix_<Cell_>* parameters) {
        *parameters =
            Excel::CurvatureVectorCells(Excel::CheckedCurvatureValue(calibration, "RateCalibration_Get_Parameters; calibration").Parameters());
    }

    void RateCalibration_Get_QuotePlan(const Handle_<StorableRateCalibrationSnapshot_>& calibration, Handle_<StorableCalibrationRiskPlan_>* plan) {
        const auto& value = Excel::CheckedCurvatureValue(calibration, "RateCalibration_Get_QuotePlan; calibration");
        plan->reset(new StorableCalibrationRiskPlan_("", PlanCalibrationRiskRequest(NewCalibrationPullback(value.Provenance()))));
    }

    void RateTradeQuoteCurvatureSettings_New(const String_& name,
                                             const Matrix_<Cell_>& weights,
                                             const Handle_<StorableMarketFixingSnapshot_>& fixings,
                                             Handle_<StorableRateTradeQuoteCurvatureSettings_>* result) {
        Excel::CheckCurvatureText(name, "RateTradeQuoteCurvatureSettings_New; name");
        RateTradeQuoteCurvatureSettings_ value;
        value.weights_ = RateNumbers(weights, "RateTradeQuoteCurvatureSettings_New; weights");
        if (fixings) {
            REQUIRE(fixings->val_, "InvalidRateCurvatureRequest: RateTradeQuoteCurvatureSettings_New; fixings; snapshot is null");
            value.fixings_.reset(new MarketFixingSnapshot_(*fixings->val_));
        }
        result->reset(new StorableRateTradeQuoteCurvatureSettings_(name, std::move(value)));
    }

    void RateTradeQuoteCurvatureSettings_Get_Weights(const Handle_<StorableRateTradeQuoteCurvatureSettings_>& settings, Matrix_<Cell_>* weights) {
        *weights =
            Excel::CurvatureVectorCells(Excel::CheckedCurvatureValue(settings, "RateTradeQuoteCurvatureSettings_Get_Weights; settings").weights_);
    }

    void RateTradeQuoteCurvatureSettings_Get_Fixings(const Handle_<StorableRateTradeQuoteCurvatureSettings_>& settings, Matrix_<Cell_>* fixings) {
        const auto& value = Excel::CheckedCurvatureValue(settings, "RateTradeQuoteCurvatureSettings_Get_Fixings; settings");
        size_t rows = 1;
        if (value.fixings_)
            for (const auto& entry : value.fixings_->Values()) {
                REQUIRE(entry.second.size() <= static_cast<size_t>(Excel::CURVATURE_MAX_ROWS) - rows,
                        "InvalidRateCurvatureRequest: fixing history exceeds worksheet row limit");
                rows += entry.second.size();
            }
        Matrix_<Cell_> copy(static_cast<int>(rows), 3);
        copy(0, 0) = "explicit_snapshot";
        copy(0, 1) = bool(value.fixings_);
        int row = 1;
        if (value.fixings_)
            for (const auto& entry : value.fixings_->Values())
                for (const auto& observation : entry.second) {
                    copy(row, 0) = entry.first;
                    copy(row, 1) = observation.first;
                    copy(row++, 2) = observation.second;
                }
        *fixings = std::move(copy);
    }

    void RateTradeQuoteCurvatureResult_New(const String_& name,
                                           const Vector_<Handle_<Storable_>>& trades,
                                           const Handle_<StorableRateCalibrationSnapshot_>& calibration,
                                           const Handle_<StorableBumpOverAADRequest_>& bumps,
                                           const Handle_<StorableRateTradeQuoteCurvatureSettings_>& settings,
                                           Handle_<StorableRateTradeQuoteCurvatureResult_>* result) {
        Excel::CheckCurvatureText(name, "RateTradeQuoteCurvatureResult_New; name");
        const auto& source = Excel::CheckedCurvatureValue(calibration, "RateTradeQuoteCurvatureResult_New; calibration");
        const auto& request = Excel::CheckedCurvatureValue(bumps, "RateTradeQuoteCurvatureResult_New; bumps");
        const auto configuration = settings ? settings->val_ : RateTradeQuoteCurvatureSettings_();
        result->reset(
            new StorableRateTradeQuoteCurvatureResult_(name, EvaluateRateTradeQuoteCurvature(RateTrades(trades), source, request, configuration)));
    }

    void RateTradeQuoteCurvatureResult_Get_Value(const Handle_<StorableRateTradeQuoteCurvatureResult_>& result, double* value) {
        *value = RateCurvature(result, "RateTradeQuoteCurvatureResult_Get_Value; result").Value();
    }

    void RateTradeQuoteCurvatureResult_Get_Currency(const Handle_<StorableRateTradeQuoteCurvatureResult_>& result, String_* currency) {
        *currency = Excel::CheckedCurvatureValue(result, "RateTradeQuoteCurvatureResult_Get_Currency; result").Currency().String();
    }

    void RateTradeQuoteCurvatureResult_Get_Point(const Handle_<StorableRateTradeQuoteCurvatureResult_>& result, Matrix_<Cell_>* point) {
        *point = Excel::CurvatureVectorCells(RateCurvature(result, "RateTradeQuoteCurvatureResult_Get_Point; result").Point());
    }

    void RateTradeQuoteCurvatureResult_Get_Gradient(const Handle_<StorableRateTradeQuoteCurvatureResult_>& result, Matrix_<Cell_>* gradient) {
        *gradient = Excel::CurvatureVectorCells(RateCurvature(result, "RateTradeQuoteCurvatureResult_Get_Gradient; result").Gradient());
    }

    void RateTradeQuoteCurvatureResult_Get_Directions(const Handle_<StorableRateTradeQuoteCurvatureResult_>& result, Matrix_<Cell_>* directions) {
        *directions = Excel::CurvatureMatrixCells(RateCurvature(result, "RateTradeQuoteCurvatureResult_Get_Directions; result").Directions());
    }

    void RateTradeQuoteCurvatureResult_Get_Steps(const Handle_<StorableRateTradeQuoteCurvatureResult_>& result, Matrix_<Cell_>* steps) {
        *steps = Excel::CurvatureVectorCells(RateCurvature(result, "RateTradeQuoteCurvatureResult_Get_Steps; result").Steps());
    }

    void RateTradeQuoteCurvatureResult_Get_HessianProducts(const Handle_<StorableRateTradeQuoteCurvatureResult_>& result, Matrix_<Cell_>* products) {
        *products = Excel::CurvatureMatrixCells(RateCurvature(result, "RateTradeQuoteCurvatureResult_Get_HessianProducts; result").HessianProducts());
    }

    void RateTradeQuoteCurvatureResult_Get_Shape(const Handle_<StorableRateTradeQuoteCurvatureResult_>& result, Matrix_<Cell_>* shape) {
        *shape = Excel::RiskShapeCells(RateCurvature(result, "RateTradeQuoteCurvatureResult_Get_Shape; result").HessianProducts());
    }

    void RateTradeQuoteCurvatureResult_Get_Execution(const Handle_<StorableRateTradeQuoteCurvatureResult_>& result, Matrix_<Cell_>* execution) {
        using namespace Excel;
        const auto& value = RateCurvature(result, "RateTradeQuoteCurvatureResult_Get_Execution; result").Execution();
        *execution =
            FieldCells({Field("method", value.method_), Field("quote_gradient_evaluations", double(value.quoteGradientEvaluations_)),
                        Field("calibrations", double(value.calibrations_)), Field("objective_reverse_sweeps", double(value.objectiveReverseSweeps_)),
                        Field("numeric_payload_bytes", double(value.numericPayloadBytes_)), Field("peak_tape_bytes", double(value.peakTapeBytes_)),
                        Field("cleanup_reserve_bytes", double(value.cleanupReserveBytes_))});
    }

    void RateTradeQuoteCurvatureResult_Get_BaseCalibration(const Handle_<StorableRateTradeQuoteCurvatureResult_>& result,
                                                           Handle_<StorableRateCalibrationSnapshot_>* calibration) {
        calibration->reset(new StorableRateCalibrationSnapshot_(
            "", RateCurvature(result, "RateTradeQuoteCurvatureResult_Get_BaseCalibration; result").BaseCalibration()));
    }

    // clang-format off
#ifdef _WIN32
#include <dal-excel/auto/MG_RateCalibration_New_public.inc>
#include <dal-excel/auto/MG_RateCalibration_Recalibrate_public.inc>
#include <dal-excel/auto/MG_RateCalibration_Get_Point_public.inc>
#include <dal-excel/auto/MG_RateCalibration_Get_Parameters_public.inc>
#include <dal-excel/auto/MG_RateCalibration_Get_QuotePlan_public.inc>
#include <dal-excel/auto/MG_RateTradeQuoteCurvatureSettings_New_public.inc>
#include <dal-excel/auto/MG_RateTradeQuoteCurvatureSettings_Get_Weights_public.inc>
#include <dal-excel/auto/MG_RateTradeQuoteCurvatureSettings_Get_Fixings_public.inc>
#include <dal-excel/auto/MG_RateTradeQuoteCurvatureResult_New_public.inc>
#include <dal-excel/auto/MG_RateTradeQuoteCurvatureResult_Get_Value_public.inc>
#include <dal-excel/auto/MG_RateTradeQuoteCurvatureResult_Get_Currency_public.inc>
#include <dal-excel/auto/MG_RateTradeQuoteCurvatureResult_Get_Point_public.inc>
#include <dal-excel/auto/MG_RateTradeQuoteCurvatureResult_Get_Gradient_public.inc>
#include <dal-excel/auto/MG_RateTradeQuoteCurvatureResult_Get_Directions_public.inc>
#include <dal-excel/auto/MG_RateTradeQuoteCurvatureResult_Get_Steps_public.inc>
#include <dal-excel/auto/MG_RateTradeQuoteCurvatureResult_Get_HessianProducts_public.inc>
#include <dal-excel/auto/MG_RateTradeQuoteCurvatureResult_Get_Shape_public.inc>
#include <dal-excel/auto/MG_RateTradeQuoteCurvatureResult_Get_Execution_public.inc>
#include <dal-excel/auto/MG_RateTradeQuoteCurvatureResult_Get_BaseCalibration_public.inc>
#endif
    // clang-format on
} // namespace Dal

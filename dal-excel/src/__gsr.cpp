//
// Created by Codex on 2026/10/2.
//

#include "__gsr_helpers.hpp"

#include <cmath>
#include <map>

// clang-format off
/*IF--------------------------------------------------------------------------
public BondOption_New
    Create a European zero-coupon bond option
&inputs
expiry is date
    Exercise date
maturity is date
    Bond maturity
strike is number
    Bond-price strike per unit face value
type is string
    CALL or PUT
&outputs
option is handle StorableGSREuropeanOption
    Option handle
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public FixedCoupon_New
    Create one fixed swap coupon
&inputs
payment is date
    Payment date
accrual is number
    Coupon accrual fraction
&outputs
coupon is handle StorableGSRFixedCoupon
    Coupon handle
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public FloatingCoupon_New
    Create one projected floating coupon
&inputs
fixing is date
    Index fixing date, on or before start
start is date
    Accrual start date
end is date
    Accrual end date
payment is date
    Payment date, on or after end
indexAccrual is number
    Index accrual fraction
couponAccrual is number
    Payment accrual fraction
tenor is string
    Projection tenor such as 3M
&outputs
coupon is handle StorableGSRFloatingCoupon
    Coupon handle
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public Caplet_New
    Create a caplet or floorlet
&inputs
expiry is date
    Index fixing date, on or before accrual start
coupon is handle StorableGSRFloatingCoupon
    Underlying coupon
strike is number
    Rate strike
type is string
    CALL for caplet or PUT for floorlet
&outputs
option is handle StorableGSREuropeanOption
    Option handle
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public Swaption_New
    Create a physically settled European swaption
&inputs
expiry is date
    Exercise date
fixed is handle[]
    GSRFixedCoupon handles
floating is handle[]
    GSRFloatingCoupon handles
strike is number
    Fixed rate strike
type is string
    CALL for payer or PUT for receiver
&outputs
option is handle StorableGSREuropeanOption
    Option handle
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public GSR_EuropeanOptionPrice
    Price a European rate option in a Gaussian short rate model
&inputs
model is handle ModelData
    GSRModelData or MultiFactorGSRModelData
option is handle StorableGSREuropeanOption
    Bond option, caplet or swaption
&optional
settings is cell[][]
    Key/value rows: quadratureOrder (16), estimateError (true)
&outputs
result is cell[][]
    One row: price, numerical refinement estimate
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CalibrationQuote_New
    Create a price quote for Gaussian volatility calibration
&inputs
name is string
    Unique quote name
option is handle StorableGSREuropeanOption
    Quoted option
price is number
    Market price per unit notional
priceScale is number
    Positive price error scale
&outputs
quote is handle StorableGSRCalibrationQuote
    Calibration quote
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public Calibrate_GSRVolatility
    Fit selected g buckets with curves, H and correlations fixed
&inputs
initial is handle ModelData
    Initial MultiFactorGSRModelData
quotes is handle[]
    GSRCalibrationQuote handles in residual order
parameters is number[][]
    Four columns: factor index, knot index, lower bound, upper bound
&optional
settings is cell[][]
    Key/value rows: maxIterations, quadratureOrder, gradientTolerance, stepTolerance, finiteDifferenceStep, parameterScale, priorWeight, smoothingWeight, numericalErrorFraction
&outputs
result is handle StorableGSRCalibrationResult
    Fitted model and diagnostics
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public GSRCalibrationResult_Get
    Read a Gaussian calibration diagnostic
&inputs
result is handle StorableGSRCalibrationResult
    Calibration result
attribute is string
    modelPrices, residuals, numericalErrors, parameters, activeBounds, quoteJacobian, converged, fitWithinTolerance, numericalValidationPassed, iterations, evaluations, objective, jacobianRank, jacobianConditionEstimate, terminationReason
&outputs
value is cell[][]
    Scalar, column vector or quote-by-parameter matrix
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public GSRCalibrationResult_Get_Model
    Extract the calibrated Gaussian model
&inputs
result is handle StorableGSRCalibrationResult
    Calibration result
&outputs
model is handle ModelData
    MultiFactorGSRModelData usable by pricing and Monte Carlo
-IF-------------------------------------------------------------------------*/
// clang-format on

namespace Dal {
    namespace {
        using namespace GSRWorksheet;

        GSRCalibrationSettings_ Settings(const Matrix_<Cell_>& cells) {
            REQUIRE(cells.Empty() || cells.Cols() == 2, "InvalidGSRWorksheet: settings must have two columns");
            GSRCalibrationSettings_ settings;
            Vector_<String_> used;
            for (int row = 0; row < cells.Rows(); ++row) {
                const String_ key = Cell::ToString(cells(row, 0));
                REQUIRE(std::find(used.begin(), used.end(), key) == used.end(), "InvalidGSRWorksheet: duplicate settings key " + key);
                used.push_back(key);
                const double number = Cell::ToDouble(cells(row, 1));
                if (AssignSolver(&settings, key, cells(row, 1)))
                    continue;
                else if (key == "parameterScale")
                    settings.parameterScale_ = number;
                else if (key == "quadratureOrder")
                    settings.pricing_.quadratureOrder_ = Integer(number);
                else
                    THROW("InvalidGSRWorksheet: unknown settings key " + key);
            }
            return settings;
        }

        GSRPricingSettings_ PricingSettings(const Matrix_<Cell_>& cells) {
            REQUIRE(cells.Empty() || cells.Cols() == 2, "InvalidGSRWorksheet: settings must have two columns");
            GSRPricingSettings_ settings;
            Dictionary_ values;
            for (int row = 0; row < cells.Rows(); ++row)
                values.Insert(Cell::ToString(cells(row, 0)), cells(row, 1));
            for (const auto& [key, value] : values) {
                if (key == "quadratureOrder")
                    settings.quadratureOrder_ = Integer(Cell::ToDouble(value));
                else if (key == "estimateError") {
                    REQUIRE(Cell::IsBool(value), "InvalidGSRWorksheet: estimateError must be boolean");
                    settings.estimateError_ = Cell::ToBool(value);
                } else
                    THROW("InvalidGSRWorksheet: unknown pricing settings key " + key);
            }
            return settings;
        }
    } // namespace

    void
    BondOption_New(const Date_& expiry, const Date_& maturity, double strike, const String_& type, Handle_<StorableEuropeanRateOption_>* option) {
        *option = Handle_<StorableEuropeanRateOption_>(new StorableEuropeanRateOption_(BondOption_{expiry, maturity, strike, OptionType_(type)}));
    }

    void FixedCoupon_New(const Date_& payment, double accrual, Handle_<StorableFixedCoupon_>* coupon) {
        *coupon = Handle_<StorableFixedCoupon_>(new StorableFixedCoupon_("FixedCoupon", {payment, accrual}));
    }

    void FloatingCoupon_New(const Date_& fixing,
                            const Date_& start,
                            const Date_& end,
                            const Date_& payment,
                            double indexAccrual,
                            double couponAccrual,
                            const String_& tenor,
                            Handle_<StorableFloatingCoupon_>* coupon) {
        *coupon = Handle_<StorableFloatingCoupon_>(
            new StorableFloatingCoupon_("FloatingCoupon", {fixing, start, end, payment, indexAccrual, couponAccrual, tenor}));
    }

    void Caplet_New(const Date_& expiry,
                    const Handle_<StorableFloatingCoupon_>& coupon,
                    double strike,
                    const String_& type,
                    Handle_<StorableEuropeanRateOption_>* option) {
        REQUIRE(coupon, "InvalidGSRWorksheet: coupon is required");
        const auto& value = coupon->value_;
        REQUIRE(value.fixing_ == expiry, "InvalidGSRWorksheet: caplet expiry must equal coupon fixing date");
        *option = Handle_<StorableEuropeanRateOption_>(new StorableEuropeanRateOption_(Caplet_{
            expiry, value.start_, value.end_, value.payment_, value.indexAccrual_, value.couponAccrual_, value.tenor_, strike, OptionType_(type)}));
    }

    void Swaption_New(const Date_& expiry,
                      const Vector_<Handle_<Storable_>>& fixed,
                      const Vector_<Handle_<Storable_>>& floating,
                      double strike,
                      const String_& type,
                      Handle_<StorableEuropeanRateOption_>* option) {
        *option = Handle_<StorableEuropeanRateOption_>(new StorableEuropeanRateOption_(
            Swaption_{expiry, Values<FixedCoupon_>(fixed), Values<FloatingCoupon_>(floating), strike, OptionType_(type)}));
    }

    void GSR_EuropeanOptionPrice(const Handle_<ModelData_>& model,
                                 const Handle_<StorableEuropeanRateOption_>& option,
                                 const Matrix_<Cell_>& settings,
                                 Matrix_<Cell_>* result) {
        REQUIRE(option, "InvalidGSRWorksheet: option is required");
        const auto priced = PriceGSREuropeanOption(model, option->option_, PricingSettings(settings));
        *result = Matrix_<Cell_>(1, 2);
        (*result)(0, 0) = Cell_(priced.price_);
        (*result)(0, 1) = Cell_(priced.numericalError_);
    }

    void CalibrationQuote_New(const String_& name,
                              const Handle_<StorableEuropeanRateOption_>& option,
                              double price,
                              double priceScale,
                              Handle_<StorableCalibrationQuote_>* quote) {
        REQUIRE(option, "InvalidGSRWorksheet: option is required");
        *quote = Handle_<StorableCalibrationQuote_>(new StorableCalibrationQuote_("CalibrationQuote", {name, option->option_, price, priceScale}));
    }

    void Calibrate_GSRVolatility(const Handle_<ModelData_>& initial,
                                 const Vector_<Handle_<Storable_>>& quotes,
                                 const Matrix_<>& parameters,
                                 const Matrix_<Cell_>& settings,
                                 Handle_<StorableGSRCalibrationResult_>* result) {
        REQUIRE(parameters.Cols() == 4, "InvalidGSRWorksheet: parameters must have four columns");
        Vector_<GSRCalibrationParameter_> selected;
        for (int row = 0; row < parameters.Rows(); ++row)
            selected.push_back({Integer(parameters(row, 0)), Integer(parameters(row, 1)), parameters(row, 2), parameters(row, 3)});
        *result = Handle_<StorableGSRCalibrationResult_>(new StorableGSRCalibrationResult_(
            "GSRCalibrationResult", CalibrateGSRVolatility(initial, Values<CalibrationQuote_>(quotes), selected, Settings(settings))));
    }

    void GSRCalibrationResult_Get(const Handle_<StorableGSRCalibrationResult_>& result, const String_& attribute, Matrix_<Cell_>* value) {
        REQUIRE(result, "InvalidGSRWorksheet: result is required");
        const auto& data = result->value_;
        const std::map<String_, const Vector_<>*> vectors{{"modelPrices", &data.modelPrices_},
                                                          {"residuals", &data.residuals_},
                                                          {"numericalErrors", &data.numericalErrors_},
                                                          {"parameters", &data.parameters_}};
        const auto vector = vectors.find(attribute);
        if (vector != vectors.end()) {
            *value = Column(*vector->second);
            return;
        }
        if (attribute == "activeBounds") {
            *value = Column(data.activeBounds_);
            return;
        }
        if (attribute == "quoteJacobian") {
            *value = Matrix_<Cell_>(data.quoteJacobian_.Rows(), data.quoteJacobian_.Cols());
            for (int row = 0; row < value->Rows(); ++row)
                for (int col = 0; col < value->Cols(); ++col)
                    (*value)(row, col) = Cell_(data.quoteJacobian_(row, col));
            return;
        }
        const std::map<String_, Cell_> scalars{{"converged", Cell_(data.converged_)},
                                               {"fitWithinTolerance", Cell_(data.fitWithinTolerance_)},
                                               {"numericalValidationPassed", Cell_(data.numericalValidationPassed_)},
                                               {"iterations", Cell_(double(data.iterations_))},
                                               {"evaluations", Cell_(double(data.evaluations_))},
                                               {"objective", Cell_(data.objective_)},
                                               {"jacobianRank", Cell_(double(data.jacobianRank_))},
                                               {"jacobianConditionEstimate", Cell_(data.jacobianConditionEstimate_)},
                                               {"terminationReason", Cell_(data.terminationReason_)}};
        const auto scalar = scalars.find(attribute);
        REQUIRE(scalar != scalars.end(), "InvalidGSRWorksheet: unknown result attribute " + attribute);
        *value = Matrix_<Cell_>(1, 1, scalar->second);
    }

    void GSRCalibrationResult_Get_Model(const Handle_<StorableGSRCalibrationResult_>& result, Handle_<ModelData_>* model) {
        REQUIRE(result, "InvalidGSRWorksheet: result is required");
        *model = Handle_<ModelData_>(result->value_.model_);
    }

#ifdef _WIN32
#include <dal-excel/auto/MG_BondOption_New_public.inc>
#include <dal-excel/auto/MG_Calibrate_GSRVolatility_public.inc>
#include <dal-excel/auto/MG_CalibrationQuote_New_public.inc>
#include <dal-excel/auto/MG_Caplet_New_public.inc>
#include <dal-excel/auto/MG_FixedCoupon_New_public.inc>
#include <dal-excel/auto/MG_FloatingCoupon_New_public.inc>
#include <dal-excel/auto/MG_GSRCalibrationResult_Get_Model_public.inc>
#include <dal-excel/auto/MG_GSRCalibrationResult_Get_public.inc>
#include <dal-excel/auto/MG_GSR_EuropeanOptionPrice_public.inc>
#include <dal-excel/auto/MG_Swaption_New_public.inc>
#endif
} // namespace Dal

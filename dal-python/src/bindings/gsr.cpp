//
// Created by Codex on 2026/10/2.
//

#include "bindings.h"

#include <pybind11/stl.h>

#include <type_traits>

#include <dal-public/src/gsr.hpp>
#include <dal-public/src/curvepricing.hpp>

using namespace Dal;

namespace {
    template <class T_> py::list List(const Vector_<T_>& values) {
        py::list result;
        for (const T_& value : values)
            if constexpr (std::is_same_v<T_, String_>)
                result.append(std::string(value.c_str()));
            else
                result.append(value);
        return result;
    }

    Handle_<ModelData_> Model(const std::shared_ptr<ModelData_>& model) { return Handle_<ModelData_>(std::shared_ptr<const ModelData_>(model)); }
} // namespace

void init_bindings_gsr(py::module_& m) {
    py::class_<BondOption_>(m, "BondOption_")
        .def(py::init([](const Date_& expiry, const Date_& maturity, double strike, const std::string& type) {
                 return BondOption_{expiry, maturity, strike, OptionType_(String_(type))};
             }),
             py::arg("expiry"), py::arg("maturity"), py::arg("strike"), py::arg("type") = "CALL");
    py::class_<FixedCoupon_>(m, "FixedCoupon_")
        .def(py::init([](const Date_& payment, double accrual) { return FixedCoupon_{payment, accrual}; }), py::arg("payment"), py::arg("accrual"));
    py::class_<FloatingCoupon_>(m, "FloatingCoupon_")
        .def(py::init(
                 [](const Date_& fixing, const Date_& start, const Date_& end, const Date_& payment, double indexAccrual, double couponAccrual,
                    const std::string& tenor) { return FloatingCoupon_{fixing, start, end, payment, indexAccrual, couponAccrual, String_(tenor)}; }),
             py::arg("fixing"), py::arg("start"), py::arg("end"), py::arg("payment"), py::arg("index_accrual"), py::arg("coupon_accrual"),
             py::arg("tenor"));
    py::class_<Caplet_>(m, "Caplet_")
        .def(py::init([](const Date_& expiry, const Date_& start, const Date_& end, const Date_& payment, double indexAccrual, double couponAccrual,
                         const std::string& tenor, double strike, const std::string& type) {
                 return Caplet_{expiry, start, end, payment, indexAccrual, couponAccrual, String_(tenor), strike, OptionType_(String_(type))};
             }),
             py::arg("expiry"), py::arg("start"), py::arg("end"), py::arg("payment"), py::arg("index_accrual"), py::arg("coupon_accrual"),
             py::arg("tenor"), py::arg("strike"), py::arg("type") = "CALL");
    py::class_<Swaption_>(m, "Swaption_")
        .def(py::init([](const Date_& expiry, const std::vector<FixedCoupon_>& fixed, const std::vector<FloatingCoupon_>& floating, double strike,
                         const std::string& type) {
                 return Swaption_{expiry, {fixed.begin(), fixed.end()}, {floating.begin(), floating.end()}, strike, OptionType_(String_(type))};
             }),
             py::arg("expiry"), py::arg("fixed"), py::arg("floating"), py::arg("strike"), py::arg("type") = "CALL");
    py::class_<GSRPricingSettings_>(m, "GSRPricingSettings_")
        .def(py::init<>())
        .def_readwrite("quadrature_order", &GSRPricingSettings_::quadratureOrder_)
        .def_readwrite("estimate_error", &GSRPricingSettings_::estimateError_);
    py::class_<GSRPriceResult_>(m, "GSRPriceResult_")
        .def_readonly("price", &GSRPriceResult_::price_)
        .def_readonly("numerical_error", &GSRPriceResult_::numericalError_);
    py::class_<CalibrationQuote_>(m, "CalibrationQuote_")
        .def(py::init([](const std::string& name, const EuropeanRateOption_& option, double price, double priceScale) {
                 return CalibrationQuote_{String_(name), option, price, priceScale};
             }),
             py::arg("name"), py::arg("option"), py::arg("price"), py::arg("price_scale"));
    py::class_<GSRCalibrationParameter_>(m, "GSRCalibrationParameter_")
        .def(py::init([](int factor, int knot, double lower, double upper) { return GSRCalibrationParameter_{factor, knot, lower, upper}; }),
             py::arg("factor"), py::arg("knot"), py::arg("lower"), py::arg("upper"));
    py::class_<GSRCalibrationSettings_>(m, "GSRCalibrationSettings_")
        .def(py::init<>())
        .def_readwrite("max_iterations", &GSRCalibrationSettings_::maxIterations_)
        .def_readwrite("gradient_tolerance", &GSRCalibrationSettings_::gradientTolerance_)
        .def_readwrite("step_tolerance", &GSRCalibrationSettings_::stepTolerance_)
        .def_readwrite("finite_difference_step", &GSRCalibrationSettings_::finiteDifferenceStep_)
        .def_readwrite("parameter_scale", &GSRCalibrationSettings_::parameterScale_)
        .def_readwrite("prior_weight", &GSRCalibrationSettings_::priorWeight_)
        .def_readwrite("smoothing_weight", &GSRCalibrationSettings_::smoothingWeight_)
        .def_readwrite("numerical_error_fraction", &GSRCalibrationSettings_::numericalErrorFraction_)
        .def_readwrite("pricing", &GSRCalibrationSettings_::pricing_);
    py::class_<GSRCalibrationResult_>(m, "GSRCalibrationResult_")
        .def_property_readonly("model",
                               [](const GSRCalibrationResult_& result) {
                                   return std::const_pointer_cast<ModelData_>(std::static_pointer_cast<const ModelData_>(result.model_));
                               })
        .def_property_readonly("model_prices", [](const GSRCalibrationResult_& result) { return List(result.modelPrices_); })
        .def_property_readonly("residuals", [](const GSRCalibrationResult_& result) { return List(result.residuals_); })
        .def_property_readonly("numerical_errors", [](const GSRCalibrationResult_& result) { return List(result.numericalErrors_); })
        .def_property_readonly("parameters", [](const GSRCalibrationResult_& result) { return List(result.parameters_); })
        .def_property_readonly("active_bounds", [](const GSRCalibrationResult_& result) { return List(result.activeBounds_); })
        .def_readonly("quote_jacobian", &GSRCalibrationResult_::quoteJacobian_)
        .def_readonly("converged", &GSRCalibrationResult_::converged_)
        .def_readonly("fit_within_tolerance", &GSRCalibrationResult_::fitWithinTolerance_)
        .def_readonly("numerical_validation_passed", &GSRCalibrationResult_::numericalValidationPassed_)
        .def_readonly("iterations", &GSRCalibrationResult_::iterations_)
        .def_readonly("evaluations", &GSRCalibrationResult_::evaluations_)
        .def_readonly("jacobian_rank", &GSRCalibrationResult_::jacobianRank_)
        .def_readonly("jacobian_condition_estimate", &GSRCalibrationResult_::jacobianConditionEstimate_)
        .def_readonly("objective", &GSRCalibrationResult_::objective_)
        .def_property_readonly("termination_reason",
                               [](const GSRCalibrationResult_& result) { return std::string(result.terminationReason_.c_str()); });
    m.def(
        "GSR_EuropeanOptionPrice",
        [](const std::shared_ptr<ModelData_>& model, const EuropeanRateOption_& option, const GSRPricingSettings_& settings) {
            return PriceGSREuropeanOption(Model(model), option, settings);
        },
        py::arg("model"), py::arg("option"), py::arg("settings") = GSRPricingSettings_{});
    m.def(
        "Calibrate_GSRVolatility",
        [](const std::shared_ptr<ModelData_>& initial, const std::vector<CalibrationQuote_>& quotes,
           const std::vector<GSRCalibrationParameter_>& parameters, const GSRCalibrationSettings_& settings) {
            return CalibrateGSRVolatility(Model(initial), {quotes.begin(), quotes.end()}, {parameters.begin(), parameters.end()}, settings);
        },
        py::arg("initial"), py::arg("quotes"), py::arg("parameters"), py::arg("settings") = GSRCalibrationSettings_{});

    py::class_<GSRMonteCarloSettings_>(m, "GSRMonteCarloSettings_")
        .def(py::init<>())
        .def_readwrite("paths", &GSRMonteCarloSettings_::paths_)
        .def_readwrite("seed", &GSRMonteCarloSettings_::seed_)
        .def_readwrite("conditional_paths", &GSRMonteCarloSettings_::conditionalPaths_);
    py::class_<GSRMonteCarloPrice_>(m, "GSRMonteCarloPrice_")
        .def_readonly("price", &GSRMonteCarloPrice_::price_)
        .def_readonly("standard_error", &GSRMonteCarloPrice_::standardError_)
        .def_readonly("conditional_error", &GSRMonteCarloPrice_::conditionalError_);
    py::class_<GSRSLVCalibrationParameter_>(m, "GSRSLVCalibrationParameter_")
        .def(py::init([](const std::string& label, double lower, double upper, double scale) {
                 return GSRSLVCalibrationParameter_{String_(label), lower, upper, scale};
             }),
             py::arg("label"), py::arg("lower"), py::arg("upper"), py::arg("scale") = 1.0);
    py::class_<GSRSLVCalibrationSettings_>(m, "GSRSLVCalibrationSettings_")
        .def(py::init<>())
        .def_readwrite("solver", &GSRSLVCalibrationSettings_::solver_)
        .def_readwrite("pricing", &GSRSLVCalibrationSettings_::pricing_)
        .def_readwrite("validation", &GSRSLVCalibrationSettings_::validation_)
        .def_readwrite("validation_sigma", &GSRSLVCalibrationSettings_::validationSigma_)
        .def_readwrite("staged", &GSRSLVCalibrationSettings_::staged_)
        .def_readwrite("use_aad_jacobian", &GSRSLVCalibrationSettings_::useAADJacobian_);
    py::class_<GSRSLVCalibrationResult_>(m, "GSRSLVCalibrationResult_")
        .def_property_readonly("model",
                               [](const GSRSLVCalibrationResult_& result) {
                                   return std::const_pointer_cast<ModelData_>(std::static_pointer_cast<const ModelData_>(result.model_));
                               })
        .def_property_readonly("model_prices", [](const GSRSLVCalibrationResult_& r) { return List(r.modelPrices_); })
        .def_property_readonly("residuals", [](const GSRSLVCalibrationResult_& r) { return List(r.residuals_); })
        .def_property_readonly("standard_errors", [](const GSRSLVCalibrationResult_& r) { return List(r.standardErrors_); })
        .def_property_readonly("conditional_errors", [](const GSRSLVCalibrationResult_& r) { return List(r.conditionalErrors_); })
        .def_property_readonly("validation_conditional_errors",
                               [](const GSRSLVCalibrationResult_& r) { return List(r.validationConditionalErrors_); })
        .def_property_readonly("held_out_conditional_errors", [](const GSRSLVCalibrationResult_& r) { return List(r.heldOutConditionalErrors_); })
        .def_property_readonly("parameters", [](const GSRSLVCalibrationResult_& r) { return List(r.parameters_); })
        .def_property_readonly("active_bounds", [](const GSRSLVCalibrationResult_& r) { return List(r.activeBounds_); })
        .def_property_readonly("validation_prices", [](const GSRSLVCalibrationResult_& r) { return List(r.validationPrices_); })
        .def_property_readonly("validation_standard_errors", [](const GSRSLVCalibrationResult_& r) { return List(r.validationStandardErrors_); })
        .def_property_readonly("numerical_errors", [](const GSRSLVCalibrationResult_& r) { return List(r.numericalErrors_); })
        .def_property_readonly("held_out_prices", [](const GSRSLVCalibrationResult_& r) { return List(r.heldOutPrices_); })
        .def_property_readonly("held_out_residuals", [](const GSRSLVCalibrationResult_& r) { return List(r.heldOutResiduals_); })
        .def_property_readonly("held_out_standard_errors", [](const GSRSLVCalibrationResult_& r) { return List(r.heldOutStandardErrors_); })
        .def_readonly("quote_jacobian", &GSRSLVCalibrationResult_::quoteJacobian_)
        .def_readonly("converged", &GSRSLVCalibrationResult_::converged_)
        .def_readonly("fit_within_tolerance", &GSRSLVCalibrationResult_::fitWithinTolerance_)
        .def_readonly("numerical_validation_passed", &GSRSLVCalibrationResult_::numericalValidationPassed_)
        .def_readonly("held_out_within_tolerance", &GSRSLVCalibrationResult_::heldOutWithinTolerance_)
        .def_readonly("iterations", &GSRSLVCalibrationResult_::iterations_)
        .def_readonly("evaluations", &GSRSLVCalibrationResult_::evaluations_)
        .def_readonly("jacobian_rank", &GSRSLVCalibrationResult_::jacobianRank_)
        .def_readonly("jacobian_condition_estimate", &GSRSLVCalibrationResult_::jacobianConditionEstimate_)
        .def_readonly("objective", &GSRSLVCalibrationResult_::objective_)
        .def_property_readonly("termination_reason", [](const GSRSLVCalibrationResult_& r) { return std::string(r.terminationReason_.c_str()); });
    py::class_<GSRSLVQuoteRiskSettings_>(m, "GSRSLVQuoteRiskSettings_")
        .def(py::init<>())
        .def_readwrite("relative_bump", &GSRSLVQuoteRiskSettings_::relativeBump_)
        .def_readwrite("absolute_bump", &GSRSLVQuoteRiskSettings_::absoluteBump_)
        .def_readwrite("stability_tolerance", &GSRSLVQuoteRiskSettings_::stabilityTolerance_);
    py::class_<GSRSLVQuoteRiskResult_>(m, "GSRSLVQuoteRiskResult_")
        .def_readonly("calibration", &GSRSLVQuoteRiskResult_::calibration_)
        .def_property_readonly("quote_names", [](const GSRSLVQuoteRiskResult_& r) { return List(r.quoteNames_); })
        .def_property_readonly("quote_units", [](const GSRSLVQuoteRiskResult_& r) { return List(r.quoteUnits_); })
        .def_property_readonly("prices", [](const GSRSLVQuoteRiskResult_& r) { return List(r.prices_); })
        .def_property_readonly("stable", [](const GSRSLVQuoteRiskResult_& r) { return List(r.stable_); })
        .def_property_readonly("active_set_stable", [](const GSRSLVQuoteRiskResult_& r) { return List(r.activeSetStable_); })
        .def_readonly("sensitivities", &GSRSLVQuoteRiskResult_::sensitivities_)
        .def_readonly("refinement_errors", &GSRSLVQuoteRiskResult_::refinementErrors_)
        .def_readonly("curve_risk_included", &GSRSLVQuoteRiskResult_::curveRiskIncluded_);
    py::class_<GSRCurveQuoteRisk_>(m, "GSRCurveQuoteRisk_")
        .def_property_readonly("quote_names", [](const GSRCurveQuoteRisk_& r) { return List(r.QuoteNames()); })
        .def_property_readonly("quote_units", [](const GSRCurveQuoteRisk_& r) { return List(r.QuoteUnits()); })
        .def_property_readonly("log_df_quote_jacobian", [](const GSRCurveQuoteRisk_& r) { return Matrix_<>(r.LogDFQuoteJacobian()); });
    m.def(
        "GSRSLV_EuropeanOptionPrices",
        [](const std::shared_ptr<ModelData_>& model, const std::vector<EuropeanRateOption_>& options, GSRMonteCarloSettings_ settings) {
            Vector_<GSRMonteCarloPrice_> result;
            {
                py::gil_scoped_release release;
                result = PriceGSRSLVEuropeanOptions(Model(model), {options.begin(), options.end()}, settings);
            }
            return List(result);
        },
        py::arg("model"), py::arg("options"), py::arg("settings") = GSRMonteCarloSettings_{});
    m.def(
        "Calibrate_GSRSLV",
        [](const std::shared_ptr<ModelData_>& initial, const std::vector<CalibrationQuote_>& quotes,
           const std::vector<GSRSLVCalibrationParameter_>& parameters, GSRSLVCalibrationSettings_ settings,
           const std::vector<CalibrationQuote_>& heldOut) {
            py::gil_scoped_release release;
            return CalibrateGSRSLV(Model(initial), {quotes.begin(), quotes.end()}, {parameters.begin(), parameters.end()}, settings,
                                   {heldOut.begin(), heldOut.end()});
        },
        py::arg("initial"), py::arg("quotes"), py::arg("parameters"), py::arg("settings") = GSRSLVCalibrationSettings_{},
        py::arg("held_out") = std::vector<CalibrationQuote_>{});
    m.def(
        "GSRSLV_QuoteRisk",
        [](const std::shared_ptr<ModelData_>& initial, const std::vector<CalibrationQuote_>& quotes,
           const std::vector<GSRSLVCalibrationParameter_>& parameters, const std::vector<EuropeanRateOption_>& targets,
           GSRSLVCalibrationSettings_ settings, GSRSLVQuoteRiskSettings_ riskSettings, const GSRCurveQuoteRisk_* curveRisk) {
            py::gil_scoped_release release;
            return GSRSLVQuoteRisk(Model(initial), {quotes.begin(), quotes.end()}, {parameters.begin(), parameters.end()},
                                   {targets.begin(), targets.end()}, settings, riskSettings, curveRisk);
        },
        py::arg("initial"), py::arg("quotes"), py::arg("parameters"), py::arg("targets"), py::arg("settings") = GSRSLVCalibrationSettings_{},
        py::arg("risk_settings") = GSRSLVQuoteRiskSettings_{}, py::arg("curve_risk") = nullptr);
    m.def(
        "GSRCurveQuoteRisk_New",
        [](const std::shared_ptr<Storable_>& snapshot, const RatePricingMarket_& market, const RateQuoteRiskProvenance_& provenance,
           const std::string& discountComponent, const py::iterable& projectionComponents) {
            Vector_<String_> keys;
            for (const auto& key : projectionComponents)
                keys.push_back(String_(py::cast<std::string>(key)));
            return BuildGSRCurveQuoteRisk(Handle_<Storable_>(std::shared_ptr<const Storable_>(snapshot)), market, provenance,
                                          String_(discountComponent), keys);
        },
        py::arg("snapshot"), py::arg("market"), py::arg("provenance"), py::arg("discount_component"), py::arg("projection_components") = py::tuple{});

    py::class_<VolQuote_>(m, "VolQuote_")
        .def(py::init(
                 [](const std::string& name, const EuropeanRateOption_& option, double volatility, double priceScale, const std::string& convention,
                    double shift) { return VolQuote_{String_(name), option, volatility, priceScale, VolConvention_(String_(convention)), shift}; }),
             py::arg("name"), py::arg("option"), py::arg("volatility"), py::arg("price_scale"), py::arg("convention") = "NORMAL",
             py::arg("shift") = 0.0);
    py::class_<VolQuoteValue_>(m, "VolQuoteValue_")
        .def_readonly("forward", &VolQuoteValue_::forward_)
        .def_readonly("annuity", &VolQuoteValue_::annuity_)
        .def_readonly("price", &VolQuoteValue_::price_)
        .def_readonly("vega", &VolQuoteValue_::vega_);
    m.def(
        "VolQuotes_Get_Prices",
        [](const std::shared_ptr<Storable_>& snapshot, const std::vector<VolQuote_>& quotes) {
            return List(ConvertVolQuotes(Handle_<Storable_>(std::shared_ptr<const Storable_>(snapshot)), {quotes.begin(), quotes.end()}));
        },
        py::arg("snapshot"), py::arg("quotes"));
    m.def(
        "Calibrate_GSRSLVMarket",
        [](const std::shared_ptr<ModelData_>& initial, const std::vector<VolQuote_>& quotes,
           const std::vector<GSRSLVCalibrationParameter_>& parameters, GSRSLVCalibrationSettings_ settings, const std::vector<VolQuote_>& heldOut) {
            py::gil_scoped_release release;
            return CalibrateGSRSLVMarket(Model(initial), {quotes.begin(), quotes.end()}, {parameters.begin(), parameters.end()}, settings,
                                         {heldOut.begin(), heldOut.end()});
        },
        py::arg("initial"), py::arg("quotes"), py::arg("parameters"), py::arg("settings") = GSRSLVCalibrationSettings_{},
        py::arg("held_out") = std::vector<VolQuote_>{});
    m.def(
        "GSRSLV_MarketQuoteRisk",
        [](const std::shared_ptr<ModelData_>& initial, const std::vector<VolQuote_>& quotes,
           const std::vector<GSRSLVCalibrationParameter_>& parameters, const std::vector<EuropeanRateOption_>& targets,
           GSRSLVCalibrationSettings_ settings, GSRSLVQuoteRiskSettings_ riskSettings, const GSRCurveQuoteRisk_* curveRisk) {
            py::gil_scoped_release release;
            return GSRSLVMarketQuoteRisk(Model(initial), {quotes.begin(), quotes.end()}, {parameters.begin(), parameters.end()},
                                         {targets.begin(), targets.end()}, settings, riskSettings, curveRisk);
        },
        py::arg("initial"), py::arg("quotes"), py::arg("parameters"), py::arg("targets"), py::arg("settings") = GSRSLVCalibrationSettings_{},
        py::arg("risk_settings") = GSRSLVQuoteRiskSettings_{}, py::arg("curve_risk") = nullptr);
}

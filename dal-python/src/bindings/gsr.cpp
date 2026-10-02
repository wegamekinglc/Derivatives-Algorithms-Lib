//
// Created by Codex on 2026/10/2.
//

#include "bindings.h"

#include <pybind11/stl.h>

#include <dal-public/src/gsr.hpp>

using namespace Dal;

namespace {
    template <class T_> std::vector<T_> List(const Vector_<T_>& values) { return {values.begin(), values.end()}; }

    Handle_<ModelData_> Model(const std::shared_ptr<ModelData_>& model) { return Handle_<ModelData_>(std::shared_ptr<const ModelData_>(model)); }
} // namespace

void init_bindings_gsr(py::module_& m) {
    py::class_<GSRBondOption_>(m, "GSRBondOption_")
        .def(py::init([](const Date_& expiry, const Date_& maturity, double strike, const std::string& type) {
                 return GSRBondOption_{expiry, maturity, strike, OptionType_(String_(type))};
             }),
             py::arg("expiry"), py::arg("maturity"), py::arg("strike"), py::arg("type") = "CALL");
    py::class_<GSRFixedCoupon_>(m, "GSRFixedCoupon_")
        .def(py::init([](const Date_& payment, double accrual) { return GSRFixedCoupon_{payment, accrual}; }), py::arg("payment"),
             py::arg("accrual"));
    py::class_<GSRFloatingCoupon_>(m, "GSRFloatingCoupon_")
        .def(py::init([](const Date_& fixing, const Date_& start, const Date_& end, const Date_& payment, double indexAccrual, double couponAccrual,
                         const std::string& tenor) {
                 return GSRFloatingCoupon_{fixing, start, end, payment, indexAccrual, couponAccrual, String_(tenor)};
             }),
             py::arg("fixing"), py::arg("start"), py::arg("end"), py::arg("payment"), py::arg("index_accrual"), py::arg("coupon_accrual"),
             py::arg("tenor"));
    py::class_<GSRCaplet_>(m, "GSRCaplet_")
        .def(py::init([](const Date_& expiry, const Date_& start, const Date_& end, const Date_& payment, double indexAccrual, double couponAccrual,
                         const std::string& tenor, double strike, const std::string& type) {
                 return GSRCaplet_{expiry, start, end, payment, indexAccrual, couponAccrual, String_(tenor), strike, OptionType_(String_(type))};
             }),
             py::arg("expiry"), py::arg("start"), py::arg("end"), py::arg("payment"), py::arg("index_accrual"), py::arg("coupon_accrual"),
             py::arg("tenor"), py::arg("strike"), py::arg("type") = "CALL");
    py::class_<GSRSwaption_>(m, "GSRSwaption_")
        .def(py::init([](const Date_& expiry, const std::vector<GSRFixedCoupon_>& fixed, const std::vector<GSRFloatingCoupon_>& floating,
                         double strike, const std::string& type) {
                 return GSRSwaption_{expiry, {fixed.begin(), fixed.end()}, {floating.begin(), floating.end()}, strike, OptionType_(String_(type))};
             }),
             py::arg("expiry"), py::arg("fixed"), py::arg("floating"), py::arg("strike"), py::arg("type") = "CALL");
    py::class_<GSRPricingSettings_>(m, "GSRPricingSettings_")
        .def(py::init<>())
        .def_readwrite("quadrature_order", &GSRPricingSettings_::quadratureOrder_)
        .def_readwrite("estimate_error", &GSRPricingSettings_::estimateError_);
    py::class_<GSRPriceResult_>(m, "GSRPriceResult_")
        .def_readonly("price", &GSRPriceResult_::price_)
        .def_readonly("numerical_error", &GSRPriceResult_::numericalError_);
    py::class_<GSRCalibrationQuote_>(m, "GSRCalibrationQuote_")
        .def(py::init([](const std::string& name, const GSREuropeanOption_& option, double price, double priceScale) {
                 return GSRCalibrationQuote_{String_(name), option, price, priceScale};
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
        [](const std::shared_ptr<ModelData_>& model, const GSREuropeanOption_& option, const GSRPricingSettings_& settings) {
            return PriceGSREuropeanOption(Model(model), option, settings);
        },
        py::arg("model"), py::arg("option"), py::arg("settings") = GSRPricingSettings_{});
    m.def(
        "Calibrate_GSRVolatility",
        [](const std::shared_ptr<ModelData_>& initial, const std::vector<GSRCalibrationQuote_>& quotes,
           const std::vector<GSRCalibrationParameter_>& parameters, const GSRCalibrationSettings_& settings) {
            return CalibrateGSRVolatility(Model(initial), {quotes.begin(), quotes.end()}, {parameters.begin(), parameters.end()}, settings);
        },
        py::arg("initial"), py::arg("quotes"), py::arg("parameters"), py::arg("settings") = GSRCalibrationSettings_{});
}

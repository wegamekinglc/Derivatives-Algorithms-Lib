#include "bindings.h"

#include "calibrationinput.hpp"
#include "giltestbarrier.hpp"
#include <dal-public/src/xccymontecarlo.hpp>

using namespace Dal;
using namespace Dal::Python;
using namespace Dal::Python::Calibration;

#ifdef DAL_XCCY_TEST_BRIDGE
void init_xccy_mc_core_test(py::module_& m);
#endif

namespace Dal::Python::Calibration {
    template <> struct Input_<Matrix_<>> {
        static Matrix_<> Read(const py::handle& value, const std::string& field) {
            const auto rows = Input_<Vector_<Vector_<>>>::Read(value, field);
            const size_t columns = rows.empty() ? 0 : rows.front().size();
            Matrix_<> result(static_cast<int>(rows.size()), static_cast<int>(columns));
            for (size_t row = 0; row < rows.size(); ++row) {
                if (rows[row].size() != columns)
                    CalibrationInputFailure("InvalidCalibrationPlan", String_(field), "rectangular numeric matrix");
                for (size_t col = 0; col < columns; ++col)
                    result(static_cast<int>(row), static_cast<int>(col)) = rows[row][col];
            }
            return result;
        }
    };
} // namespace Dal::Python::Calibration

namespace {
    thread_local bool gilBarrierArmed = false;
    GilTestBarrier_ gilBarrier;
    void MarketTypes(py::module_& m) {
        auto market = Struct<XccyGaussianMarketSpec_>(m, "XccyGaussianMarketSpec_", [](Fields_& f) {
            XccyGaussianMarketSpec_ v;
            v.valuationTime_ = f.Required<DateTime_>("valuation_time");
            v.pair_ = f.Required<CurrencyPair_>("pair");
            v.collateralCurrency_ = f.Required<Ccy_>("collateral_currency");
            v.fxSpot_ = f.Required<double>("fx_spot");
            v.domesticBlock_ = f.Required<Handle_<CurveBlock_>>("domestic_block");
            v.foreignBlock_ = f.Required<Handle_<CurveBlock_>>("foreign_block");
            v.basis_ = f.Required<Handle_<DiscountCurve_>>("basis");
            v.discountRoute_ = f.Required<String_>("discount_route");
            return v;
        });
        Property(market, "valuation_time", &XccyGaussianMarketSpec_::valuationTime_);
        Property(market, "pair", &XccyGaussianMarketSpec_::pair_);
        Property(market, "collateral_currency", &XccyGaussianMarketSpec_::collateralCurrency_);
        Property(market, "fx_spot", &XccyGaussianMarketSpec_::fxSpot_);
        Property(market, "domestic_block", &XccyGaussianMarketSpec_::domesticBlock_);
        Property(market, "foreign_block", &XccyGaussianMarketSpec_::foreignBlock_);
        Property(market, "basis", &XccyGaussianMarketSpec_::basis_);
        Property(market, "discount_route", &XccyGaussianMarketSpec_::discountRoute_);
        auto dynamics = Struct<XccyGaussianDynamicsSpec_>(m, "XccyGaussianDynamicsSpec_", [](Fields_& f) {
            XccyGaussianDynamicsSpec_ v;
            v.domesticA_ = f.Required<double>("domestic_a");
            v.foreignA_ = f.Required<double>("foreign_a");
            v.bucketEndDates_ = f.Required<Vector_<Date_>>("bucket_end_dates");
            v.domesticSigma_ = f.Required<Vector_<>>("domestic_sigma");
            v.foreignSigma_ = f.Required<Vector_<>>("foreign_sigma");
            v.fxSigma_ = f.Required<Vector_<>>("fx_sigma");
            v.factorNames_ = f.Required<Vector_<String_>>("factor_names");
            v.correlation_ = f.Required<Matrix_<>>("correlation");
            return v;
        });
        Property(dynamics, "domestic_a", &XccyGaussianDynamicsSpec_::domesticA_);
        Property(dynamics, "foreign_a", &XccyGaussianDynamicsSpec_::foreignA_);
        Property(dynamics, "bucket_end_dates", &XccyGaussianDynamicsSpec_::bucketEndDates_);
        Property(dynamics, "domestic_sigma", &XccyGaussianDynamicsSpec_::domesticSigma_);
        Property(dynamics, "foreign_sigma", &XccyGaussianDynamicsSpec_::foreignSigma_);
        Property(dynamics, "fx_sigma", &XccyGaussianDynamicsSpec_::fxSigma_);
        Property(dynamics, "factor_names", &XccyGaussianDynamicsSpec_::factorNames_);
        dynamics.def_property_readonly("correlation", [](const XccyGaussianDynamicsSpec_& v) {
            py::list rows;
            for (int i = 0; i < v.correlation_.Rows(); ++i) {
                py::list row;
                for (int j = 0; j < v.correlation_.Cols(); ++j)
                    row.append(v.correlation_(i, j));
                rows.append(row);
            }
            return rows;
        });
        auto spec = Struct<XccyGaussianModelSpec_>(m, "XccyGaussianModelSpec_", [](Fields_& f) {
            return XccyGaussianModelSpec_{f.Required<XccyGaussianMarketSpec_>("market"), f.Required<XccyGaussianDynamicsSpec_>("dynamics")};
        });
        Property(spec, "market", &XccyGaussianModelSpec_::market_);
        Property(spec, "dynamics", &XccyGaussianModelSpec_::dynamics_);
        py::class_<XccyGaussianDynamics_, Storable_, std::shared_ptr<XccyGaussianDynamics_>>(m, "XccyGaussianDynamics_");
        py::class_<XccyGaussianModelData_, ModelData_, std::shared_ptr<XccyGaussianModelData_>>(m, "XccyGaussianModelData_")
            .def_property_readonly("identity", [](const XccyGaussianModelData_& v) { return Text(v.Identity()); });
    }
    void RequestTypes(py::module_& m) {
        auto request = Struct<XccyMonteCarloRequest_>(m, "XccyMonteCarloRequest_", [](Fields_& f) {
            XccyMonteCarloRequest_ v;
            v.measureId_ = f.Required<String_>("measure_id");
            v.phase_ = ParseXccyMonteCarloPhase(f.Required<String_>("phase"));
            v.sampling_ = f.Required<CalibrationMonteCarloSampling_>("sampling");
            v.fixings_ = f.Maybe<Handle_<MarketFixingSnapshot_>>("fixings").value_or(Handle_<MarketFixingSnapshot_>());
            v.simulation_ = f.Default<CalibrationSimulationSettings_>("simulation", v.simulation_);
            v.derivativeMethod_ = f.Default<CalibrationDerivative_>("derivative_method", v.derivativeMethod_);
            v.parameterLabels_ = f.Default<Vector_<String_>>("parameter_labels", {});
            return v;
        });
        Property(request, "measure_id", &XccyMonteCarloRequest_::measureId_);
        Property(request, "phase", &XccyMonteCarloRequest_::phase_);
        Property(request, "sampling", &XccyMonteCarloRequest_::sampling_);
        Property(request, "fixings", &XccyMonteCarloRequest_::fixings_);
        Property(request, "simulation", &XccyMonteCarloRequest_::simulation_);
        Property(request, "derivative_method", &XccyMonteCarloRequest_::derivativeMethod_);
        Property(request, "parameter_labels", &XccyMonteCarloRequest_::parameterLabels_);
        auto result = py::class_<XccyMonteCarloResult_, CalibrationMonteCarloResult_>(m, "XccyMonteCarloResult_");
        result.def_property_readonly("details_json", [](const XccyMonteCarloResult_& v) { return Text(DumpXccyMonteCarloResultJson(v)); });
        result.def_property_readonly("parameter_labels",
                                     [](const XccyMonteCarloResult_& v) { return Output(XccyMonteCarloDetails(v).estimate_.parameterLabels_); });
        result.def_property_readonly("gradient", [](const XccyMonteCarloResult_& v) { return Output(XccyMonteCarloDetails(v).estimate_.gradient_); });
        result.def_property_readonly("gradient_standard_error", [](const XccyMonteCarloResult_& v) {
            return Output(XccyMonteCarloDetails(v).estimate_.gradientStandardError_);
        });
        auto fast = py::class_<XccyGaussianValue_>(m, "XccyGaussianValue_");
        Property(fast, "a", &XccyGaussianValue_::a_);
        Property(fast, "b", &XccyGaussianValue_::b_);
        Property(fast, "value", &XccyGaussianValue_::value_);
        Property(fast, "report", &XccyGaussianValue_::report_);
    }
    void ValidationTypes(py::module_& m) {
        Struct<CalibrationMcReference_>(m, "XccyMonteCarloReference_", [](Fields_& f) {
            CalibrationMcReference_ v;
            v.value_ = f.Required<double>("value");
            v.numericalError_ = f.Required<double>("numerical_error");
            v.standardError_ = f.Default<double>("standard_error", 0.);
            v.gradient_ = f.Default<Vector_<>>("gradient", {});
            v.gradientStandardError_ = f.Default<Vector_<>>("gradient_standard_error", {});
            v.finiteDifferenceError_ = f.Default<Vector_<>>("finite_difference_error", {});
            return v;
        });
        Struct<CalibrationMcPrecision_>(m, "XccyMonteCarloPrecision_", [](Fields_& f) {
            CalibrationMcPrecision_ v;
            v.errorScale_ = f.Required<double>("error_scale");
            v.sigma_ = f.Default<double>("sigma", v.sigma_);
            v.numericalErrorFraction_ = f.Default<double>("numerical_error_fraction", v.numericalErrorFraction_);
            v.epsilonValue_ = f.Default<double>("epsilon_value", v.epsilonValue_);
            v.gradientErrorFraction_ = f.Default<double>("gradient_error_fraction", v.gradientErrorFraction_);
            v.parameterScales_ = f.Default<Vector_<>>("parameter_scales", {});
            v.epsilonGradient_ = f.Default<Vector_<>>("epsilon_gradient", {});
            return v;
        });
        auto validation = py::class_<XccyMcValidation_>(m, "XccyMonteCarloValidation_");
        validation.def_property_readonly("report", [](const XccyMcValidation_& v) { return Output(v.validation_.report_); });
        validation.def_property_readonly("gradient_combined_passed", [](const XccyMcValidation_& v) {
            return Output(v.validation_.gradientValidationRequested_ ? std::optional<bool>(v.gradientCombinedPassed_) : std::nullopt);
        });
        for (const auto& field : std::initializer_list<std::pair<const char*, bool CalibrationMcValidation_::*>>{
                 {"parity_passed", &CalibrationMcValidation_::parityPassed_},
                 {"precision_passed", &CalibrationMcValidation_::precisionPassed_},
                 {"value_error_passed", &CalibrationMcValidation_::valueErrorPassed_},
                 {"gradient_parity_passed", &CalibrationMcValidation_::gradientParityPassed_},
                 {"gradient_precision_passed", &CalibrationMcValidation_::gradientPrecisionPassed_},
                 {"numerical_validation_passed", &CalibrationMcValidation_::numericalValidationPassed_},
                 {"budget_exhausted", &CalibrationMcValidation_::budgetExhausted_}})
            validation.def_property_readonly(field.first, [member = field.second,
                                                           gradient = std::string(field.first).find("gradient_") == 0](const XccyMcValidation_& v) {
                return Output(gradient && !v.validation_.gradientValidationRequested_ ? std::nullopt : std::optional<bool>(v.validation_.*member));
            });
    }
} // namespace

void init_xccy_monte_carlo(py::module_& m) {
#ifdef DAL_XCCY_TEST_BRIDGE
    init_xccy_mc_core_test(m);
#endif
    MarketTypes(m);
    RequestTypes(m);
    ValidationTypes(m);
    m.def("_XccyMcGilBarrier_ArmForTesting", [] { gilBarrier.Arm(&gilBarrierArmed); });
    m.def("_XccyMcGilBarrier_AwaitEntryForTesting", [] {
        py::gil_scoped_release release;
        return gilBarrier.AwaitEntry();
    });
    m.def("_XccyMcGilBarrier_ReleaseForTesting", [] { return gilBarrier.Release(); });
    m.def("_XccyMcGilBarrier_BlockWithGilForTesting", [] { gilBarrier.Wait(&gilBarrierArmed, "XCCY MC GIL barrier timed out"); });
    m.def(
        "XccyGaussianModel_New",
        [](const py::object& name, const py::object& spec) {
            const auto label = Input_<String_>::Read(name, "/name");
            const auto input = Input_<XccyGaussianModelSpec_>::Read(spec, "/spec");
            py::gil_scoped_release release;
            return std::const_pointer_cast<XccyGaussianModelData_>(NewXccyGaussianModel(label, input));
        },
        py::arg("name"), py::arg("spec"));
    m.def(
        "XccyGaussianDynamics_New",
        [](const py::object& name, const py::object& spec) {
            const auto label = Input_<String_>::Read(name, "/name");
            const auto input = Input_<XccyGaussianDynamicsSpec_>::Read(spec, "/spec");
            py::gil_scoped_release release;
            return std::make_shared<XccyGaussianDynamics_>(label, input);
        },
        py::arg("name"), py::arg("spec"));
    m.def(
        "XccyInstrument_ValueByMonteCarlo",
        [](const py::object& instrument, const py::object& model, const py::object& request) {
            const auto contract = Input_<Handle_<CalibrationInstrument_>>::Read(instrument, "/instrument");
            const auto data = Input_<Handle_<XccyGaussianModelData_>>::Read(model, "/model");
            const auto input = Input_<XccyMonteCarloRequest_>::Read(request, "/request");
            py::gil_scoped_release release;
            gilBarrier.Wait(&gilBarrierArmed, "XCCY MC GIL barrier timed out");
            return ValueXccyInstrumentByMonteCarlo(contract, data, input);
        },
        py::arg("instrument"), py::arg("model"), py::arg("request"));
    m.def(
        "XccyInstrument_ValueByGaussian",
        [](const py::object& instrument, const py::object& model, const py::object& measure, const py::object& fixings) {
            const auto contract = Input_<Handle_<CalibrationInstrument_>>::Read(instrument, "/instrument");
            const auto data = Input_<Handle_<XccyGaussianModelData_>>::Read(model, "/model");
            const auto selected = Input_<String_>::Read(measure, "/measure_id");
            const auto history =
                fixings.is_none() ? Handle_<MarketFixingSnapshot_>() : Input_<Handle_<MarketFixingSnapshot_>>::Read(fixings, "/fixings");
            py::gil_scoped_release release;
            return ValueXccyInstrumentByGaussian(contract, data, selected, history);
        },
        py::arg("instrument"), py::arg("model"), py::arg("measure_id"), py::kw_only(), py::arg("fixings") = py::none());
    m.def(
        "XccyMonteCarloResult_Validate",
        [](const XccyMonteCarloResult_& result, const CalibrationMcReference_& reference, const CalibrationMcPrecision_& precision) {
            const auto owned = result;
            const auto target = reference;
            const auto budgets = precision;
            py::gil_scoped_release release;
            return ValidateXccyMonteCarlo(owned, target, budgets);
        },
        py::arg("result"), py::arg("reference"), py::arg("precision"));
    m.def(
        "CalibrationInstrument_Get_XccyGaussianCapabilities",
        [](const py::object& instrument, const py::object& model, const py::object& request) {
            const auto contract = Input_<Handle_<CalibrationInstrument_>>::Read(instrument, "/instrument");
            const auto data = Input_<Handle_<XccyGaussianModelData_>>::Read(model, "/model");
            const auto input = Input_<XccyMonteCarloRequest_>::Read(request, "/request");
            py::gil_scoped_release release;
            return std::const_pointer_cast<CalibrationReport_>(XccyGaussianInstrumentCapabilities(contract, data, input));
        },
        py::arg("instrument"), py::arg("model"), py::arg("request"));
}

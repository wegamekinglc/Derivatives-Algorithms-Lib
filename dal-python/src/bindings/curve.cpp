//
// curve.cpp — curve calibration bindings
//

#include "bindings.h"

#include <pybind11/stl.h>

#include <dal/math/matrix/matrixs.hpp>
#include <dal/curve/calibration.hpp>
#include <dal/curve/xccycalibration.hpp>
#include <dal/protocol/collateraltype.hpp>
#include <dal/time/daybasis.hpp>
#include <dal/time/periodlength.hpp>

#include <dal-public/src/curveprotocol.hpp>
#include <dal-public/src/curveinstrument.hpp>
#include <dal-public/src/curvedata.hpp>
#include <dal-public/src/curvespec.hpp>
#include <dal-public/src/xccycalibration.hpp>

using namespace Dal;

namespace {
    void init_bindings_curve_handles(py::module_& m) {
        py::class_<YCInstrument_, std::shared_ptr<YCInstrument_>>(m, "YCInstrument_");
        py::class_<DiscountCurve_, std::shared_ptr<DiscountCurve_>>(m, "DiscountCurve_");
        py::class_<CurveBlock_, std::shared_ptr<CurveBlock_>>(m, "CurveBlock_");
        py::class_<CrossCurrencySwap_, std::shared_ptr<CrossCurrencySwap_>>(m, "CrossCurrencySwap_");
        py::class_<CrossCurrencyMarket_, std::shared_ptr<CrossCurrencyMarket_>>(m, "CrossCurrencyMarket_");

        // Value types used as function parameters / return values
        py::class_<CurveCalibrationSpec_>(m, "CurveCalibrationSpec_");
        py::class_<CrossCurrencyCalibrationSpec_>(m, "CrossCurrencyCalibrationSpec_");
    }

    void init_bindings_curve_protocol(py::module_& m) {
        py::class_<CollateralType_>(m, "CollateralType_")
            .def(py::init<const char*>(), py::arg("src"))
            .def("__repr__", [](const CollateralType_& ct) { return std::string(ct.String()); });

        m.def("CollateralType_OIS", &CollateralType_OIS);
        m.def("CollateralType_Libor", &CollateralType_Libor, py::arg("tenor"));

        py::class_<PeriodLength_>(m, "PeriodLength_")
            .def(py::init<const char*>(), py::arg("iso"))
            .def("__repr__", [](const PeriodLength_& pl) { return std::string(pl.String()); });

        m.def("PeriodLength_New", [](const char* iso) { return PeriodLength_New(String_(iso)); },
              py::arg("iso"));

        py::class_<DayBasis_>(m, "DayBasis_")
            .def(py::init<const char*>(), py::arg("name"))
            .def("__repr__", [](const DayBasis_& db) { return std::string(db.String()); });

        m.def("DayBasis_New", [](const char* name) { return DayBasis_New(String_(name)); },
              py::arg("name"));

        py::class_<RateLegConvention_>(m, "RateLegConvention_")
            .def(py::init<>());

        m.def("RateLegConvention_New", &RateLegConvention_New,
              py::arg("freq"), py::arg("basis"));

        py::class_<RateIndexConvention_>(m, "RateIndexConvention_")
            .def(py::init<>());

        m.def("RateIndexConvention_New", &RateIndexConvention_New,
              py::arg("forecast_tenor"), py::arg("basis"), py::arg("collateral"),
              py::arg("use_projection_curve") = false);

        py::class_<CurrencyPair_>(m, "CurrencyPair_");

        m.def("CurrencyPair_New",
              [](const char* domestic, const char* foreign) {
                  return CurrencyPair_New(String_(domestic), String_(foreign));
              },
              py::arg("domestic"), py::arg("foreign"));
    }

    void init_bindings_curve_instruments(py::module_& m) {
        m.def("Deposit_New",
            [](const Date_& tradeDate, const Date_& start, const Date_& maturity,
               double marketRate, const RateIndexConvention_& convention)
               -> std::shared_ptr<YCInstrument_> {
                return std::const_pointer_cast<YCInstrument_>(
                    Handle_<YCInstrument_>(DepositNew(tradeDate, start, maturity, marketRate, convention)));
            },
            py::arg("trade_date"), py::arg("start"), py::arg("maturity"),
            py::arg("market_rate"), py::arg("convention"));

        m.def("FRA_New",
            [](const Date_& tradeDate, const Date_& start, const Date_& maturity,
               double marketRate, const RateIndexConvention_& convention)
               -> std::shared_ptr<YCInstrument_> {
                return std::const_pointer_cast<YCInstrument_>(
                    Handle_<YCInstrument_>(FRANew(tradeDate, start, maturity, marketRate, convention)));
            },
            py::arg("trade_date"), py::arg("start"), py::arg("maturity"),
            py::arg("market_rate"), py::arg("convention"));

        m.def("Future_New",
            [](const Date_& tradeDate, const Date_& start, const Date_& maturity,
               double marketRate, const RateIndexConvention_& convention,
               double convexityAdjustment) -> std::shared_ptr<YCInstrument_> {
                return std::const_pointer_cast<YCInstrument_>(
                    Handle_<YCInstrument_>(FutureNew(tradeDate, start, maturity, marketRate, convention, convexityAdjustment)));
            },
            py::arg("trade_date"), py::arg("start"), py::arg("maturity"),
            py::arg("market_rate"), py::arg("convention"),
            py::arg("convexity_adjustment") = 0.0);

        m.def("Swap_New",
            [](const Date_& tradeDate, const Date_& start, const Date_& maturity,
               double marketRate,
               const RateLegConvention_& fixedLeg,
               const RateIndexConvention_& floatIndex,
               const RateLegConvention_& floatLeg)
               -> std::shared_ptr<YCInstrument_> {
                return std::const_pointer_cast<YCInstrument_>(
                    Handle_<YCInstrument_>(SwapNew(tradeDate, start, maturity, marketRate, fixedLeg, floatIndex, floatLeg)));
            },
            py::arg("trade_date"), py::arg("start"), py::arg("maturity"),
            py::arg("market_rate"), py::arg("fixed_leg"),
            py::arg("float_index"), py::arg("float_leg"));

        m.def("OISSwap_New",
            [](const Date_& tradeDate, const Date_& start, const Date_& maturity,
               double marketRate,
               const RateLegConvention_& fixedLeg,
               const RateIndexConvention_& overnightIndex,
               const RateLegConvention_& floatLeg)
               -> std::shared_ptr<YCInstrument_> {
                return std::const_pointer_cast<YCInstrument_>(
                    Handle_<YCInstrument_>(OISSwapNew(tradeDate, start, maturity, marketRate, fixedLeg, overnightIndex, floatLeg)));
            },
            py::arg("trade_date"), py::arg("start"), py::arg("maturity"),
            py::arg("market_rate"), py::arg("fixed_leg"),
            py::arg("overnight_index"), py::arg("float_leg"));

        m.def("BasisSwap_New",
            [](const Date_& tradeDate, const Date_& start, const Date_& maturity,
               double marketRate,
               const RateIndexConvention_& spreadIndex,
               const RateLegConvention_& spreadLeg,
               const RateIndexConvention_& refIndex,
               const RateLegConvention_& refLeg)
               -> std::shared_ptr<YCInstrument_> {
                return std::const_pointer_cast<YCInstrument_>(
                    Handle_<YCInstrument_>(BasisSwapNew(tradeDate, start, maturity, marketRate, spreadIndex, spreadLeg, refIndex, refLeg)));
            },
            py::arg("trade_date"), py::arg("start"), py::arg("maturity"),
            py::arg("market_rate"), py::arg("spread_index"), py::arg("spread_leg"),
            py::arg("ref_index"), py::arg("ref_leg"));

        m.def("CrossCurrencySwap_New",
            [](const Date_& tradeDate, const Date_& start, const Date_& maturity,
               double marketRate,
               const CurrencyPair_& currencies,
               double domesticNotional, double foreignNotional,
               const RateLegConvention_& domesticLeg,
               const RateIndexConvention_& domesticIndex,
               const RateLegConvention_& foreignLeg,
               const RateIndexConvention_& foreignIndex)
               -> std::shared_ptr<CrossCurrencySwap_> {
                return std::const_pointer_cast<CrossCurrencySwap_>(
                    Handle_<CrossCurrencySwap_>(CrossCurrencySwapNew(
                        tradeDate, start, maturity, marketRate, currencies,
                        domesticNotional, foreignNotional,
                        domesticLeg, domesticIndex, foreignLeg, foreignIndex)));
            },
            py::arg("trade_date"), py::arg("start"), py::arg("maturity"),
            py::arg("market_rate"), py::arg("currencies"),
            py::arg("domestic_notional") = 100.0,
            py::arg("foreign_notional") = 100.0,
            py::arg("domestic_leg") = RateLegConvention_(),
            py::arg("domestic_index") = RateIndexConvention_(),
            py::arg("foreign_leg") = RateLegConvention_(),
            py::arg("foreign_index") = RateIndexConvention_());
    }

    void init_bindings_curve_data(py::module_& m) {
        m.def("DiscountPWLF_New",
            [](const char* name, const char* ccy,
               const py::iterable& knotDatesPy,
               const py::iterable& fwdRatesPy,
               const std::shared_ptr<DiscountCurve_>& base)
               -> std::shared_ptr<DiscountCurve_> {
                Vector_<Date_> knotDates;
                for (auto item : knotDatesPy)
                    knotDates.push_back(py::cast<Date_>(item));
                Vector_<> fwdRates;
                for (auto item : fwdRatesPy)
                    fwdRates.push_back(py::cast<double>(item));
                Handle_<DiscountCurve_> baseHandle(
                    std::const_pointer_cast<const DiscountCurve_>(base));
                return std::const_pointer_cast<DiscountCurve_>(
                    DiscountPWLFNew(String_(name), String_(ccy), knotDates, fwdRates, baseHandle));
            },
            py::arg("name"), py::arg("ccy"),
            py::arg("knot_dates"), py::arg("fwd_rates"),
            py::arg("base") = std::shared_ptr<DiscountCurve_>());

        m.def("CurveBlock_New",
            [](const std::shared_ptr<DiscountCurve_>& dc,
               const DayBasis_& liborBasis)
               -> std::shared_ptr<CurveBlock_> {
                Handle_<DiscountCurve_> dcHandle(
                    std::const_pointer_cast<const DiscountCurve_>(dc));
                return std::const_pointer_cast<CurveBlock_>(
                    CurveBlockNew(dcHandle, liborBasis));
            },
            py::arg("discount_curve"),
            py::arg("libor_basis") = DayBasis_("ACT_365F"));

        m.def("CurveBlock_New",
            [](const char* name, const char* ccy,
               const std::map<CollateralType_, std::shared_ptr<DiscountCurve_>>& discountsPy,
               const std::map<PeriodLength_, std::shared_ptr<DiscountCurve_>>& forwardsPy,
               const DayBasis_& liborBasis)
               -> std::shared_ptr<CurveBlock_> {
                std::map<CollateralType_, Handle_<DiscountCurve_>> discounts;
                for (auto& kv : discountsPy)
                    discounts[kv.first] = Handle_<DiscountCurve_>(
                        std::const_pointer_cast<const DiscountCurve_>(kv.second));
                std::map<PeriodLength_, Handle_<DiscountCurve_>> forwards;
                for (auto& kv : forwardsPy)
                    forwards[kv.first] = Handle_<DiscountCurve_>(
                        std::const_pointer_cast<const DiscountCurve_>(kv.second));
                return std::const_pointer_cast<CurveBlock_>(
                    CurveBlockNew(String_(name), String_(ccy), discounts, forwards, liborBasis));
            },
            py::arg("name"), py::arg("ccy"),
            py::arg("discounts"), py::arg("forwards"),
            py::arg("libor_basis"));
    }

    void init_bindings_curve_enums(py::module_& m) {
        py::enum_<CurveSolveMode_::Value_>(m, "CurveSolveMode")
            .value("EXACT", CurveSolveMode_::Value_::EXACT)
            .value("APPROXIMATE", CurveSolveMode_::Value_::APPROXIMATE);

        py::enum_<CurveParameterization_::Value_>(m, "CurveParameterization")
            .value("PIECEWISE_LINEAR_FWD", CurveParameterization_::Value_::PIECEWISE_LINEAR_FWD)
            .value("PIECEWISE_CONSTANT_FWD", CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD)
            .value("ZERO_RATE", CurveParameterization_::Value_::ZERO_RATE)
            .value("LOG_DISCOUNT", CurveParameterization_::Value_::LOG_DISCOUNT);

        py::enum_<CurveJacobianMode_::Value_>(m, "CurveJacobianMode")
            .value("BUMPED", CurveJacobianMode_::Value_::BUMPED)
            .value("ANALYTIC", CurveJacobianMode_::Value_::ANALYTIC);

        py::enum_<LogDfScheme_::Value_>(m, "LogDfScheme")
            .value("LOG_LINEAR", LogDfScheme_::Value_::LOG_LINEAR)
            .value("LOG_CUBIC_NATURAL", LogDfScheme_::Value_::LOG_CUBIC_NATURAL)
            .value("MIXED", LogDfScheme_::Value_::MIXED);
    }

    void init_bindings_curve_calibration_builder(py::module_& m) {
        py::class_<CurveCalibrationSpecBuilder_>(m, "CurveCalibrationSpecBuilder_")
            .def(py::init<>())
            .def_readwrite("today_", &CurveCalibrationSpecBuilder_::today_)
            .def_readwrite("ccy_", &CurveCalibrationSpecBuilder_::ccy_)
            .def_readwrite("curveName_", &CurveCalibrationSpecBuilder_::curveName_)
            .def_readwrite("targetCollateral_", &CurveCalibrationSpecBuilder_::targetCollateral_)
            .def_readwrite("targetTenor_", &CurveCalibrationSpecBuilder_::targetTenor_)
            .def_readwrite("calibrateDiscountCurve_", &CurveCalibrationSpecBuilder_::calibrateDiscountCurve_)
            .def_readwrite("liborBasis_", &CurveCalibrationSpecBuilder_::liborBasis_)
            .def_readwrite("smoothingWeight_", &CurveCalibrationSpecBuilder_::smoothingWeight_)
            .def_readwrite("tolerance_", &CurveCalibrationSpecBuilder_::tolerance_)
            .def_readwrite("fitTolerance_", &CurveCalibrationSpecBuilder_::fitTolerance_)
            .def_readwrite("maxEvaluations_", &CurveCalibrationSpecBuilder_::maxEvaluations_)
            .def_readwrite("maxRestarts_", &CurveCalibrationSpecBuilder_::maxRestarts_)
            .def_readwrite("initialGuess_", &CurveCalibrationSpecBuilder_::initialGuess_)
            .def_property("solveMode_",
                [](const CurveCalibrationSpecBuilder_& b) { return b.solveMode_.Switch(); },
                [](CurveCalibrationSpecBuilder_& b, CurveSolveMode_::Value_ v) {
                    b.solveMode_ = CurveSolveMode_(v);
                })
            .def_property("parameterization_",
                [](const CurveCalibrationSpecBuilder_& b) { return b.parameterization_.Switch(); },
                [](CurveCalibrationSpecBuilder_& b, CurveParameterization_::Value_ v) {
                    b.parameterization_ = CurveParameterization_(v);
                })
            .def_property("logDfScheme_",
                [](const CurveCalibrationSpecBuilder_& b) { return b.logDfScheme_.Switch(); },
                [](CurveCalibrationSpecBuilder_& b, LogDfScheme_::Value_ v) {
                    b.logDfScheme_ = LogDfScheme_(v);
                })
            .def_property("instruments_",
                [](const CurveCalibrationSpecBuilder_& b) -> py::list {
                    py::list result;
                    for (auto& inst : b.instruments_)
                        result.append(std::const_pointer_cast<YCInstrument_>(
                            Handle_<YCInstrument_>(inst)));
                    return result;
                },
                [](CurveCalibrationSpecBuilder_& b, const py::iterable& instruments) {
                    b.instruments_.clear();
                    for (auto item : instruments) {
                        auto handle = py::cast<std::shared_ptr<YCInstrument_>>(item);
                        b.instruments_.push_back(Handle_<YCInstrument_>(
                            std::const_pointer_cast<const YCInstrument_>(handle)));
                    }
                })
            .def_property("knotDates_",
                [](const CurveCalibrationSpecBuilder_& b) -> py::list {
                    py::list result;
                    for (auto& d : b.knotDates_)
                        result.append(d);
                    return result;
                },
                [](CurveCalibrationSpecBuilder_& b, const py::iterable& dates) {
                    b.knotDates_.clear();
                    for (auto item : dates)
                        b.knotDates_.push_back(py::cast<Date_>(item));
                })
            .def_property("discountCurves_",
                nullptr,
                [](CurveCalibrationSpecBuilder_& b,
                   const std::map<CollateralType_, std::shared_ptr<DiscountCurve_>>& curves) {
                    b.discountCurves_.clear();
                    for (auto& kv : curves)
                        b.discountCurves_[kv.first] = Handle_<DiscountCurve_>(
                            std::const_pointer_cast<const DiscountCurve_>(kv.second));
                })
            .def_property("forwardCurves_",
                nullptr,
                [](CurveCalibrationSpecBuilder_& b,
                   const std::map<PeriodLength_, std::shared_ptr<DiscountCurve_>>& curves) {
                    b.forwardCurves_.clear();
                    for (auto& kv : curves)
                        b.forwardCurves_[kv.first] = Handle_<DiscountCurve_>(
                            std::const_pointer_cast<const DiscountCurve_>(kv.second));
                })
            .def_property("baseCurve_",
                nullptr,
                [](CurveCalibrationSpecBuilder_& b,
                   const std::shared_ptr<DiscountCurve_>& curve) {
                    b.baseCurve_ = Handle_<DiscountCurve_>(
                        std::const_pointer_cast<const DiscountCurve_>(curve));
                })
            .def("Build", &CurveCalibrationSpecBuilder_::Build);
    }

    void init_bindings_curve_calibration_diagnostics(py::module_& m) {
        py::class_<CurveCalibrationDiagnostics_>(m, "CurveCalibrationDiagnostics_")
            .def_property_readonly("curveName_", [](const CurveCalibrationDiagnostics_& d) { return d.curveName_; })
            .def_property_readonly("marketRates_", [](const CurveCalibrationDiagnostics_& d) -> py::list {
                py::list result;
                for (auto& v : d.marketRates_) result.append(v);
                return result;
            })
            .def_property_readonly("modelRates_", [](const CurveCalibrationDiagnostics_& d) -> py::list {
                py::list result;
                for (auto& v : d.modelRates_) result.append(v);
                return result;
            })
            .def_property_readonly("residuals_", [](const CurveCalibrationDiagnostics_& d) -> py::list {
                py::list result;
                for (auto& v : d.residuals_) result.append(v);
                return result;
            })
            .def_property_readonly("jacobian_", py::cpp_function([](const CurveCalibrationDiagnostics_& d) -> const Matrix_<>& { return d.jacobian_; }, py::return_value_policy::reference_internal))
            .def_property_readonly("effJacobianInverse_", py::cpp_function([](const CurveCalibrationDiagnostics_& d) -> const Matrix_<>& { return d.effJacobianInverse_; }, py::return_value_policy::reference_internal))
            .def_property_readonly("maxAbsResidual_", [](const CurveCalibrationDiagnostics_& d) { return d.maxAbsResidual_; })
            .def_property_readonly("rmsResidual_", [](const CurveCalibrationDiagnostics_& d) { return d.rmsResidual_; });

        py::class_<CalibrationResult_>(m, "CalibrationResult_")
            .def_property_readonly("curve_", [](const CalibrationResult_& r) -> std::shared_ptr<DiscountCurve_> {
                return std::const_pointer_cast<DiscountCurve_>(r.curve_);
            })
            .def_property_readonly("diagnostics_", py::cpp_function([](const CalibrationResult_& r) -> const CurveCalibrationDiagnostics_& {
                return r.diagnostics_;
            }, py::return_value_policy::reference_internal));
    }

    void init_bindings_curve_calibration_results(py::module_& m) {
        py::class_<MultiCurveCalibrationResult_>(m, "MultiCurveCalibrationResult_")
            .def_property_readonly("discountCurves_", [](const MultiCurveCalibrationResult_& r)
                -> std::map<CollateralType_, std::shared_ptr<DiscountCurve_>> {
                std::map<CollateralType_, std::shared_ptr<DiscountCurve_>> result;
                for (auto& kv : r.discountCurves_)
                    result[kv.first] = std::const_pointer_cast<DiscountCurve_>(kv.second);
                return result;
            })
            .def_property_readonly("forwardCurves_", [](const MultiCurveCalibrationResult_& r)
                -> std::map<PeriodLength_, std::shared_ptr<DiscountCurve_>> {
                std::map<PeriodLength_, std::shared_ptr<DiscountCurve_>> result;
                for (auto& kv : r.forwardCurves_)
                    result[kv.first] = std::const_pointer_cast<DiscountCurve_>(kv.second);
                return result;
            })
            .def_property_readonly("diagnostics_", [](const MultiCurveCalibrationResult_& r) -> py::list {
                py::list result;
                for (auto& d : r.diagnostics_)
                    result.append(d);
                return result;
            });

        py::class_<MultiCurveCalibrationSpec_>(m, "MultiCurveCalibrationSpec_")
            .def(py::init<>())
            .def_readwrite("name_", &MultiCurveCalibrationSpec_::name_)
            .def_readwrite("ccy_", &MultiCurveCalibrationSpec_::ccy_)
            .def_readwrite("liborBasis_", &MultiCurveCalibrationSpec_::liborBasis_)
            .def_property("stages_",
                [](const MultiCurveCalibrationSpec_& s) -> py::list {
                    py::list result;
                    for (auto& stage : s.stages_)
                        result.append(stage);
                    return result;
                },
                [](MultiCurveCalibrationSpec_& s, const py::iterable& stages) {
                    s.stages_.clear();
                    for (auto item : stages)
                        s.stages_.push_back(py::cast<CurveCalibrationSpec_>(item));
                });

        m.def("CalibrateSingleCurve",
            py::overload_cast<const CurveCalibrationSpec_&>(&CalibrateSingleCurve),
            py::arg("spec"));

        m.def("CalibrateSingleCurve",
            [](const CurveCalibrationSpec_& spec, CurveJacobianMode_::Value_ jacobianMode) {
                return CalibrateSingleCurve(spec, CurveJacobianMode_(jacobianMode));
            },
            py::arg("spec"), py::arg("jacobian_mode"));

        m.def("CalibrateMultiCurveBundle", &CalibrateMultiCurveBundle,
              py::arg("spec"));
    }

    void init_bindings_curve_xccy(py::module_& m) {
        py::class_<CrossCurrencyCalibrationSpecBuilder_>(m, "CrossCurrencyCalibrationSpecBuilder_")
            .def(py::init<>())
            .def_readwrite("today_", &CrossCurrencyCalibrationSpecBuilder_::today_)
            .def_readwrite("basisPair_", &CrossCurrencyCalibrationSpecBuilder_::basisPair_)
            .def_readwrite("fxSpot_", &CrossCurrencyCalibrationSpecBuilder_::fxSpot_)
            .def_readwrite("fxForwardCollateral_", &CrossCurrencyCalibrationSpecBuilder_::fxForwardCollateral_)
            .def_readwrite("smoothingWeight_", &CrossCurrencyCalibrationSpecBuilder_::smoothingWeight_)
            .def_readwrite("tolerance_", &CrossCurrencyCalibrationSpecBuilder_::tolerance_)
            .def_readwrite("fitTolerance_", &CrossCurrencyCalibrationSpecBuilder_::fitTolerance_)
            .def_readwrite("initialGuess_", &CrossCurrencyCalibrationSpecBuilder_::initialGuess_)
            .def_readwrite("maxEvaluations_", &CrossCurrencyCalibrationSpecBuilder_::maxEvaluations_)
            .def_readwrite("maxRestarts_", &CrossCurrencyCalibrationSpecBuilder_::maxRestarts_)
            .def_property("solveMode_",
                [](const CrossCurrencyCalibrationSpecBuilder_& b) { return b.solveMode_.Switch(); },
                [](CrossCurrencyCalibrationSpecBuilder_& b, CurveSolveMode_::Value_ v) {
                    b.solveMode_ = CurveSolveMode_(v);
                })
            .def_property("domesticCurveBlock_",
                nullptr,
                [](CrossCurrencyCalibrationSpecBuilder_& b,
                   const std::shared_ptr<CurveBlock_>& block) {
                    b.domesticCurveBlock_ = Handle_<CurveBlock_>(
                        std::const_pointer_cast<const CurveBlock_>(block));
                })
            .def_property("foreignCurveBlock_",
                nullptr,
                [](CrossCurrencyCalibrationSpecBuilder_& b,
                   const std::shared_ptr<CurveBlock_>& block) {
                    b.foreignCurveBlock_ = Handle_<CurveBlock_>(
                        std::const_pointer_cast<const CurveBlock_>(block));
                })
            .def_property("instruments_",
                nullptr,
                [](CrossCurrencyCalibrationSpecBuilder_& b, const py::iterable& instruments) {
                    b.instruments_.clear();
                    for (auto item : instruments) {
                        auto handle = py::cast<std::shared_ptr<CrossCurrencySwap_>>(item);
                        b.instruments_.push_back(Handle_<CrossCurrencySwap_>(
                            std::const_pointer_cast<const CrossCurrencySwap_>(handle)));
                    }
                })
            .def_property("knotDates_",
                nullptr,
                [](CrossCurrencyCalibrationSpecBuilder_& b, const py::iterable& dates) {
                    b.knotDates_.clear();
                    for (auto item : dates)
                        b.knotDates_.push_back(py::cast<Date_>(item));
                })
            .def("Build", &CrossCurrencyCalibrationSpecBuilder_::Build);

        py::class_<CrossCurrencyCalibrationDiagnostics_>(m, "CrossCurrencyCalibrationDiagnostics_")
            .def_property_readonly("marketRates_", [](const CrossCurrencyCalibrationDiagnostics_& d) -> py::list {
                py::list result;
                for (auto& v : d.marketRates_) result.append(v);
                return result;
            })
            .def_property_readonly("modelRates_", [](const CrossCurrencyCalibrationDiagnostics_& d) -> py::list {
                py::list result;
                for (auto& v : d.modelRates_) result.append(v);
                return result;
            })
            .def_property_readonly("residuals_", [](const CrossCurrencyCalibrationDiagnostics_& d) -> py::list {
                py::list result;
                for (auto& v : d.residuals_) result.append(v);
                return result;
            })
            .def_property_readonly("maxAbsResidual_", [](const CrossCurrencyCalibrationDiagnostics_& d) { return d.maxAbsResidual_; })
            .def_property_readonly("rmsResidual_", [](const CrossCurrencyCalibrationDiagnostics_& d) { return d.rmsResidual_; });

        py::class_<CrossCurrencyFxForwardCurve_>(m, "CrossCurrencyFxForwardCurve_")
            .def_property_readonly("pair_", [](const CrossCurrencyFxForwardCurve_& c) { return c.pair_; })
            .def_property_readonly("dates_", [](const CrossCurrencyFxForwardCurve_& c) -> py::list {
                py::list result;
                for (auto& d : c.dates_) result.append(d);
                return result;
            })
            .def_property_readonly("forwards_", [](const CrossCurrencyFxForwardCurve_& c) -> py::list {
                py::list result;
                for (auto& v : c.forwards_) result.append(v);
                return result;
            });

        py::class_<CrossCurrencyCalibrationResult_>(m, "CrossCurrencyCalibrationResult_")
            .def_property_readonly("market_", [](const CrossCurrencyCalibrationResult_& r)
                -> std::shared_ptr<CrossCurrencyMarket_> {
                const auto& mkt = r.market_;
                return std::make_shared<CrossCurrencyMarket_>(mkt);
            })
            .def_property_readonly("fxForwardCurve_", py::cpp_function([](const CrossCurrencyCalibrationResult_& r) -> const CrossCurrencyFxForwardCurve_& {
                return r.fxForwardCurve_;
            }, py::return_value_policy::reference_internal))
            .def_property_readonly("diagnostics_", py::cpp_function([](const CrossCurrencyCalibrationResult_& r) -> const CrossCurrencyCalibrationDiagnostics_& {
                return r.diagnostics_;
            }, py::return_value_policy::reference_internal));

        m.def("CalibrateXccyMarket", &CalibrateXccyMarket, py::arg("spec"));
    }
} // anonymous namespace

void init_bindings_curve(py::module_& m) {
    init_bindings_curve_handles(m);
    init_bindings_curve_protocol(m);
    init_bindings_curve_instruments(m);
    init_bindings_curve_data(m);
    init_bindings_curve_enums(m);
    init_bindings_curve_calibration_builder(m);
    init_bindings_curve_calibration_diagnostics(m);
    init_bindings_curve_calibration_results(m);
    init_bindings_curve_xccy(m);
}

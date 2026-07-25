//
// curve.cpp - curve calibration bindings
//

#include "bindings.h"

#include <pybind11/stl.h>

#include <dal/curve/calibration.hpp>
#include <dal/curve/xccycalibration.hpp>
#include <dal/curve/xccynotionalmode.hpp>
#include <dal/math/matrix/matrixs.hpp>
#include <dal/protocol/collateraltype.hpp>
#include <dal/time/datetime.hpp>
#include <dal/time/daybasis.hpp>
#include <dal/time/periodlength.hpp>

#include <dal-public/src/curveprotocol.hpp>
#include <dal-public/src/curveinstrument.hpp>
#include <dal-public/src/curvedata.hpp>
#include <dal-public/src/curvespec.hpp>
#include <dal-public/src/xccycalibration.hpp>

using namespace Dal;

namespace {
    template <class Class_, class Member_>
    void DefReadWriteAliases(py::class_<Class_>& cls, const char* legacyName, const char* snakeName, Member_ Class_::* member) {
        cls.def_readwrite(legacyName, member).def_readwrite(snakeName, member);
    }

    template <class Class_, class Getter_, class Setter_>
    void DefPropertyAliases(py::class_<Class_>& cls, const char* legacyName, const char* snakeName, Getter_ getter, Setter_ setter) {
        cls.def_property(legacyName, getter, setter).def_property(snakeName, getter, setter);
    }

    template <class Class_> void DefStringAliases(py::class_<Class_>& cls, const char* legacyName, const char* snakeName, String_ Class_::* member) {
        DefPropertyAliases(
            cls, legacyName, snakeName, [member](const Class_& value) { return std::string((value.*member).c_str()); },
            [member](Class_& value, const char* text) { value.*member = String_(text); });
    }

    template <class Instrument_> py::list InstrumentHandlesToList(const Vector_<Handle_<Instrument_>>& instruments) {
        py::list result;
        for (const auto& instrument : instruments)
            result.append(std::const_pointer_cast<Instrument_>(Handle_<Instrument_>(instrument)));
        return result;
    }

    template <class Instrument_> void SetInstrumentHandles(Vector_<Handle_<Instrument_>>* destination, const py::iterable& instruments) {
        destination->clear();
        for (const auto item : instruments) {
            const auto instrument = py::cast<std::shared_ptr<Instrument_>>(item);
            destination->push_back(Handle_<Instrument_>(std::const_pointer_cast<const Instrument_>(instrument)));
        }
    }

    py::list DatesToList(const Vector_<Date_>& dates) {
        py::list result;
        for (const auto& date : dates)
            result.append(date);
        return result;
    }

    void SetDates(Vector_<Date_>* destination, const py::iterable& dates) {
        destination->clear();
        for (const auto item : dates)
            destination->push_back(py::cast<Date_>(item));
    }

    py::list DoublesToList(const Vector_<>& values) {
        py::list result;
        for (const auto value : values)
            result.append(value);
        return result;
    }

    template <class Value_> py::list ValuesToList(const Vector_<Value_>& values) {
        py::list result;
        for (const auto& value : values)
            result.append(value);
        return result;
    }

    void SetDoubles(Vector_<>* destination, const py::iterable& values) {
        destination->clear();
        for (const auto item : values)
            destination->push_back(py::cast<double>(item));
    }

    std::shared_ptr<MarketFixingSnapshot_> MutableSnapshot(const Handle_<MarketFixingSnapshot_>& snapshot) {
        return std::const_pointer_cast<MarketFixingSnapshot_>(Handle_<MarketFixingSnapshot_>(snapshot));
    }

    Handle_<MarketFixingSnapshot_> ConstSnapshot(const std::shared_ptr<MarketFixingSnapshot_>& snapshot) {
        return Handle_<MarketFixingSnapshot_>(std::const_pointer_cast<const MarketFixingSnapshot_>(snapshot));
    }

    MarketFixingSnapshot_::values_t SnapshotValues(const py::dict& values) {
        MarketFixingSnapshot_::values_t result;
        for (const auto outerItem : py::reinterpret_borrow<py::iterable>(values.attr("items")())) {
            const auto pair = py::reinterpret_borrow<py::tuple>(outerItem);
            const String_ indexName(py::cast<std::string>(pair[0]));
            const auto history = py::cast<py::dict>(pair[1]);
            for (const auto innerItem : py::reinterpret_borrow<py::iterable>(history.attr("items")())) {
                const auto fixing = py::reinterpret_borrow<py::tuple>(innerItem);
                result[indexName][py::cast<DateTime_>(fixing[0])] = py::cast<double>(fixing[1]);
            }
        }
        return result;
    }

    void AddMatrixSnakeCaseAliases(py::module_& m) {
        const py::object matrixClass = m.attr("DoubleMatrix_");
        matrixClass.attr("rows") = py::cpp_function([](const Matrix_<>& matrix) { return matrix.Rows(); }, py::is_method(matrixClass));
        matrixClass.attr("cols") = py::cpp_function([](const Matrix_<>& matrix) { return matrix.Cols(); }, py::is_method(matrixClass));
    }

    void init_bindings_curve_handles(py::module_& m) {
        py::class_<YCInstrument_, std::shared_ptr<YCInstrument_>>(m, "YCInstrument_");
        py::class_<DiscountCurve_, std::shared_ptr<DiscountCurve_>>(m, "DiscountCurve_")
            .def("__call__", [](const DiscountCurve_& curve, const Date_& from, const Date_& to) { return curve(from, to); },
                 py::arg("from_date"), py::arg("to_date"));
        py::class_<CurveBlock_, std::shared_ptr<CurveBlock_>>(m, "CurveBlock_");
        py::class_<CrossCurrencySwap_, std::shared_ptr<CrossCurrencySwap_>>(m, "CrossCurrencySwap_");
        py::class_<CrossCurrencyMarket_, std::shared_ptr<CrossCurrencyMarket_>>(m, "CrossCurrencyMarket_");

        // Value types used as function parameters / return values
        py::class_<CurveCalibrationSpec_>(m, "CurveCalibrationSpec_");
        py::class_<CrossCurrencyCalibrationSpec_>(m, "CrossCurrencyCalibrationSpec_");
        py::class_<JointXccyCalibrationSpec_>(m, "JointXccyCalibrationSpec_");
    }

    void init_bindings_curve_protocol(py::module_& m) {
        py::class_<DateTime_>(m, "DateTime_")
            .def(py::init<>())
            .def(py::init<const Date_&, int, int, int>(), py::arg("date"), py::arg("hour"), py::arg("minute") = 0, py::arg("second") = 0)
            .def_property_readonly("date", &DateTime_::Date)
            .def_property_readonly("fraction", &DateTime_::Frac)
            .def("IsValid", &DateTime_::IsValid)
            .def("is_valid", &DateTime_::IsValid)
            .def("__repr__", [](const DateTime_& value) { return std::string(DateTime::ToString(value).c_str()); });

        py::class_<Ccy_>(m, "Ccy_")
            .def(py::init([](const char* src) { return Ccy_(String_(src)); }), py::arg("src"))
            .def("__repr__", [](const Ccy_& ccy) { return std::string(ccy.String()); });

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

        auto rateLeg = py::class_<RateLegConvention_>(m, "RateLegConvention_");
        rateLeg.def(py::init<>());
        DefReadWriteAliases(rateLeg, "paymentLag_", "payment_lag", &RateLegConvention_::paymentLag_);
        DefReadWriteAliases(rateLeg, "paymentFrequency_", "payment_frequency", &RateLegConvention_::paymentFrequency_);
        DefReadWriteAliases(rateLeg, "dayBasis_", "day_basis", &RateLegConvention_::dayBasis_);
        DefPropertyAliases(
            rateLeg, "businessDayConvention_", "business_day_convention",
            [](const RateLegConvention_& value) { return value.businessDayConvention_.Switch(); },
            [](RateLegConvention_& value, BizDayConvention_::Value_ convention) { value.businessDayConvention_ = BizDayConvention_(convention); });
        DefPropertyAliases(
            rateLeg, "paymentConvention_", "payment_convention", [](const RateLegConvention_& value) { return value.paymentConvention_.Switch(); },
            [](RateLegConvention_& value, BizDayConvention_::Value_ convention) { value.paymentConvention_ = BizDayConvention_(convention); });
        DefReadWriteAliases(rateLeg, "accrualHolidays_", "accrual_holidays", &RateLegConvention_::accrualHolidays_);
        DefReadWriteAliases(rateLeg, "paymentHolidays_", "payment_holidays", &RateLegConvention_::paymentHolidays_);
        DefReadWriteAliases(rateLeg, "endOfMonth_", "end_of_month", &RateLegConvention_::endOfMonth_);

        m.def("RateLegConvention_New", &RateLegConvention_New,
              py::arg("freq"), py::arg("basis"));

        auto rateIndex = py::class_<RateIndexConvention_>(m, "RateIndexConvention_");
        rateIndex.def(py::init<>());
        DefReadWriteAliases(rateIndex, "spotLag_", "spot_lag", &RateIndexConvention_::spotLag_);
        DefReadWriteAliases(rateIndex, "fixingLag_", "fixing_lag", &RateIndexConvention_::fixingLag_);
        DefReadWriteAliases(rateIndex, "useProjectionCurve_", "use_projection_curve", &RateIndexConvention_::useProjectionCurve_);
        DefReadWriteAliases(rateIndex, "forecastTenor_", "forecast_tenor", &RateIndexConvention_::forecastTenor_);
        DefReadWriteAliases(rateIndex, "dayBasis_", "day_basis", &RateIndexConvention_::dayBasis_);
        DefPropertyAliases(
            rateIndex, "businessDayConvention_", "business_day_convention",
            [](const RateIndexConvention_& value) { return value.businessDayConvention_.Switch(); },
            [](RateIndexConvention_& value, BizDayConvention_::Value_ convention) { value.businessDayConvention_ = BizDayConvention_(convention); });
        DefReadWriteAliases(rateIndex, "fixingHolidays_", "fixing_holidays", &RateIndexConvention_::fixingHolidays_);
        DefReadWriteAliases(rateIndex, "accrualHolidays_", "accrual_holidays", &RateIndexConvention_::accrualHolidays_);
        DefReadWriteAliases(rateIndex, "endOfMonth_", "end_of_month", &RateIndexConvention_::endOfMonth_);
        DefReadWriteAliases(rateIndex, "collateral_", "collateral", &RateIndexConvention_::collateral_);

        m.def("RateIndexConvention_New", &RateIndexConvention_New,
              py::arg("forecast_tenor"), py::arg("basis"), py::arg("collateral"),
              py::arg("use_projection_curve") = false);

        auto currencyPair = py::class_<CurrencyPair_>(m, "CurrencyPair_");
        DefReadWriteAliases(currencyPair, "domestic_", "domestic", &CurrencyPair_::domestic_);
        DefReadWriteAliases(currencyPair, "foreign_", "foreign", &CurrencyPair_::foreign_);

        m.def("CurrencyPair_New",
              [](const char* domestic, const char* foreign) {
                  return CurrencyPair_New(String_(domestic), String_(foreign));
              },
              py::arg("domestic"), py::arg("foreign"));

        auto fixingIdentity = py::class_<FixingIdentity_>(m, "FixingIdentity_");
        fixingIdentity.def(py::init<>());
        DefStringAliases(fixingIdentity, "indexName_", "index_name", &FixingIdentity_::indexName_);
        DefReadWriteAliases(fixingIdentity, "fixingHour_", "fixing_hour", &FixingIdentity_::fixingHour_);
        DefReadWriteAliases(fixingIdentity, "fixingMinute_", "fixing_minute", &FixingIdentity_::fixingMinute_);

        auto fxReset = py::class_<FxResetConvention_>(m, "FxResetConvention_");
        fxReset.def(py::init<>());
        DefReadWriteAliases(fxReset, "fixingLag_", "fixing_lag", &FxResetConvention_::fixingLag_);
        DefReadWriteAliases(fxReset, "fixingHolidays_", "fixing_holidays", &FxResetConvention_::fixingHolidays_);
        DefPropertyAliases(
            fxReset, "fixingConvention_", "fixing_convention", [](const FxResetConvention_& value) { return value.fixingConvention_.Switch(); },
            [](FxResetConvention_& value, BizDayConvention_::Value_ convention) { value.fixingConvention_ = BizDayConvention_(convention); });
        DefReadWriteAliases(fxReset, "fixingHour_", "fixing_hour", &FxResetConvention_::fixingHour_);
        DefReadWriteAliases(fxReset, "fixingMinute_", "fixing_minute", &FxResetConvention_::fixingMinute_);

        m.def(
            "FxResetConvention_New",
            [](int fixingLag, const Holidays_& fixingHolidays, BizDayConvention_::Value_ fixingConvention, int fixingHour, int fixingMinute) {
                return FxResetConventionNew(fixingLag, fixingHolidays, BizDayConvention_(fixingConvention), fixingHour, fixingMinute);
            },
            py::arg("fixing_lag"), py::arg("fixing_holidays"), py::arg("fixing_convention"), py::arg("fixing_hour"), py::arg("fixing_minute"));

        auto snapshot = py::class_<MarketFixingSnapshot_, std::shared_ptr<MarketFixingSnapshot_>>(m, "MarketFixingSnapshot_");
        snapshot
            .def("Find", [](const MarketFixingSnapshot_& value, const char* indexName,
                            const DateTime_& fixingTime) { return value.Find(String_(indexName), fixingTime); })
            .def("find", [](const MarketFixingSnapshot_& value, const char* indexName,
                            const DateTime_& fixingTime) { return value.Find(String_(indexName), fixingTime); })
            .def("Require", [](const MarketFixingSnapshot_& value, const char* indexName, const DateTime_& fixingTime,
                               const char* context) { return value.Require(String_(indexName), fixingTime, String_(context)); })
            .def("require", [](const MarketFixingSnapshot_& value, const char* indexName, const DateTime_& fixingTime, const char* context) {
                return value.Require(String_(indexName), fixingTime, String_(context));
            });

        m.def(
            "MarketFixingSnapshot_New", [](const py::dict& values) { return MutableSnapshot(MarketFixingSnapshotNew(SnapshotValues(values))); },
            py::arg("values"));
    }

    void init_bindings_curve_instruments(py::module_& m) {
        auto convention = py::class_<CrossCurrencyConvention_>(m, "CrossCurrencyConvention_");
        convention.def(py::init<>());
        DefReadWriteAliases(convention, "initialNotionalExchange_", "initial_notional_exchange", &CrossCurrencyConvention_::initialNotionalExchange_);
        DefReadWriteAliases(convention, "finalNotionalExchange_", "final_notional_exchange", &CrossCurrencyConvention_::finalNotionalExchange_);
        DefReadWriteAliases(convention, "spreadOnForeignLeg_", "spread_on_foreign_leg", &CrossCurrencyConvention_::spreadOnForeignLeg_);
        DefReadWriteAliases(convention, "domesticIndex_", "domestic_index", &CrossCurrencyConvention_::domesticIndex_);
        DefReadWriteAliases(convention, "domesticLeg_", "domestic_leg", &CrossCurrencyConvention_::domesticLeg_);
        DefReadWriteAliases(convention, "foreignIndex_", "foreign_index", &CrossCurrencyConvention_::foreignIndex_);
        DefReadWriteAliases(convention, "foreignLeg_", "foreign_leg", &CrossCurrencyConvention_::foreignLeg_);

        auto config = py::class_<CrossCurrencySwapConfig_>(m, "CrossCurrencySwapConfig_");
        config.def(py::init<>());
        DefReadWriteAliases(config, "pair_", "pair", &CrossCurrencySwapConfig_::pair_);
        DefReadWriteAliases(config, "domesticNotional_", "domestic_notional", &CrossCurrencySwapConfig_::domesticNotional_);
        DefReadWriteAliases(config, "foreignNotional_", "foreign_notional", &CrossCurrencySwapConfig_::foreignNotional_);
        DefReadWriteAliases(config, "convention_", "convention", &CrossCurrencySwapConfig_::convention_);
        DefPropertyAliases(
            config, "notionalMode_", "notional_mode", [](const CrossCurrencySwapConfig_& value) { return value.notionalMode_.Switch(); },
            [](CrossCurrencySwapConfig_& value, XccyNotionalMode_::Value_ mode) { value.notionalMode_ = XccyNotionalMode_(mode); });
        DefReadWriteAliases(config, "fxReset_", "fx_reset", &CrossCurrencySwapConfig_::fxReset_);
        DefReadWriteAliases(config, "domesticRateFixing_", "domestic_rate_fixing", &CrossCurrencySwapConfig_::domesticRateFixing_);
        DefReadWriteAliases(config, "foreignRateFixing_", "foreign_rate_fixing", &CrossCurrencySwapConfig_::foreignRateFixing_);

        auto configBuilder = py::class_<CrossCurrencySwapConfigBuilder_>(m, "CrossCurrencySwapConfigBuilder_");
        configBuilder.def(py::init<>());
        DefReadWriteAliases(configBuilder, "pair_", "pair", &CrossCurrencySwapConfigBuilder_::pair_);
        DefReadWriteAliases(configBuilder, "domesticNotional_", "domestic_notional", &CrossCurrencySwapConfigBuilder_::domesticNotional_);
        DefReadWriteAliases(configBuilder, "foreignNotional_", "foreign_notional", &CrossCurrencySwapConfigBuilder_::foreignNotional_);
        DefReadWriteAliases(configBuilder, "convention_", "convention", &CrossCurrencySwapConfigBuilder_::convention_);
        DefPropertyAliases(
            configBuilder, "notionalMode_", "notional_mode",
            [](const CrossCurrencySwapConfigBuilder_& value) { return value.notionalMode_.Switch(); },
            [](CrossCurrencySwapConfigBuilder_& value, XccyNotionalMode_::Value_ mode) { value.notionalMode_ = XccyNotionalMode_(mode); });
        DefReadWriteAliases(configBuilder, "fxReset_", "fx_reset", &CrossCurrencySwapConfigBuilder_::fxReset_);
        DefReadWriteAliases(configBuilder, "domesticRateFixing_", "domestic_rate_fixing", &CrossCurrencySwapConfigBuilder_::domesticRateFixing_);
        DefReadWriteAliases(configBuilder, "foreignRateFixing_", "foreign_rate_fixing", &CrossCurrencySwapConfigBuilder_::foreignRateFixing_);
        configBuilder.def("Build", &CrossCurrencySwapConfigBuilder_::Build).def("build", &CrossCurrencySwapConfigBuilder_::Build);

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

        m.def(
            "CrossCurrencySwap_New",
            [](const Date_& tradeDate, const Date_& start, const Date_& maturity, double marketRate,
               const CrossCurrencySwapConfig_& config) -> std::shared_ptr<CrossCurrencySwap_> {
                return std::const_pointer_cast<CrossCurrencySwap_>(
                    Handle_<CrossCurrencySwap_>(CrossCurrencySwapNew(tradeDate, start, maturity, marketRate, config)));
            },
            py::arg("trade_date"), py::arg("start"), py::arg("maturity"), py::arg("market_rate"), py::arg("config"));
    }

    void init_bindings_curve_data(py::module_& m) {
        py::class_<DiscountZeroRate_, DiscountCurve_, std::shared_ptr<DiscountZeroRate_>>(m, "DiscountZeroRate_")
            .def_property_readonly("anchor_date", [](const DiscountZeroRate_& curve) { return curve.AnchorDate(); })
            .def_property_readonly("node_dates", [](const DiscountZeroRate_& curve) {
                py::list result;
                for (const auto& date : curve.NodeDates())
                    result.append(date);
                return result;
            })
            .def_property_readonly("zero_rates", [](const DiscountZeroRate_& curve) {
                py::list result;
                for (const auto rate : curve.NodeZeroRates())
                    result.append(rate);
                return result;
            })
            .def_property_readonly("day_count", [](const DiscountZeroRate_& curve) { return std::string(curve.DayCount().String()); })
            .def_property_readonly("log_df_scheme", [](const DiscountZeroRate_& curve) { return curve.Scheme().Switch(); });

        m.def("DiscountZeroRate_New",
            [](const char* name, const char* ccy,
               const Date_& anchorDate,
               const py::iterable& nodeDatesPy,
               const py::iterable& zeroRatesPy,
               const DayBasis_& dayCount,
               LogDfScheme_::Value_ logDfScheme,
               const std::shared_ptr<DiscountCurve_>& base)
               -> std::shared_ptr<DiscountCurve_> {
                Vector_<Date_> nodeDates;
                SetDates(&nodeDates, nodeDatesPy);
                Vector_<> zeroRates;
                SetDoubles(&zeroRates, zeroRatesPy);
                Handle_<DiscountCurve_> baseHandle(
                    std::const_pointer_cast<const DiscountCurve_>(base));
                return std::const_pointer_cast<DiscountCurve_>(
                    DiscountZeroRateNew(String_(name), String_(ccy), anchorDate, nodeDates,
                                        zeroRates, dayCount, LogDfScheme_(logDfScheme), baseHandle));
            },
            py::arg("name"), py::arg("ccy"), py::arg("anchor_date"),
            py::arg("node_dates"), py::arg("zero_rates"),
            py::arg("day_count") = DayBasis_("ACT_365F"),
            py::arg("log_df_scheme") = LogDfScheme_::Value_::LOG_LINEAR,
            py::arg("base") = std::shared_ptr<DiscountCurve_>());

        m.def("DiscountPWLF_New",
            [](const char* name, const char* ccy,
               const py::iterable& knotDatesPy,
               const py::iterable& fwdRatesPy,
               const std::shared_ptr<DiscountCurve_>& base)
               -> std::shared_ptr<DiscountCurve_> {
                Vector_<Date_> knotDates;
                SetDates(&knotDates, knotDatesPy);
                Vector_<> fwdRates;
                SetDoubles(&fwdRates, fwdRatesPy);
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

        py::enum_<XccyNotionalMode_::Value_>(m, "XccyNotionalMode")
            .value("FIXED", XccyNotionalMode_::Value_::FIXED)
            .value("RESETTABLE", XccyNotionalMode_::Value_::RESETTABLE)
            .value("MARK_TO_MARKET", XccyNotionalMode_::Value_::MARK_TO_MARKET);

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
            py::arg("spec"),
            py::call_guard<py::gil_scoped_release>());

        m.def("CalibrateSingleCurve",
            [](const CurveCalibrationSpec_& spec, CurveJacobianMode_::Value_ jacobianMode) {
                py::gil_scoped_release release;
                return CalibrateSingleCurve(spec, CurveJacobianMode_(jacobianMode));
            },
            py::arg("spec"), py::arg("jacobian_mode"));

        m.def("CalibrateMultiCurveBundle", &CalibrateMultiCurveBundle,
              py::arg("spec"),
              py::call_guard<py::gil_scoped_release>());
    }

    void init_bindings_curve_xccy(py::module_& m) {
        auto xccyBuilder = py::class_<CrossCurrencyCalibrationSpecBuilder_>(m, "CrossCurrencyCalibrationSpecBuilder_");
        xccyBuilder.def(py::init<>());
        DefReadWriteAliases(xccyBuilder, "today_", "today", &CrossCurrencyCalibrationSpecBuilder_::today_);
        DefReadWriteAliases(xccyBuilder, "valuationTime_", "valuation_time", &CrossCurrencyCalibrationSpecBuilder_::valuationTime_);
        DefReadWriteAliases(xccyBuilder, "collateralCurrency_", "collateral_currency", &CrossCurrencyCalibrationSpecBuilder_::collateralCurrency_);
        DefPropertyAliases(
            xccyBuilder, "fixings_", "fixings", [](const CrossCurrencyCalibrationSpecBuilder_& value) { return MutableSnapshot(value.fixings_); },
            [](CrossCurrencyCalibrationSpecBuilder_& value, const std::shared_ptr<MarketFixingSnapshot_>& fixings) {
                value.fixings_ = ConstSnapshot(fixings);
            });
        DefReadWriteAliases(xccyBuilder, "basisPair_", "basis_pair", &CrossCurrencyCalibrationSpecBuilder_::basisPair_);
        DefReadWriteAliases(xccyBuilder, "fxSpot_", "fx_spot", &CrossCurrencyCalibrationSpecBuilder_::fxSpot_);
        DefReadWriteAliases(xccyBuilder, "fxForwardCollateral_", "fx_forward_collateral",
                            &CrossCurrencyCalibrationSpecBuilder_::fxForwardCollateral_);
        DefReadWriteAliases(xccyBuilder, "smoothingWeight_", "smoothing_weight", &CrossCurrencyCalibrationSpecBuilder_::smoothingWeight_);
        DefReadWriteAliases(xccyBuilder, "tolerance_", "tolerance", &CrossCurrencyCalibrationSpecBuilder_::tolerance_);
        DefReadWriteAliases(xccyBuilder, "fitTolerance_", "fit_tolerance", &CrossCurrencyCalibrationSpecBuilder_::fitTolerance_);
        DefReadWriteAliases(xccyBuilder, "initialGuess_", "initial_guess", &CrossCurrencyCalibrationSpecBuilder_::initialGuess_);
        DefReadWriteAliases(xccyBuilder, "maxEvaluations_", "max_evaluations", &CrossCurrencyCalibrationSpecBuilder_::maxEvaluations_);
        DefReadWriteAliases(xccyBuilder, "maxRestarts_", "max_restarts", &CrossCurrencyCalibrationSpecBuilder_::maxRestarts_);
        DefPropertyAliases(
            xccyBuilder, "solveMode_", "solve_mode", [](const CrossCurrencyCalibrationSpecBuilder_& value) { return value.solveMode_.Switch(); },
            [](CrossCurrencyCalibrationSpecBuilder_& value, CurveSolveMode_::Value_ mode) { value.solveMode_ = CurveSolveMode_(mode); });
        DefPropertyAliases(
            xccyBuilder, "domesticCurveBlock_", "domestic_curve_block",
            [](const CrossCurrencyCalibrationSpecBuilder_& value) { return std::const_pointer_cast<CurveBlock_>(value.domesticCurveBlock_); },
            [](CrossCurrencyCalibrationSpecBuilder_& value, const std::shared_ptr<CurveBlock_>& block) {
                value.domesticCurveBlock_ = Handle_<CurveBlock_>(std::const_pointer_cast<const CurveBlock_>(block));
            });
        DefPropertyAliases(
            xccyBuilder, "foreignCurveBlock_", "foreign_curve_block",
            [](const CrossCurrencyCalibrationSpecBuilder_& value) { return std::const_pointer_cast<CurveBlock_>(value.foreignCurveBlock_); },
            [](CrossCurrencyCalibrationSpecBuilder_& value, const std::shared_ptr<CurveBlock_>& block) {
                value.foreignCurveBlock_ = Handle_<CurveBlock_>(std::const_pointer_cast<const CurveBlock_>(block));
            });
        DefPropertyAliases(
            xccyBuilder, "instruments_", "instruments",
            [](const CrossCurrencyCalibrationSpecBuilder_& value) { return InstrumentHandlesToList(value.instruments_); },
            [](CrossCurrencyCalibrationSpecBuilder_& value, const py::iterable& instruments) {
                SetInstrumentHandles(&value.instruments_, instruments);
            });
        DefPropertyAliases(
            xccyBuilder, "knotDates_", "knot_dates", [](const CrossCurrencyCalibrationSpecBuilder_& value) { return DatesToList(value.knotDates_); },
            [](CrossCurrencyCalibrationSpecBuilder_& value, const py::iterable& dates) { SetDates(&value.knotDates_, dates); });
        xccyBuilder.def("Build", &CrossCurrencyCalibrationSpecBuilder_::Build).def("build", &CrossCurrencyCalibrationSpecBuilder_::Build);

        auto xccyOptions = py::class_<CrossCurrencyCalibrationOptions_>(m, "CrossCurrencyCalibrationOptions_");
        xccyOptions.def(py::init<>());
        DefPropertyAliases(
            xccyOptions, "jacobianMode_", "jacobian_mode", [](const CrossCurrencyCalibrationOptions_& value) { return value.jacobianMode_.Switch(); },
            [](CrossCurrencyCalibrationOptions_& value, CurveJacobianMode_::Value_ mode) { value.jacobianMode_ = CurveJacobianMode_(mode); });
        DefReadWriteAliases(xccyOptions, "computeEffJacobianInverse_", "compute_eff_jacobian_inverse",
                            &CrossCurrencyCalibrationOptions_::computeEffJacobianInverse_);
        DefReadWriteAliases(xccyOptions, "computeForwardJacobian_", "compute_forward_jacobian",
                            &CrossCurrencyCalibrationOptions_::computeForwardJacobian_);

        auto xccyDiagnostics = py::class_<CrossCurrencyCalibrationDiagnostics_>(m, "CrossCurrencyCalibrationDiagnostics_");
        const auto instrumentNames = [](const CrossCurrencyCalibrationDiagnostics_& value) {
            py::list result;
            for (const auto& name : value.instrumentNames_)
                result.append(std::string(name.c_str()));
            return result;
        };
        const auto jacobian = py::cpp_function(
            [](const CrossCurrencyCalibrationDiagnostics_& value) -> const Matrix_<>& { return value.jacobian_; },
            py::return_value_policy::reference_internal);
        const auto effJacobianInverse = py::cpp_function(
            [](const CrossCurrencyCalibrationDiagnostics_& value) -> const Matrix_<>& { return value.effJacobianInverse_; },
            py::return_value_policy::reference_internal);
        xccyDiagnostics
            .def_property_readonly("instrumentNames_", instrumentNames)
            .def_property_readonly("instrument_names", instrumentNames)
            .def_property_readonly("parameterKnotDates_",
                                   [](const CrossCurrencyCalibrationDiagnostics_& value) { return DatesToList(value.parameterKnotDates_); })
            .def_property_readonly("parameter_knot_dates",
                                   [](const CrossCurrencyCalibrationDiagnostics_& value) { return DatesToList(value.parameterKnotDates_); })
            .def_property_readonly("marketRates_",
                                   [](const CrossCurrencyCalibrationDiagnostics_& value) { return DoublesToList(value.marketRates_); })
            .def_property_readonly("market_rates",
                                   [](const CrossCurrencyCalibrationDiagnostics_& value) { return DoublesToList(value.marketRates_); })
            .def_property_readonly("modelRates_", [](const CrossCurrencyCalibrationDiagnostics_& value) { return DoublesToList(value.modelRates_); })
            .def_property_readonly("model_rates", [](const CrossCurrencyCalibrationDiagnostics_& value) { return DoublesToList(value.modelRates_); })
            .def_property_readonly("residuals_", [](const CrossCurrencyCalibrationDiagnostics_& value) { return DoublesToList(value.residuals_); })
            .def_property_readonly("residuals", [](const CrossCurrencyCalibrationDiagnostics_& value) { return DoublesToList(value.residuals_); })
            .def_property_readonly("maxAbsResidual_", [](const CrossCurrencyCalibrationDiagnostics_& value) { return value.maxAbsResidual_; })
            .def_property_readonly("max_abs_residual", [](const CrossCurrencyCalibrationDiagnostics_& value) { return value.maxAbsResidual_; })
            .def_property_readonly("rmsResidual_", [](const CrossCurrencyCalibrationDiagnostics_& value) { return value.rmsResidual_; })
            .def_property_readonly("rms_residual", [](const CrossCurrencyCalibrationDiagnostics_& value) { return value.rmsResidual_; })
            .def_property_readonly("jacobian_", jacobian)
            .def_property_readonly("jacobian", jacobian)
            .def_property_readonly("effJacobianInverse_", effJacobianInverse)
            .def_property_readonly("eff_jacobian_inverse", effJacobianInverse)
            .def_property_readonly("residualTolerance_",
                                   [](const CrossCurrencyCalibrationDiagnostics_& value) { return value.residualTolerance_; })
            .def_property_readonly("residual_tolerance",
                                   [](const CrossCurrencyCalibrationDiagnostics_& value) { return value.residualTolerance_; })
            .def_property_readonly("jacobianScaling_",
                                   [](const CrossCurrencyCalibrationDiagnostics_& value) {
                                       return std::string(value.jacobianScaling_.c_str());
                                   })
            .def_property_readonly("jacobian_scaling",
                                   [](const CrossCurrencyCalibrationDiagnostics_& value) {
                                       return std::string(value.jacobianScaling_.c_str());
                                   })
            .def_property_readonly("effJacobianInverseScaling_",
                                   [](const CrossCurrencyCalibrationDiagnostics_& value) {
                                       return std::string(value.effJacobianInverseScaling_.c_str());
                                   })
            .def_property_readonly("eff_jacobian_inverse_scaling",
                                   [](const CrossCurrencyCalibrationDiagnostics_& value) {
                                       return std::string(value.effJacobianInverseScaling_.c_str());
                                   })
            .def_property_readonly("jacobianAvailability_",
                                   [](const CrossCurrencyCalibrationDiagnostics_& value) {
                                       return std::string(value.jacobianAvailability_.c_str());
                                   })
            .def_property_readonly("jacobian_availability",
                                   [](const CrossCurrencyCalibrationDiagnostics_& value) {
                                       return std::string(value.jacobianAvailability_.c_str());
                                   })
            .def_property_readonly("effJacobianInverseAvailability_",
                                   [](const CrossCurrencyCalibrationDiagnostics_& value) {
                                       return std::string(value.effJacobianInverseAvailability_.c_str());
                                   })
            .def_property_readonly("eff_jacobian_inverse_availability",
                                   [](const CrossCurrencyCalibrationDiagnostics_& value) {
                                       return std::string(value.effJacobianInverseAvailability_.c_str());
                                   })
            .def_property_readonly("usedApproximateFit_",
                                   [](const CrossCurrencyCalibrationDiagnostics_& value) { return value.usedApproximateFit_; })
            .def_property_readonly("used_approximate_fit",
                                   [](const CrossCurrencyCalibrationDiagnostics_& value) { return value.usedApproximateFit_; });

        auto fxForwardCurve = py::class_<CrossCurrencyFxForwardCurve_>(m, "CrossCurrencyFxForwardCurve_");
        fxForwardCurve.def_property_readonly("pair_", [](const CrossCurrencyFxForwardCurve_& value) { return value.pair_; })
            .def_property_readonly("pair", [](const CrossCurrencyFxForwardCurve_& value) { return value.pair_; })
            .def_property_readonly("dates_", [](const CrossCurrencyFxForwardCurve_& value) { return DatesToList(value.dates_); })
            .def_property_readonly("dates", [](const CrossCurrencyFxForwardCurve_& value) { return DatesToList(value.dates_); })
            .def_property_readonly("forwards_", [](const CrossCurrencyFxForwardCurve_& value) { return DoublesToList(value.forwards_); })
            .def_property_readonly("forwards", [](const CrossCurrencyFxForwardCurve_& value) { return DoublesToList(value.forwards_); });

        auto xccyResult = py::class_<CrossCurrencyCalibrationResult_>(m, "CrossCurrencyCalibrationResult_");
        const auto market = [](const CrossCurrencyCalibrationResult_& value) { return std::make_shared<CrossCurrencyMarket_>(value.market_); };
        const auto fxForwards = py::cpp_function(
            [](const CrossCurrencyCalibrationResult_& value) -> const CrossCurrencyFxForwardCurve_& { return value.fxForwardCurve_; },
            py::return_value_policy::reference_internal);
        const auto diagnostics = py::cpp_function(
            [](const CrossCurrencyCalibrationResult_& value) -> const CrossCurrencyCalibrationDiagnostics_& { return value.diagnostics_; },
            py::return_value_policy::reference_internal);
        xccyResult.def_property_readonly("market_", market).def_property_readonly("market", market);
        xccyResult.def_property_readonly("fxForwardCurve_", fxForwards).def_property_readonly("fx_forward_curve", fxForwards);
        xccyResult.def_property_readonly("diagnostics_", diagnostics).def_property_readonly("diagnostics", diagnostics);

        auto solverOptions = py::class_<CurveSolverOptions_>(m, "CurveSolverOptions_");
        solverOptions.def(py::init<>());
        DefReadWriteAliases(solverOptions, "smoothingWeight_", "smoothing_weight", &CurveSolverOptions_::smoothingWeight_);
        DefReadWriteAliases(solverOptions, "tolerance_", "tolerance", &CurveSolverOptions_::tolerance_);
        DefReadWriteAliases(solverOptions, "fitTolerance_", "fit_tolerance", &CurveSolverOptions_::fitTolerance_);
        DefReadWriteAliases(solverOptions, "initialGuess_", "initial_guess", &CurveSolverOptions_::initialGuess_);
        DefReadWriteAliases(solverOptions, "maxEvaluations_", "max_evaluations", &CurveSolverOptions_::maxEvaluations_);
        DefReadWriteAliases(solverOptions, "maxRestarts_", "max_restarts", &CurveSolverOptions_::maxRestarts_);
        DefPropertyAliases(
            solverOptions, "solveMode_", "solve_mode", [](const CurveSolverOptions_& value) { return value.solveMode_.Switch(); },
            [](CurveSolverOptions_& value, CurveSolveMode_::Value_ mode) { value.solveMode_ = CurveSolveMode_(mode); });

        auto curveDeclaration = py::class_<JointCurveDeclaration_>(m, "JointCurveDeclaration_");
        curveDeclaration.def(py::init<>());
        DefStringAliases(curveDeclaration, "curveName_", "curve_name", &JointCurveDeclaration_::curveName_);
        DefPropertyAliases(
            curveDeclaration, "instruments_", "instruments",
            [](const JointCurveDeclaration_& value) { return InstrumentHandlesToList(value.instruments_); },
            [](JointCurveDeclaration_& value, const py::iterable& instruments) { SetInstrumentHandles(&value.instruments_, instruments); });
        DefPropertyAliases(
            curveDeclaration, "knotDates_", "knot_dates", [](const JointCurveDeclaration_& value) { return DatesToList(value.knotDates_); },
            [](JointCurveDeclaration_& value, const py::iterable& dates) { SetDates(&value.knotDates_, dates); });
        DefReadWriteAliases(curveDeclaration, "targetCollateral_", "target_collateral", &JointCurveDeclaration_::targetCollateral_);
        DefReadWriteAliases(curveDeclaration, "targetTenor_", "target_tenor", &JointCurveDeclaration_::targetTenor_);
        DefReadWriteAliases(curveDeclaration, "calibrateDiscountCurve_", "calibrate_discount_curve",
                            &JointCurveDeclaration_::calibrateDiscountCurve_);
        DefReadWriteAliases(curveDeclaration, "baseLayeredOverDiscount_", "base_layered_over_discount",
                            &JointCurveDeclaration_::baseLayeredOverDiscount_);
        DefPropertyAliases(
            curveDeclaration, "parameterization_", "parameterization",
            [](const JointCurveDeclaration_& value) { return value.parameterization_.Switch(); },
            [](JointCurveDeclaration_& value, CurveParameterization_::Value_ parameterization) {
                value.parameterization_ = CurveParameterization_(parameterization);
            });
        DefPropertyAliases(
            curveDeclaration, "logDfScheme_", "log_df_scheme", [](const JointCurveDeclaration_& value) { return value.logDfScheme_.Switch(); },
            [](JointCurveDeclaration_& value, LogDfScheme_::Value_ scheme) { value.logDfScheme_ = LogDfScheme_(scheme); });
        DefReadWriteAliases(curveDeclaration, "smoothingWeight_", "smoothing_weight", &JointCurveDeclaration_::smoothingWeight_);
        DefPropertyAliases(
            curveDeclaration, "initialGuessPerNode_", "initial_guess_per_node",
            [](const JointCurveDeclaration_& value) { return DoublesToList(value.initialGuessPerNode_); },
            [](JointCurveDeclaration_& value, const py::iterable& values) { SetDoubles(&value.initialGuessPerNode_, values); });

        auto currencySpec = py::class_<JointCurrencyCurveSpec_>(m, "JointCurrencyCurveSpec_");
        currencySpec.def(py::init<>());
        DefReadWriteAliases(currencySpec, "ccy_", "ccy", &JointCurrencyCurveSpec_::ccy_);
        DefReadWriteAliases(currencySpec, "liborBasis_", "libor_basis", &JointCurrencyCurveSpec_::liborBasis_);
        DefPropertyAliases(
            currencySpec, "curves_", "curves",
            [](const JointCurrencyCurveSpec_& value) {
                py::list result;
                for (const auto& curve : value.curves_)
                    result.append(curve);
                return result;
            },
            [](JointCurrencyCurveSpec_& value, const py::iterable& curves) {
                value.curves_.clear();
                for (const auto curve : curves)
                    value.curves_.push_back(py::cast<JointCurveDeclaration_>(curve));
            });

        auto basisDeclaration = py::class_<XccyBasisCurveDeclaration_>(m, "XccyBasisCurveDeclaration_");
        basisDeclaration.def(py::init<>());
        DefStringAliases(basisDeclaration, "curveName_", "curve_name", &XccyBasisCurveDeclaration_::curveName_);
        DefPropertyAliases(
            basisDeclaration, "instruments_", "instruments",
            [](const XccyBasisCurveDeclaration_& value) { return InstrumentHandlesToList(value.instruments_); },
            [](XccyBasisCurveDeclaration_& value, const py::iterable& instruments) { SetInstrumentHandles(&value.instruments_, instruments); });
        DefPropertyAliases(
            basisDeclaration, "knotDates_", "knot_dates", [](const XccyBasisCurveDeclaration_& value) { return DatesToList(value.knotDates_); },
            [](XccyBasisCurveDeclaration_& value, const py::iterable& dates) { SetDates(&value.knotDates_, dates); });
        DefPropertyAliases(
            basisDeclaration, "parameterization_", "parameterization",
            [](const XccyBasisCurveDeclaration_& value) { return value.parameterization_.Switch(); },
            [](XccyBasisCurveDeclaration_& value, CurveParameterization_::Value_ parameterization) {
                value.parameterization_ = CurveParameterization_(parameterization);
            });
        DefReadWriteAliases(basisDeclaration, "smoothingWeight_", "smoothing_weight", &XccyBasisCurveDeclaration_::smoothingWeight_);
        DefPropertyAliases(
            basisDeclaration, "initialGuessPerNode_", "initial_guess_per_node",
            [](const XccyBasisCurveDeclaration_& value) { return DoublesToList(value.initialGuessPerNode_); },
            [](XccyBasisCurveDeclaration_& value, const py::iterable& values) { SetDoubles(&value.initialGuessPerNode_, values); });

        auto jointBuilder = py::class_<JointXccyCalibrationSpecBuilder_>(m, "JointXccyCalibrationSpecBuilder_");
        jointBuilder.def(py::init<>());
        DefReadWriteAliases(jointBuilder, "valuationTime_", "valuation_time", &JointXccyCalibrationSpecBuilder_::valuationTime_);
        DefReadWriteAliases(jointBuilder, "pair_", "pair", &JointXccyCalibrationSpecBuilder_::pair_);
        DefReadWriteAliases(jointBuilder, "collateralCurrency_", "collateral_currency", &JointXccyCalibrationSpecBuilder_::collateralCurrency_);
        DefReadWriteAliases(jointBuilder, "fxSpot_", "fx_spot", &JointXccyCalibrationSpecBuilder_::fxSpot_);
        DefReadWriteAliases(jointBuilder, "domestic_", "domestic", &JointXccyCalibrationSpecBuilder_::domestic_);
        DefReadWriteAliases(jointBuilder, "foreign_", "foreign", &JointXccyCalibrationSpecBuilder_::foreign_);
        DefReadWriteAliases(jointBuilder, "basis_", "basis", &JointXccyCalibrationSpecBuilder_::basis_);
        DefPropertyAliases(
            jointBuilder, "fixings_", "fixings", [](const JointXccyCalibrationSpecBuilder_& value) { return MutableSnapshot(value.fixings_); },
            [](JointXccyCalibrationSpecBuilder_& value, const std::shared_ptr<MarketFixingSnapshot_>& fixings) {
                value.fixings_ = ConstSnapshot(fixings);
            });
        DefReadWriteAliases(jointBuilder, "solverOptions_", "solver_options", &JointXccyCalibrationSpecBuilder_::solverOptions_);
        jointBuilder.def("Build", &JointXccyCalibrationSpecBuilder_::Build).def("build", &JointXccyCalibrationSpecBuilder_::Build);

        auto jointOptions = py::class_<JointXccyCalibrationOptions_>(m, "JointXccyCalibrationOptions_");
        jointOptions.def(py::init<>());
        DefPropertyAliases(
            jointOptions, "jacobianMode_", "jacobian_mode", [](const JointXccyCalibrationOptions_& value) { return value.jacobianMode_.Switch(); },
            [](JointXccyCalibrationOptions_& value, CurveJacobianMode_::Value_ mode) { value.jacobianMode_ = CurveJacobianMode_(mode); });
        DefReadWriteAliases(jointOptions, "computeEffJacobianInverse_", "compute_eff_jacobian_inverse",
                            &JointXccyCalibrationOptions_::computeEffJacobianInverse_);
        DefReadWriteAliases(jointOptions, "computeForwardJacobian_", "compute_forward_jacobian",
                            &JointXccyCalibrationOptions_::computeForwardJacobian_);

        auto blockRange = py::class_<CalibrationBlockRange_>(m, "CalibrationBlockRange_");
        blockRange.def(py::init<>());
        DefStringAliases(blockRange, "name_", "name", &CalibrationBlockRange_::name_);
        DefReadWriteAliases(blockRange, "offset_", "offset", &CalibrationBlockRange_::offset_);
        DefReadWriteAliases(blockRange, "size_", "size", &CalibrationBlockRange_::size_);

        auto jointDiagnostics = py::class_<JointCurveCalibrationDiagnostics_>(m, "JointCurveCalibrationDiagnostics_");
        jointDiagnostics.def(py::init<>());
        DefStringAliases(jointDiagnostics, "curveName_", "curve_name", &JointCurveCalibrationDiagnostics_::curveName_);
        DefReadWriteAliases(jointDiagnostics, "curveIndex_", "curve_index", &JointCurveCalibrationDiagnostics_::curveIndex_);
        jointDiagnostics
            .def_property_readonly("instrumentNames_",
                                   [](const JointCurveCalibrationDiagnostics_& value) {
                                       py::list result;
                                       for (const auto& name : value.instrumentNames_)
                                           result.append(std::string(name.c_str()));
                                       return result;
                                   })
            .def_property_readonly("instrument_names",
                                   [](const JointCurveCalibrationDiagnostics_& value) {
                                       py::list result;
                                       for (const auto& name : value.instrumentNames_)
                                           result.append(std::string(name.c_str()));
                                       return result;
                                   })
            .def_property_readonly("marketRates_", [](const JointCurveCalibrationDiagnostics_& value) { return DoublesToList(value.marketRates_); })
            .def_property_readonly("market_rates", [](const JointCurveCalibrationDiagnostics_& value) { return DoublesToList(value.marketRates_); })
            .def_property_readonly("modelRates_", [](const JointCurveCalibrationDiagnostics_& value) { return DoublesToList(value.modelRates_); })
            .def_property_readonly("model_rates", [](const JointCurveCalibrationDiagnostics_& value) { return DoublesToList(value.modelRates_); })
            .def_property_readonly("residuals_", [](const JointCurveCalibrationDiagnostics_& value) { return DoublesToList(value.residuals_); })
            .def_property_readonly("residuals", [](const JointCurveCalibrationDiagnostics_& value) { return DoublesToList(value.residuals_); });
        DefReadWriteAliases(jointDiagnostics, "maxAbsResidual_", "max_abs_residual", &JointCurveCalibrationDiagnostics_::maxAbsResidual_);
        DefReadWriteAliases(jointDiagnostics, "rmsResidual_", "rms_residual", &JointCurveCalibrationDiagnostics_::rmsResidual_);
        DefReadWriteAliases(jointDiagnostics, "usedApproximateFit_", "used_approximate_fit", &JointCurveCalibrationDiagnostics_::usedApproximateFit_);

        auto jointResult = py::class_<JointXccyCalibrationResult_>(m, "JointXccyCalibrationResult_");
        const auto domesticBlock = [](const JointXccyCalibrationResult_& value) {
            return std::const_pointer_cast<CurveBlock_>(JointXccyResultDomesticBlock(value));
        };
        const auto foreignBlock = [](const JointXccyCalibrationResult_& value) {
            return std::const_pointer_cast<CurveBlock_>(JointXccyResultForeignBlock(value));
        };
        const auto basisCurve = [](const JointXccyCalibrationResult_& value) {
            return std::const_pointer_cast<DiscountCurve_>(JointXccyResultBasisCurve(value));
        };
        const auto jointFxForwards = py::cpp_function(
            [](const JointXccyCalibrationResult_& value) -> const CrossCurrencyFxForwardCurve_& { return JointXccyResultFxForwards(value); },
            py::return_value_policy::reference_internal);
        const auto jointMatrix =
            py::cpp_function([](const JointXccyCalibrationResult_& value) -> const Matrix_<>& { return JointXccyResultJacobian(value); },
                             py::return_value_policy::reference_internal);
        const auto inverseMatrix =
            py::cpp_function([](const JointXccyCalibrationResult_& value) -> const Matrix_<>& {
                return JointXccyResultEffJacobianInverse(value);
            },
                             py::return_value_policy::reference_internal);
        jointResult.def_property_readonly("domesticCurveBlock_", domesticBlock).def_property_readonly("domestic_curve_block", domesticBlock);
        jointResult.def_property_readonly("foreignCurveBlock_", foreignBlock).def_property_readonly("foreign_curve_block", foreignBlock);
        jointResult.def_property_readonly("basisCurve_", basisCurve).def_property_readonly("basis_curve", basisCurve);
        jointResult.def_property_readonly("fxForwardCurve_", jointFxForwards).def_property_readonly("fx_forward_curve", jointFxForwards);
        jointResult.def_property_readonly("fixings_", [](const JointXccyCalibrationResult_& value) { return MutableSnapshot(value.fixings_); })
            .def_property_readonly("fixings", [](const JointXccyCalibrationResult_& value) { return MutableSnapshot(value.fixings_); });
        jointResult
            .def_property_readonly("domesticDiagnostics_",
                                   [](const JointXccyCalibrationResult_& value) { return ValuesToList(value.domesticDiagnostics_); })
            .def_property_readonly("domestic_diagnostics",
                                   [](const JointXccyCalibrationResult_& value) { return ValuesToList(value.domesticDiagnostics_); })
            .def_property_readonly("foreignDiagnostics_",
                                   [](const JointXccyCalibrationResult_& value) { return ValuesToList(value.foreignDiagnostics_); })
            .def_property_readonly("foreign_diagnostics",
                                   [](const JointXccyCalibrationResult_& value) { return ValuesToList(value.foreignDiagnostics_); });
        const auto jointXccyDiagnostics = py::cpp_function(
            [](const JointXccyCalibrationResult_& value) -> const CrossCurrencyCalibrationDiagnostics_& { return value.xccyDiagnostics_; },
            py::return_value_policy::reference_internal);
        jointResult.def_property_readonly("xccyDiagnostics_", jointXccyDiagnostics).def_property_readonly("xccy_diagnostics", jointXccyDiagnostics);
        jointResult
            .def_property_readonly("marketRates_",
                                   [](const JointXccyCalibrationResult_& value) { return DoublesToList(JointXccyResultMarketRates(value)); })
            .def_property_readonly("market_rates",
                                   [](const JointXccyCalibrationResult_& value) { return DoublesToList(JointXccyResultMarketRates(value)); })
            .def_property_readonly("modelRates_",
                                   [](const JointXccyCalibrationResult_& value) { return DoublesToList(JointXccyResultModelRates(value)); })
            .def_property_readonly("model_rates",
                                   [](const JointXccyCalibrationResult_& value) { return DoublesToList(JointXccyResultModelRates(value)); })
            .def_property_readonly("residuals_",
                                   [](const JointXccyCalibrationResult_& value) { return DoublesToList(JointXccyResultResiduals(value)); })
            .def_property_readonly("residuals",
                                   [](const JointXccyCalibrationResult_& value) { return DoublesToList(JointXccyResultResiduals(value)); });
        jointResult.def_property_readonly("jacobianAtSolution_", jointMatrix).def_property_readonly("jacobian_at_solution", jointMatrix);
        jointResult.def_property_readonly("effJacobianInverse_", inverseMatrix).def_property_readonly("eff_jacobian_inverse", inverseMatrix);
        jointResult
            .def_property_readonly("parameterRanges_",
                                   [](const JointXccyCalibrationResult_& value) { return ValuesToList(JointXccyResultParameterRanges(value)); })
            .def_property_readonly("parameter_ranges",
                                   [](const JointXccyCalibrationResult_& value) { return ValuesToList(JointXccyResultParameterRanges(value)); })
            .def_property_readonly("residualRanges_",
                                   [](const JointXccyCalibrationResult_& value) { return ValuesToList(JointXccyResultResidualRanges(value)); })
            .def_property_readonly("residual_ranges",
                                   [](const JointXccyCalibrationResult_& value) { return ValuesToList(JointXccyResultResidualRanges(value)); });
        jointResult.def_property_readonly("jointMaxAbsResidual_", [](const JointXccyCalibrationResult_& value) { return value.jointMaxAbsResidual_; })
            .def_property_readonly("joint_max_abs_residual", [](const JointXccyCalibrationResult_& value) { return value.jointMaxAbsResidual_; })
            .def_property_readonly("jointRmsResidual_", [](const JointXccyCalibrationResult_& value) { return value.jointRmsResidual_; })
            .def_property_readonly("joint_rms_residual", [](const JointXccyCalibrationResult_& value) { return value.jointRmsResidual_; })
            .def_property_readonly("usedApproximateFit_", [](const JointXccyCalibrationResult_& value) { return value.usedApproximateFit_; })
            .def_property_readonly("used_approximate_fit", [](const JointXccyCalibrationResult_& value) { return value.usedApproximateFit_; })
            .def_property_readonly("converged_", [](const JointXccyCalibrationResult_& value) { return value.converged_; })
            .def_property_readonly("converged", [](const JointXccyCalibrationResult_& value) { return value.converged_; })
            .def_property_readonly("solverEvaluations_", [](const JointXccyCalibrationResult_& value) { return value.solverEvaluations_; })
            .def_property_readonly("solver_evaluations", [](const JointXccyCalibrationResult_& value) { return value.solverEvaluations_; });

        m.def("CalibrateXccyMarket", py::overload_cast<const CrossCurrencyCalibrationSpec_&>(&CalibrateXccyMarket), py::arg("spec"),
              py::call_guard<py::gil_scoped_release>());
        m.def("CalibrateXccyMarket",
              py::overload_cast<const CrossCurrencyCalibrationSpec_&, const CrossCurrencyCalibrationOptions_&>(&CalibrateXccyMarket),
              py::arg("spec"), py::arg("options"), py::call_guard<py::gil_scoped_release>());
        m.def("CalibrateJointXccyMarket", py::overload_cast<const JointXccyCalibrationSpec_&>(&CalibrateJointXccyMarket), py::arg("spec"), py::call_guard<py::gil_scoped_release>());
        m.def("CalibrateJointXccyMarket",
              py::overload_cast<const JointXccyCalibrationSpec_&, const JointXccyCalibrationOptions_&>(&CalibrateJointXccyMarket), py::arg("spec"),
              py::arg("options"),
              py::call_guard<py::gil_scoped_release>());

        AddMatrixSnakeCaseAliases(m);
    }
} // anonymous namespace

void init_bindings_curve(py::module_& m) {
    init_bindings_curve_handles(m);
    init_bindings_curve_protocol(m);
    init_bindings_curve_enums(m);
    init_bindings_curve_instruments(m);
    init_bindings_curve_data(m);
    init_bindings_curve_calibration_builder(m);
    init_bindings_curve_calibration_diagnostics(m);
    init_bindings_curve_calibration_results(m);
    init_bindings_curve_xccy(m);
}

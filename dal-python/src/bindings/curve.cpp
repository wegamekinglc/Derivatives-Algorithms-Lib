//
// curve.cpp - curve calibration bindings
//

#include "bindings.h"

#include <pybind11/stl.h>

#include <atomic>
#include <chrono>
#include <limits>
#include <stdexcept>
#include <thread>
#include <type_traits>

#include <dal/curve/calibration.hpp>
#include <dal/curve/yc.hpp>
#include <dal/curve/yccomponent.hpp>
#include <dal/curve/xccycalibration.hpp>
#include <dal/curve/xccynotionalmode.hpp>
#include <dal/curve/xccypricing.hpp>
#include <dal/curve/ycpwlf.hpp>
#include <dal/math/matrix/matrixs.hpp>
#include <dal/math/optimization/underdetermined.hpp>
#include <dal/protocol/collateraltype.hpp>
#include <dal/time/datetime.hpp>
#include <dal/time/daybasis.hpp>
#include <dal/time/periodlength.hpp>
#include <dal/storage/bag.hpp>
#include <dal/storage/json.hpp>

#include <dal-public/src/curvedata.hpp>
#include <dal-public/src/curveinstrument.hpp>
#include <dal-public/src/curveprotocol.hpp>
#include <dal-public/src/curvespec.hpp>
#include <dal-public/src/xccycalibration.hpp>

using namespace Dal;

namespace {
    std::atomic<int> s_curveCalibrationGilBarrierMilliseconds{0};

    void RunCurveCalibrationGilBarrierForTesting() {
        const int milliseconds = s_curveCalibrationGilBarrierMilliseconds.exchange(0);
        if (milliseconds > 0)
            std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
    }

    template <class Class_, class Member_>
    void DefReadWriteAliases(py::class_<Class_>& cls, const char* legacyName, const char* snakeName, Member_ Class_::* member) {
        cls.def_readwrite(legacyName, member).def_readwrite(snakeName, member);
    }

    template <class Class_, class Getter_, class Setter_>
    void DefPropertyAliases(py::class_<Class_>& cls, const char* legacyName, const char* snakeName, Getter_ getter, Setter_ setter) {
        cls.def_property(legacyName, getter, setter).def_property(snakeName, getter, setter);
    }

    template <class Class_, class Getter_>
    void DefReadonlyAliases(py::class_<Class_>& cls, const char* legacyName, const char* snakeName, Getter_ getter) {
        cls.def_property_readonly(legacyName, getter).def_property_readonly(snakeName, getter);
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

    py::tuple DatesToTuple(const Vector_<Date_>& dates) {
        py::tuple result(dates.size());
        for (int i = 0; i < static_cast<int>(dates.size()); ++i)
            result[static_cast<size_t>(i)] = dates[i];
        return result;
    }

    template <class Value_> py::tuple ValuesToTuple(const Vector_<Value_>& values) {
        py::tuple result(values.size());
        for (int i = 0; i < static_cast<int>(values.size()); ++i)
            result[static_cast<size_t>(i)] = values[i];
        return result;
    }

    std::shared_ptr<DiscountCurve_> MutableCurve(const Handle_<DiscountCurve_>& curve) { return std::const_pointer_cast<DiscountCurve_>(curve); }

    std::string StdString(const String_& value) {
        return std::string(value.data(), value.size());
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

    ResolvedSingleKnotPlan_ PlanCurveCalibrationKnotsForPython(const Date_& today,
                                                               const std::vector<std::shared_ptr<YCInstrument_>>& instruments,
                                                               const py::iterable& submittedKnots,
                                                               CurveKnotPolicy_::Value_ requestedPolicy,
                                                               CurveParameterization_::Value_ parameterization) {
        Vector_<Handle_<YCInstrument_>> nativeInstruments;
        nativeInstruments.reserve(instruments.size());
        for (const auto& instrument : instruments)
            nativeInstruments.push_back(Handle_<YCInstrument_>(std::const_pointer_cast<const YCInstrument_>(instrument)));
        Vector_<Date_> nativeSubmittedKnots;
        SetDates(&nativeSubmittedKnots, submittedKnots);
        py::gil_scoped_release release;
        RunCurveCalibrationGilBarrierForTesting();
        return PlanCurveCalibrationKnots(today, nativeInstruments, nativeSubmittedKnots, CurveKnotPolicy_(requestedPolicy),
                                         CurveParameterization_(parameterization));
    }

    std::vector<std::tuple<int, std::string, DateTime_>>
    RequiredHistoricalXccyFixingsForPython(const std::vector<std::shared_ptr<CrossCurrencySwap_>>& instruments, const DateTime_& valuationTime) {
        std::vector<std::tuple<int, std::string, DateTime_>> result;
        {
            py::gil_scoped_release release;
            for (int index = 0; index < static_cast<int>(instruments.size()); ++index) {
                const auto span = instruments[index]->TimeSpan();
                const XccyCashflowPlan_ plan = BuildXccyCashflowPlan(span.first, span.second, instruments[index]->Config());
                const Vector_<FixingRequest_> required = RequiredHistoricalFixings(plan, valuationTime);
                for (const auto& item : required)
                    result.emplace_back(index, std::string(item.indexName_.c_str()), item.fixingTime_);
            }
        }
        return result;
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
        static_assert(std::is_base_of_v<Storable_, YCComponent_>);
        static_assert(std::is_base_of_v<YCComponent_, DiscountCurve_>);
        static_assert(std::is_base_of_v<Storable_, YieldCurve_>);
        static_assert(std::is_base_of_v<YieldCurve_, CurveBlock_>);
        static_assert(!std::is_base_of_v<YieldCurve_, DiscountCurve_>);
        static_assert(std::is_base_of_v<Storable_, Bag_>);

        py::class_<YCInstrument_, std::shared_ptr<YCInstrument_>>(m, "YCInstrument_");
        py::class_<YCComponent_, Storable_, std::shared_ptr<YCComponent_>>(m, "YCComponent_");
        py::class_<DiscountCurve_, YCComponent_, std::shared_ptr<DiscountCurve_>>(m, "DiscountCurve_")
            .def(
                "__call__", [](const DiscountCurve_& curve, const Date_& from, const Date_& to) { return curve(from, to); }, py::arg("from_date"),
                py::arg("to_date"))
            .def_property_readonly("name", [](const DiscountCurve_& curve) { return std::string(curve.name_.c_str()); })
            .def_property_readonly("currency", [](const DiscountCurve_& curve) { return std::string(curve.ccy_.String()); });
        py::class_<YieldCurve_, Storable_, std::shared_ptr<YieldCurve_>>(m, "YieldCurve_");
        py::class_<CurveBlock_, YieldCurve_, std::shared_ptr<CurveBlock_>>(m, "CurveBlock_")
            .def_property_readonly("name", [](const CurveBlock_& block) { return std::string(block.name_.c_str()); })
            .def_property_readonly("currency", [](const CurveBlock_& block) { return std::string(block.ccy_.String()); })
            .def_property_readonly("discount_curves",
                                   [](const CurveBlock_& block) {
                                       std::map<CollateralType_, std::shared_ptr<DiscountCurve_>> result;
                                       for (const auto& [key, curve] : block.DiscountCurves())
                                           result[key] = MutableCurve(curve);
                                       return result;
                                   })
            .def_property_readonly("forward_curves",
                                   [](const CurveBlock_& block) {
                                       std::map<PeriodLength_, std::shared_ptr<DiscountCurve_>> result;
                                       for (const auto& [key, curve] : block.ForwardCurves())
                                           result[key] = MutableCurve(curve);
                                       return result;
                                   })
            .def_property_readonly("libor_basis", [](const CurveBlock_& block) { return block.LiborBasis(); });
        py::class_<Bag_, Storable_, std::shared_ptr<Bag_>>(m, "Bag_");
        py::class_<CrossCurrencySwap_, std::shared_ptr<CrossCurrencySwap_>>(m, "CrossCurrencySwap_");
        py::class_<CrossCurrencyMarket_, std::shared_ptr<CrossCurrencyMarket_>>(m, "CrossCurrencyMarket_");

        m.def(
            "_StorableToJson",
            [](const std::shared_ptr<Storable_>& value) {
                if (!value)
                    throw std::invalid_argument("value must be a Storable_");
                String_ payload;
                {
                    py::gil_scoped_release release;
                    payload = JSON::WriteString(*value);
                }
                return py::bytes(payload.data(), payload.size());
            },
            py::arg("value"));
        m.def(
            "_StorableFromJson",
            [](const py::bytes& payload) {
                char* data = nullptr;
                Py_ssize_t length = 0;
                if (PyBytes_AsStringAndSize(payload.ptr(), &data, &length) != 0)
                    throw py::error_already_set();
                if (length < 0 || static_cast<unsigned long long>(length) >
                                      static_cast<unsigned long long>(std::numeric_limits<std::size_t>::max()))
                    throw std::overflow_error("archive payload length does not fit size_t");
                Handle_<Storable_> restored;
                {
                    py::gil_scoped_release release;
                    restored = JSON::ReadString(data, static_cast<std::size_t>(length), JSONReadOptions_());
                }
                return std::const_pointer_cast<Storable_>(restored);
            },
            py::arg("payload"));
        m.def(
            "_BagNew",
            [](const std::string& name, const py::dict& contents) {
                Bag_::map_t native;
                for (const auto& item : contents) {
                    const std::string key = py::cast<std::string>(item.first);
                    const auto value = py::cast<std::shared_ptr<Storable_>>(item.second);
                    if (!value)
                        throw std::invalid_argument("bag values must be Storable_ instances");
                    native.emplace(
                        String_(key),
                        Handle_<Storable_>(std::const_pointer_cast<const Storable_>(value)));
                }
                py::gil_scoped_release release;
                return std::make_shared<Bag_>(String_(name), native);
            },
            py::arg("name"), py::arg("contents"));
        m.def(
            "_BagContents",
            [](const Bag_& bag) {
                py::dict result;
                for (const auto& [key, value] : bag.contents_)
                    result[py::str(StdString(key))] =
                        std::const_pointer_cast<Storable_>(value);
                return result;
            },
            py::arg("value"));

        // Value types used as function parameters / return values
        py::class_<CurveCalibrationSpec_>(m, "CurveCalibrationSpec_");
        auto xccySpec = py::class_<CrossCurrencyCalibrationSpec_>(m, "CrossCurrencyCalibrationSpec_");
        DefReadonlyAliases(xccySpec, "initialGuessPerNode_", "initial_guess_per_node",
                           [](const CrossCurrencyCalibrationSpec_& value) { return DoublesToList(value.initialGuessPerNode_); });
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

        py::class_<CollateralType_>(m, "CollateralType_").def(py::init<const char*>(), py::arg("src")).def("__repr__", [](const CollateralType_& ct) {
            return std::string(ct.String());
        });

        m.def("CollateralType_OIS", &CollateralType_OIS);
        m.def("CollateralType_Libor", &CollateralType_Libor, py::arg("tenor"));

        py::class_<PeriodLength_>(m, "PeriodLength_").def(py::init<const char*>(), py::arg("iso")).def("__repr__", [](const PeriodLength_& pl) {
            return std::string(pl.String());
        });

        m.def("PeriodLength_New", [](const char* iso) { return PeriodLength_New(String_(iso)); }, py::arg("iso"));

        py::class_<DayBasis_>(m, "DayBasis_").def(py::init<const char*>(), py::arg("name")).def("__repr__", [](const DayBasis_& db) {
            return std::string(db.String());
        });

        m.def("DayBasis_New", [](const char* name) { return DayBasis_New(String_(name)); }, py::arg("name"));

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

        m.def("RateLegConvention_New", &RateLegConvention_New, py::arg("freq"), py::arg("basis"));

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

        m.def("RateIndexConvention_New", &RateIndexConvention_New, py::arg("forecast_tenor"), py::arg("basis"), py::arg("collateral"),
              py::arg("use_projection_curve") = false);

        auto currencyPair = py::class_<CurrencyPair_>(m, "CurrencyPair_");
        DefReadWriteAliases(currencyPair, "domestic_", "domestic", &CurrencyPair_::domestic_);
        DefReadWriteAliases(currencyPair, "foreign_", "foreign", &CurrencyPair_::foreign_);

        m.def(
            "CurrencyPair_New", [](const char* domestic, const char* foreign) { return CurrencyPair_New(String_(domestic), String_(foreign)); },
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

        m.def(
            "Deposit_New",
            [](const Date_& tradeDate, const Date_& start, const Date_& maturity, double marketRate,
               const RateIndexConvention_& convention) -> std::shared_ptr<YCInstrument_> {
                return std::const_pointer_cast<YCInstrument_>(Handle_<YCInstrument_>(DepositNew(tradeDate, start, maturity, marketRate, convention)));
            },
            py::arg("trade_date"), py::arg("start"), py::arg("maturity"), py::arg("market_rate"), py::arg("convention"));

        m.def(
            "FRA_New",
            [](const Date_& tradeDate, const Date_& start, const Date_& maturity, double marketRate,
               const RateIndexConvention_& convention) -> std::shared_ptr<YCInstrument_> {
                return std::const_pointer_cast<YCInstrument_>(Handle_<YCInstrument_>(FRANew(tradeDate, start, maturity, marketRate, convention)));
            },
            py::arg("trade_date"), py::arg("start"), py::arg("maturity"), py::arg("market_rate"), py::arg("convention"));

        m.def(
            "Future_New",
            [](const Date_& tradeDate, const Date_& start, const Date_& maturity, double marketRate, const RateIndexConvention_& convention,
               double convexityAdjustment) -> std::shared_ptr<YCInstrument_> {
                return std::const_pointer_cast<YCInstrument_>(
                    Handle_<YCInstrument_>(FutureNew(tradeDate, start, maturity, marketRate, convention, convexityAdjustment)));
            },
            py::arg("trade_date"), py::arg("start"), py::arg("maturity"), py::arg("market_rate"), py::arg("convention"),
            py::arg("convexity_adjustment") = 0.0);

        m.def(
            "Swap_New",
            [](const Date_& tradeDate, const Date_& start, const Date_& maturity, double marketRate, const RateLegConvention_& fixedLeg,
               const RateIndexConvention_& floatIndex, const RateLegConvention_& floatLeg) -> std::shared_ptr<YCInstrument_> {
                return std::const_pointer_cast<YCInstrument_>(
                    Handle_<YCInstrument_>(SwapNew(tradeDate, start, maturity, marketRate, fixedLeg, floatIndex, floatLeg)));
            },
            py::arg("trade_date"), py::arg("start"), py::arg("maturity"), py::arg("market_rate"), py::arg("fixed_leg"), py::arg("float_index"),
            py::arg("float_leg"));

        m.def(
            "OISSwap_New",
            [](const Date_& tradeDate, const Date_& start, const Date_& maturity, double marketRate, const RateLegConvention_& fixedLeg,
               const RateIndexConvention_& overnightIndex, const RateLegConvention_& floatLeg) -> std::shared_ptr<YCInstrument_> {
                return std::const_pointer_cast<YCInstrument_>(
                    Handle_<YCInstrument_>(OISSwapNew(tradeDate, start, maturity, marketRate, fixedLeg, overnightIndex, floatLeg)));
            },
            py::arg("trade_date"), py::arg("start"), py::arg("maturity"), py::arg("market_rate"), py::arg("fixed_leg"), py::arg("overnight_index"),
            py::arg("float_leg"));

        m.def(
            "BasisSwap_New",
            [](const Date_& tradeDate, const Date_& start, const Date_& maturity, double marketRate, const RateIndexConvention_& spreadIndex,
               const RateLegConvention_& spreadLeg, const RateIndexConvention_& refIndex,
               const RateLegConvention_& refLeg) -> std::shared_ptr<YCInstrument_> {
                return std::const_pointer_cast<YCInstrument_>(
                    Handle_<YCInstrument_>(BasisSwapNew(tradeDate, start, maturity, marketRate, spreadIndex, spreadLeg, refIndex, refLeg)));
            },
            py::arg("trade_date"), py::arg("start"), py::arg("maturity"), py::arg("market_rate"), py::arg("spread_index"), py::arg("spread_leg"),
            py::arg("ref_index"), py::arg("ref_leg"));

        m.def(
            "CrossCurrencySwap_New",
            [](const Date_& tradeDate, const Date_& start, const Date_& maturity, double marketRate, const CurrencyPair_& currencies,
               double domesticNotional, double foreignNotional, const RateLegConvention_& domesticLeg, const RateIndexConvention_& domesticIndex,
               const RateLegConvention_& foreignLeg, const RateIndexConvention_& foreignIndex) -> std::shared_ptr<CrossCurrencySwap_> {
                return std::const_pointer_cast<CrossCurrencySwap_>(
                    Handle_<CrossCurrencySwap_>(CrossCurrencySwapNew(tradeDate, start, maturity, marketRate, currencies, domesticNotional,
                                                                     foreignNotional, domesticLeg, domesticIndex, foreignLeg, foreignIndex)));
            },
            py::arg("trade_date"), py::arg("start"), py::arg("maturity"), py::arg("market_rate"), py::arg("currencies"),
            py::arg("domestic_notional") = 100.0, py::arg("foreign_notional") = 100.0, py::arg("domestic_leg") = RateLegConvention_(),
            py::arg("domestic_index") = RateIndexConvention_(), py::arg("foreign_leg") = RateLegConvention_(),
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
        py::class_<Tape::DiscountPWC_<double>, DiscountCurve_, std::shared_ptr<Tape::DiscountPWC_<double>>>(m, "DiscountPWC_")
            .def_property_readonly("knot_dates", [](const Tape::DiscountPWC_<double>& curve) { return DatesToList(curve.KnotDates()); })
            .def_property_readonly("right_forwards", [](const Tape::DiscountPWC_<double>& curve) { return DoublesToList(curve.FRight()); })
            .def_property_readonly("base", [](const Tape::DiscountPWC_<double>& curve) { return MutableCurve(curve.Base()); });

        py::class_<Tape::DiscountPWLF_<double>, DiscountCurve_, std::shared_ptr<Tape::DiscountPWLF_<double>>>(m, "DiscountPWLF_")
            .def_property_readonly("knot_dates", [](const Tape::DiscountPWLF_<double>& curve) { return DatesToList(curve.KnotDates()); })
            .def_property_readonly("left_forwards", [](const Tape::DiscountPWLF_<double>& curve) { return DoublesToList(curve.FLeft()); })
            .def_property_readonly("right_forwards", [](const Tape::DiscountPWLF_<double>& curve) { return DoublesToList(curve.FRight()); })
            .def_property_readonly("base", [](const Tape::DiscountPWLF_<double>& curve) { return MutableCurve(curve.Base()); });

        py::class_<DiscountZeroRate_, DiscountCurve_, std::shared_ptr<DiscountZeroRate_>>(m, "DiscountZeroRate_")
            .def_property_readonly("anchor_date", [](const DiscountZeroRate_& curve) { return curve.AnchorDate(); })
            .def_property_readonly("node_dates",
                                   [](const DiscountZeroRate_& curve) {
                                       py::list result;
                                       for (const auto& date : curve.NodeDates())
                                           result.append(date);
                                       return result;
                                   })
            .def_property_readonly("zero_rates",
                                   [](const DiscountZeroRate_& curve) {
                                       py::list result;
                                       for (const auto rate : curve.NodeZeroRates())
                                           result.append(rate);
                                       return result;
                                   })
            .def_property_readonly("day_count", [](const DiscountZeroRate_& curve) { return std::string(curve.DayCount().String()); })
            .def_property_readonly("log_df_scheme", [](const DiscountZeroRate_& curve) { return curve.Scheme().Switch(); })
            .def_property_readonly("base", [](const DiscountZeroRate_& curve) { return MutableCurve(curve.Base()); });

        py::class_<DiscountLogDF_, DiscountCurve_, std::shared_ptr<DiscountLogDF_>>(m, "DiscountLogDF_")
            .def_property_readonly("node_dates", [](const DiscountLogDF_& curve) { return DatesToList(curve.NodeDates()); })
            .def_property_readonly("log_discount_factors", [](const DiscountLogDF_& curve) { return DoublesToList(curve.NodeLogDF()); })
            .def_property_readonly("day_count", [](const DiscountLogDF_& curve) { return std::string(curve.DayCount().String()); })
            .def_property_readonly("log_df_scheme", [](const DiscountLogDF_& curve) { return curve.Scheme().Switch(); })
            .def_property_readonly("base", [](const DiscountLogDF_& curve) { return MutableCurve(curve.Base()); });

        m.def(
            "DiscountPWC_New",
            [](const char* name, const char* ccy, const py::iterable& knotDatesPy, const py::iterable& rightForwardsPy,
               const std::shared_ptr<DiscountCurve_>& base) -> std::shared_ptr<DiscountCurve_> {
                Vector_<Date_> knotDates;
                SetDates(&knotDates, knotDatesPy);
                Vector_<> rightForwards;
                SetDoubles(&rightForwards, rightForwardsPy);
                const Handle_<DiscountCurve_> baseHandle(std::const_pointer_cast<const DiscountCurve_>(base));
                return MutableCurve(DiscountPWCNew(String_(name), String_(ccy), knotDates, rightForwards, baseHandle));
            },
            py::arg("name"), py::arg("ccy"), py::arg("knot_dates"), py::arg("right_forwards"), py::arg("base") = std::shared_ptr<DiscountCurve_>());

        m.def(
            "DiscountZeroRate_New",
            [](const char* name, const char* ccy, const Date_& anchorDate, const py::iterable& nodeDatesPy, const py::iterable& zeroRatesPy,
               const DayBasis_& dayCount, LogDfScheme_::Value_ logDfScheme,
               const std::shared_ptr<DiscountCurve_>& base) -> std::shared_ptr<DiscountCurve_> {
                Vector_<Date_> nodeDates;
                SetDates(&nodeDates, nodeDatesPy);
                Vector_<> zeroRates;
                SetDoubles(&zeroRates, zeroRatesPy);
                Handle_<DiscountCurve_> baseHandle(std::const_pointer_cast<const DiscountCurve_>(base));
                return std::const_pointer_cast<DiscountCurve_>(DiscountZeroRateNew(String_(name), String_(ccy), anchorDate, nodeDates, zeroRates,
                                                                                   dayCount, LogDfScheme_(logDfScheme), baseHandle));
            },
            py::arg("name"), py::arg("ccy"), py::arg("anchor_date"), py::arg("node_dates"), py::arg("zero_rates"),
            py::arg("day_count") = DayBasis_("ACT_365F"), py::arg("log_df_scheme") = LogDfScheme_::Value_::LOG_LINEAR,
            py::arg("base") = std::shared_ptr<DiscountCurve_>());

        m.def(
            "DiscountPWLF_New",
            [](const char* name, const char* ccy, const py::iterable& knotDatesPy, const py::iterable& fwdRatesPy,
               const std::shared_ptr<DiscountCurve_>& base) -> std::shared_ptr<DiscountCurve_> {
                Vector_<Date_> knotDates;
                SetDates(&knotDates, knotDatesPy);
                Vector_<> fwdRates;
                SetDoubles(&fwdRates, fwdRatesPy);
                Handle_<DiscountCurve_> baseHandle(std::const_pointer_cast<const DiscountCurve_>(base));
                return std::const_pointer_cast<DiscountCurve_>(DiscountPWLFNew(String_(name), String_(ccy), knotDates, fwdRates, baseHandle));
            },
            py::arg("name"), py::arg("ccy"), py::arg("knot_dates"), py::arg("fwd_rates"), py::arg("base") = std::shared_ptr<DiscountCurve_>());

        m.def(
            "DiscountPWLF_New",
            [](const char* name, const char* ccy, const py::iterable& knotDatesPy, const py::iterable& leftForwardsPy,
               const py::iterable& rightForwardsPy, const std::shared_ptr<DiscountCurve_>& base) -> std::shared_ptr<DiscountCurve_> {
                Vector_<Date_> knotDates;
                SetDates(&knotDates, knotDatesPy);
                Vector_<> leftForwards;
                SetDoubles(&leftForwards, leftForwardsPy);
                Vector_<> rightForwards;
                SetDoubles(&rightForwards, rightForwardsPy);
                const Handle_<DiscountCurve_> baseHandle(std::const_pointer_cast<const DiscountCurve_>(base));
                return MutableCurve(DiscountPWLFNew(String_(name), String_(ccy), knotDates, leftForwards, rightForwards, baseHandle));
            },
            py::arg("name"), py::arg("ccy"), py::arg("knot_dates"), py::arg("left_forwards"), py::arg("right_forwards"),
            py::arg("base") = std::shared_ptr<DiscountCurve_>());

        m.def(
            "DiscountLogDF_New",
            [](const char* name, const char* ccy, const py::iterable& nodeDatesPy, const py::iterable& logDiscountFactorsPy,
               const DayBasis_& dayCount, LogDfScheme_::Value_ logDfScheme,
               const std::shared_ptr<DiscountCurve_>& base) -> std::shared_ptr<DiscountCurve_> {
                Vector_<Date_> nodeDates;
                SetDates(&nodeDates, nodeDatesPy);
                Vector_<> logDiscountFactors;
                SetDoubles(&logDiscountFactors, logDiscountFactorsPy);
                MappedDiscountCurveOptions_ options;
                options.dayCount_ = dayCount;
                options.logDfScheme_ = LogDfScheme_(logDfScheme);
                options.base_ = Handle_<DiscountCurve_>(std::const_pointer_cast<const DiscountCurve_>(base));
                return MutableCurve(DiscountLogDFNew(String_(name), String_(ccy), nodeDates, logDiscountFactors, options));
            },
            py::arg("name"), py::arg("ccy"), py::arg("node_dates"), py::arg("log_discount_factors"), py::kw_only(),
            py::arg("day_count") = DayBasis_("ACT_365F"), py::arg("log_df_scheme") = LogDfScheme_::Value_::LOG_LINEAR,
            py::arg("base") = std::shared_ptr<DiscountCurve_>());

        m.def(
            "CurveBlock_New",
            [](const std::shared_ptr<DiscountCurve_>& dc, const DayBasis_& liborBasis) -> std::shared_ptr<CurveBlock_> {
                Handle_<DiscountCurve_> dcHandle(std::const_pointer_cast<const DiscountCurve_>(dc));
                return std::const_pointer_cast<CurveBlock_>(CurveBlockNew(dcHandle, liborBasis));
            },
            py::arg("discount_curve"), py::arg("libor_basis") = DayBasis_("ACT_365F"));

        m.def(
            "CurveBlock_New",
            [](const char* name, const char* ccy, const std::map<CollateralType_, std::shared_ptr<DiscountCurve_>>& discountsPy,
               const std::map<PeriodLength_, std::shared_ptr<DiscountCurve_>>& forwardsPy,
               const DayBasis_& liborBasis) -> std::shared_ptr<CurveBlock_> {
                std::map<CollateralType_, Handle_<DiscountCurve_>> discounts;
                for (auto& kv : discountsPy)
                    discounts[kv.first] = Handle_<DiscountCurve_>(std::const_pointer_cast<const DiscountCurve_>(kv.second));
                std::map<PeriodLength_, Handle_<DiscountCurve_>> forwards;
                for (auto& kv : forwardsPy)
                    forwards[kv.first] = Handle_<DiscountCurve_>(std::const_pointer_cast<const DiscountCurve_>(kv.second));
                return std::const_pointer_cast<CurveBlock_>(CurveBlockNew(String_(name), String_(ccy), discounts, forwards, liborBasis));
            },
            py::arg("name"), py::arg("ccy"), py::arg("discounts"), py::arg("forwards"), py::arg("libor_basis"));
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

        py::enum_<CurveKnotPolicy_::Value_>(m, "CurveKnotPolicy")
            .value("INPUT", CurveKnotPolicy_::Value_::INPUT)
            .value("INSTRUMENTS", CurveKnotPolicy_::Value_::INSTRUMENTS)
            .value("AUGMENTED", CurveKnotPolicy_::Value_::AUGMENTED);

        py::enum_<CurveKnotOriginKind_::Value_>(m, "CurveKnotOriginKind")
            .value("INPUT", CurveKnotOriginKind_::Value_::INPUT)
            .value("INSTRUMENT_START", CurveKnotOriginKind_::Value_::INSTRUMENT_START)
            .value("INSTRUMENT_END", CurveKnotOriginKind_::Value_::INSTRUMENT_END)
            .value("SYNTHETIC_ANCHOR", CurveKnotOriginKind_::Value_::SYNTHETIC_ANCHOR);

        py::enum_<CurveKnotCandidateDisposition_::Value_>(m, "CurveKnotCandidateDisposition")
            .value("ADDED", CurveKnotCandidateDisposition_::Value_::ADDED)
            .value("DUPLICATE", CurveKnotCandidateDisposition_::Value_::DUPLICATE)
            .value("FILTERED_NOT_AFTER_TODAY", CurveKnotCandidateDisposition_::Value_::FILTERED_NOT_AFTER_TODAY);

        py::enum_<CurveFreeParameterComponent_::Value_>(m, "CurveFreeParameterComponent")
            .value("RIGHT_FORWARD", CurveFreeParameterComponent_::Value_::RIGHT_FORWARD)
            .value("LEFT_FORWARD", CurveFreeParameterComponent_::Value_::LEFT_FORWARD)
            .value("ZERO_RATE", CurveFreeParameterComponent_::Value_::ZERO_RATE)
            .value("LOG_DISCOUNT_FACTOR", CurveFreeParameterComponent_::Value_::LOG_DISCOUNT_FACTOR);

        py::enum_<CurveJacobianMode_::Value_>(m, "CurveJacobianMode")
            .value("BUMPED", CurveJacobianMode_::Value_::BUMPED)
            .value("ANALYTIC", CurveJacobianMode_::Value_::ANALYTIC);

        py::enum_<AnalyticIneligibilityReason_::Value_>(m, "AnalyticIneligibilityReason")
            .value("DISCOUNT_TARGET_REQUIRED", AnalyticIneligibilityReason_::Value_::DISCOUNT_TARGET_REQUIRED)
            .value("TEMPLATED_RATE_UNAVAILABLE", AnalyticIneligibilityReason_::Value_::TEMPLATED_RATE_UNAVAILABLE)
            .value("PROJECTION_NOT_ALLOWED", AnalyticIneligibilityReason_::Value_::PROJECTION_NOT_ALLOWED)
            .value("PROJECTION_REQUIRED", AnalyticIneligibilityReason_::Value_::PROJECTION_REQUIRED)
            .value("TRADE_DATE_MISMATCH", AnalyticIneligibilityReason_::Value_::TRADE_DATE_MISMATCH)
            .value("LIBOR_BASIS_UNSUPPORTED", AnalyticIneligibilityReason_::Value_::LIBOR_BASIS_UNSUPPORTED)
            .value("DISCOUNT_ROUTE_MISSING", AnalyticIneligibilityReason_::Value_::DISCOUNT_ROUTE_MISSING)
            .value("PROJECTION_ROUTE_MISSING", AnalyticIneligibilityReason_::Value_::PROJECTION_ROUTE_MISSING)
            .value("PAIR_CURRENCY_MISMATCH", AnalyticIneligibilityReason_::Value_::PAIR_CURRENCY_MISMATCH)
            .value("COUPON_PLAN_EMPTY", AnalyticIneligibilityReason_::Value_::COUPON_PLAN_EMPTY)
            .value("NOTIONAL_MODE_UNSUPPORTED", AnalyticIneligibilityReason_::Value_::NOTIONAL_MODE_UNSUPPORTED)
            .value("RESET_MAPPING_INVALID", AnalyticIneligibilityReason_::Value_::RESET_MAPPING_INVALID)
            .value("CASHFLOW_PLAN_UNSUPPORTED", AnalyticIneligibilityReason_::Value_::CASHFLOW_PLAN_UNSUPPORTED);

        py::enum_<XccyNotionalMode_::Value_>(m, "XccyNotionalMode")
            .value("FIXED", XccyNotionalMode_::Value_::FIXED)
            .value("RESETTABLE", XccyNotionalMode_::Value_::RESETTABLE)
            .value("MARK_TO_MARKET", XccyNotionalMode_::Value_::MARK_TO_MARKET);

        py::enum_<LogDfScheme_::Value_>(m, "LogDfScheme")
            .value("LOG_LINEAR", LogDfScheme_::Value_::LOG_LINEAR)
            .value("LOG_CUBIC_NATURAL", LogDfScheme_::Value_::LOG_CUBIC_NATURAL)
            .value("MIXED", LogDfScheme_::Value_::MIXED);
    }

    void init_bindings_curve_planning(py::module_& m) {
        auto eligibilityIssue = py::class_<AnalyticEligibilityIssue_>(m, "AnalyticEligibilityIssue_");
        DefReadonlyAliases(eligibilityIssue, "reason_", "reason", [](const AnalyticEligibilityIssue_& value) { return value.reason_.Switch(); });
        DefReadonlyAliases(eligibilityIssue, "group_", "group",
                           [](const AnalyticEligibilityIssue_& value) { return std::string(value.group_.c_str()); });
        DefReadonlyAliases(eligibilityIssue, "declarationIndex_", "declaration_index",
                           [](const AnalyticEligibilityIssue_& value) { return value.declarationIndex_; });
        DefReadonlyAliases(eligibilityIssue, "instrumentIndex_", "instrument_index",
                           [](const AnalyticEligibilityIssue_& value) { return value.instrumentIndex_; });
        DefReadonlyAliases(eligibilityIssue, "resetIndex_", "reset_index", [](const AnalyticEligibilityIssue_& value) { return value.resetIndex_; });
        DefReadonlyAliases(eligibilityIssue, "nativeMessage_", "native_message",
                           [](const AnalyticEligibilityIssue_& value) { return std::string(value.nativeMessage_.c_str()); });

        auto eligibilityReport = py::class_<AnalyticEligibilityReport_>(m, "AnalyticEligibilityReport_");
        DefReadonlyAliases(eligibilityReport, "eligible_", "eligible", [](const AnalyticEligibilityReport_& value) { return value.eligible_; });
        DefReadonlyAliases(eligibilityReport, "issues_", "issues",
                           [](const AnalyticEligibilityReport_& value) { return ValuesToTuple(value.issues_); });

        auto origin = py::class_<CurveKnotOrigin_>(m, "CurveKnotOrigin_");
        DefReadonlyAliases(origin, "kind_", "kind", [](const CurveKnotOrigin_& value) { return value.kind_.Switch(); });
        DefReadonlyAliases(origin, "inputKnotIndex_", "input_knot_index", [](const CurveKnotOrigin_& value) { return value.inputKnotIndex_; });
        DefReadonlyAliases(origin, "instrumentInputIndex_", "instrument_input_index",
                           [](const CurveKnotOrigin_& value) { return value.instrumentInputIndex_; });

        auto candidate = py::class_<CurveKnotCandidate_>(m, "CurveKnotCandidate_");
        DefReadonlyAliases(candidate, "ordinal_", "ordinal", [](const CurveKnotCandidate_& value) { return value.ordinal_; });
        DefReadonlyAliases(candidate, "date_", "date", [](const CurveKnotCandidate_& value) { return value.date_; });
        DefReadonlyAliases(candidate, "origin_", "origin", [](const CurveKnotCandidate_& value) { return value.origin_; });
        DefReadonlyAliases(candidate, "disposition_", "disposition", [](const CurveKnotCandidate_& value) { return value.disposition_.Switch(); });
        DefReadonlyAliases(candidate, "resolvedIndex_", "resolved_index", [](const CurveKnotCandidate_& value) -> py::object {
            if (value.resolvedIndex_ < 0)
                return py::none();
            return py::int_(value.resolvedIndex_);
        });

        auto node = py::class_<ResolvedCurveKnotNode_>(m, "ResolvedCurveKnotNode_");
        DefReadonlyAliases(node, "date_", "date", [](const ResolvedCurveKnotNode_& value) { return value.date_; });
        DefReadonlyAliases(node, "origins_", "origins", [](const ResolvedCurveKnotNode_& value) { return ValuesToTuple(value.origins_); });

        auto parameter = py::class_<CurveFreeParameter_>(m, "CurveFreeParameter_");
        DefReadonlyAliases(parameter, "date_", "date", [](const CurveFreeParameter_& value) { return value.date_; });
        DefReadonlyAliases(parameter, "component_", "component", [](const CurveFreeParameter_& value) { return value.component_.Switch(); });

        auto counts = py::class_<ResolvedCurveKnotCounts_>(m, "ResolvedCurveKnotCounts_");
        DefReadonlyAliases(counts, "submittedKnots_", "submitted_knots", [](const ResolvedCurveKnotCounts_& value) { return value.submittedKnots_; });
        DefReadonlyAliases(counts, "instrumentCandidates_", "instrument_candidates",
                           [](const ResolvedCurveKnotCounts_& value) { return value.instrumentCandidates_; });
        DefReadonlyAliases(counts, "resolvedDeclaredNodes_", "resolved_declared_nodes",
                           [](const ResolvedCurveKnotCounts_& value) { return value.resolvedDeclaredNodes_; });
        DefReadonlyAliases(counts, "storageNodes_", "storage_nodes", [](const ResolvedCurveKnotCounts_& value) { return value.storageNodes_; });
        DefReadonlyAliases(counts, "freeParameters_", "free_parameters", [](const ResolvedCurveKnotCounts_& value) { return value.freeParameters_; });

        auto plan = py::class_<ResolvedSingleKnotPlan_>(m, "ResolvedSingleKnotPlan_");
        DefReadonlyAliases(plan, "plannerVersion_", "planner_version", [](const ResolvedSingleKnotPlan_& value) { return value.plannerVersion_; });
        DefReadonlyAliases(plan, "requestedPolicy_", "requested_policy",
                           [](const ResolvedSingleKnotPlan_& value) { return value.requestedPolicy_.Switch(); });
        DefReadonlyAliases(plan, "executionPolicy_", "execution_policy",
                           [](const ResolvedSingleKnotPlan_& value) { return value.executionPolicy_.Switch(); });
        DefReadonlyAliases(plan, "submittedKnotDates_", "submitted_knot_dates",
                           [](const ResolvedSingleKnotPlan_& value) { return DatesToTuple(value.submittedKnotDates_); });
        DefReadonlyAliases(plan, "candidateTrace_", "candidate_trace",
                           [](const ResolvedSingleKnotPlan_& value) { return ValuesToTuple(value.candidateTrace_); });
        DefReadonlyAliases(plan, "resolvedDeclaredNodes_", "resolved_declared_nodes",
                           [](const ResolvedSingleKnotPlan_& value) { return ValuesToTuple(value.resolvedDeclaredNodes_); });
        DefReadonlyAliases(plan, "storageNodes_", "storage_nodes",
                           [](const ResolvedSingleKnotPlan_& value) { return ValuesToTuple(value.storageNodes_); });
        DefReadonlyAliases(plan, "freeParameters_", "free_parameters",
                           [](const ResolvedSingleKnotPlan_& value) { return ValuesToTuple(value.freeParameters_); });
        DefReadonlyAliases(plan, "anchorAdded_", "anchor_added", [](const ResolvedSingleKnotPlan_& value) { return value.anchorAdded_; });
        DefReadonlyAliases(plan, "counts_", "counts", [](const ResolvedSingleKnotPlan_& value) { return value.counts_; });

        auto executionCounts = py::class_<ExecutionSingleKnotCounts_>(m, "ExecutionSingleKnotCounts_");
        DefReadonlyAliases(executionCounts, "resolvedDeclaredNodes_", "resolved_declared_nodes",
                           [](const ExecutionSingleKnotCounts_& value) { return value.resolvedDeclaredNodes_; });
        DefReadonlyAliases(executionCounts, "storageNodes_", "storage_nodes",
                           [](const ExecutionSingleKnotCounts_& value) { return value.storageNodes_; });
        DefReadonlyAliases(executionCounts, "freeParameters_", "free_parameters",
                           [](const ExecutionSingleKnotCounts_& value) { return value.freeParameters_; });

        auto identity = py::class_<ExecutionSingleKnotIdentity_>(m, "ExecutionSingleKnotIdentity_");
        DefReadonlyAliases(identity, "identityVersion_", "identity_version",
                           [](const ExecutionSingleKnotIdentity_& value) { return value.identityVersion_; });
        DefReadonlyAliases(identity, "executionPolicy_", "execution_policy",
                           [](const ExecutionSingleKnotIdentity_& value) { return value.executionPolicy_.Switch(); });
        DefReadonlyAliases(identity, "today_", "today", [](const ExecutionSingleKnotIdentity_& value) { return value.today_; });
        DefReadonlyAliases(identity, "parameterization_", "parameterization",
                           [](const ExecutionSingleKnotIdentity_& value) { return value.parameterization_.Switch(); });
        DefReadonlyAliases(identity, "logDfScheme_", "log_df_scheme", [](const ExecutionSingleKnotIdentity_& value) -> py::object {
            if (!value.logDfScheme_)
                return py::none();
            return py::cast(value.logDfScheme_->Switch());
        });
        DefReadonlyAliases(identity, "resolvedDeclaredDates_", "resolved_declared_dates",
                           [](const ExecutionSingleKnotIdentity_& value) { return DatesToTuple(value.resolvedDeclaredDates_); });
        DefReadonlyAliases(identity, "storageDates_", "storage_dates",
                           [](const ExecutionSingleKnotIdentity_& value) { return DatesToTuple(value.storageDates_); });
        DefReadonlyAliases(identity, "freeParameters_", "free_parameters",
                           [](const ExecutionSingleKnotIdentity_& value) { return ValuesToTuple(value.freeParameters_); });
        DefReadonlyAliases(identity, "counts_", "counts", [](const ExecutionSingleKnotIdentity_& value) { return value.counts_; });

        auto options = py::class_<CurveCalibrationOptions_>(m, "CurveCalibrationOptions_");
        options.def(py::init<>());
        DefPropertyAliases(
            options, "jacobianMode_", "jacobian_mode", [](const CurveCalibrationOptions_& value) { return value.jacobianMode_.Switch(); },
            [](CurveCalibrationOptions_& value, CurveJacobianMode_::Value_ mode) { value.jacobianMode_ = CurveJacobianMode_(mode); });
        DefReadWriteAliases(options, "computeEffJacobianInverse_", "compute_eff_jacobian_inverse",
                            &CurveCalibrationOptions_::computeEffJacobianInverse_);
        DefReadWriteAliases(options, "computeForwardJacobian_", "compute_forward_jacobian", &CurveCalibrationOptions_::computeForwardJacobian_);

        m.def("PlanCurveCalibrationKnots", &PlanCurveCalibrationKnotsForPython, py::arg("today"), py::arg("instruments"), py::arg("submitted_knots"),
              py::arg("requested_policy"), py::arg("parameterization"));
        m.def(
            "InspectCurveCalibrationExecutionIdentity",
            [](const CurveCalibrationSpec_& finalInputSpec) {
                py::gil_scoped_release release;
                RunCurveCalibrationGilBarrierForTesting();
                return InspectCurveCalibrationExecutionIdentity(finalInputSpec);
            },
            py::arg("final_input_spec"));
        m.def(
            "ResolveCurveCalibrationInitialGuess",
            [](const CurveCalibrationSpec_& finalInputSpec) {
                Vector_<> resolved;
                {
                    py::gil_scoped_release release;
                    resolved = ResolveCurveCalibrationInitialGuess(finalInputSpec);
                }
                return DoublesToList(resolved);
            },
            py::arg("final_input_spec"));
        m.def(
            "ValidateSingleCurveAnalyticEligibility",
            [](const CurveCalibrationSpec_& spec) {
                py::gil_scoped_release release;
                RunCurveCalibrationGilBarrierForTesting();
                return ValidateSingleCurveAnalyticEligibility(spec);
            },
            py::arg("spec"));
        m.def("_CurveCalibrationGilBarrier_EnableForTesting", [](int milliseconds) {
            if (milliseconds <= 0)
                throw std::invalid_argument("curve-calibration GIL barrier duration must be positive");
            s_curveCalibrationGilBarrierMilliseconds.store(milliseconds);
        });
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
            .def_property(
                "solveMode_", [](const CurveCalibrationSpecBuilder_& b) { return b.solveMode_.Switch(); },
                [](CurveCalibrationSpecBuilder_& b, CurveSolveMode_::Value_ v) { b.solveMode_ = CurveSolveMode_(v); })
            .def_property(
                "parameterization_", [](const CurveCalibrationSpecBuilder_& b) { return b.parameterization_.Switch(); },
                [](CurveCalibrationSpecBuilder_& b, CurveParameterization_::Value_ v) { b.parameterization_ = CurveParameterization_(v); })
            .def_property(
                "knotPolicy_", [](const CurveCalibrationSpecBuilder_& b) { return b.knotPolicy_.Switch(); },
                [](CurveCalibrationSpecBuilder_& b, CurveKnotPolicy_::Value_ v) { b.knotPolicy_ = CurveKnotPolicy_(v); })
            .def_property(
                "knot_policy", [](const CurveCalibrationSpecBuilder_& b) { return b.knotPolicy_.Switch(); },
                [](CurveCalibrationSpecBuilder_& b, CurveKnotPolicy_::Value_ v) { b.knotPolicy_ = CurveKnotPolicy_(v); })
            .def_property(
                "logDfScheme_", [](const CurveCalibrationSpecBuilder_& b) { return b.logDfScheme_.Switch(); },
                [](CurveCalibrationSpecBuilder_& b, LogDfScheme_::Value_ v) { b.logDfScheme_ = LogDfScheme_(v); })
            .def_property(
                "instruments_",
                [](const CurveCalibrationSpecBuilder_& b) -> py::list {
                    py::list result;
                    for (auto& inst : b.instruments_)
                        result.append(std::const_pointer_cast<YCInstrument_>(Handle_<YCInstrument_>(inst)));
                    return result;
                },
                [](CurveCalibrationSpecBuilder_& b, const py::iterable& instruments) {
                    b.instruments_.clear();
                    for (auto item : instruments) {
                        auto handle = py::cast<std::shared_ptr<YCInstrument_>>(item);
                        b.instruments_.push_back(Handle_<YCInstrument_>(std::const_pointer_cast<const YCInstrument_>(handle)));
                    }
                })
            .def_property(
                "knotDates_",
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
            .def_property(
                "initialGuessPerNode_", [](const CurveCalibrationSpecBuilder_& b) { return DoublesToList(b.initialGuessPerNode_); },
                [](CurveCalibrationSpecBuilder_& b, const py::iterable& values) { SetDoubles(&b.initialGuessPerNode_, values); })
            .def_property(
                "initial_guess_per_node", [](const CurveCalibrationSpecBuilder_& b) { return DoublesToList(b.initialGuessPerNode_); },
                [](CurveCalibrationSpecBuilder_& b, const py::iterable& values) { SetDoubles(&b.initialGuessPerNode_, values); })
            .def_property("discountCurves_", nullptr,
                          [](CurveCalibrationSpecBuilder_& b, const std::map<CollateralType_, std::shared_ptr<DiscountCurve_>>& curves) {
                              b.discountCurves_.clear();
                              for (auto& kv : curves)
                                  b.discountCurves_[kv.first] = Handle_<DiscountCurve_>(std::const_pointer_cast<const DiscountCurve_>(kv.second));
                          })
            .def_property("forwardCurves_", nullptr,
                          [](CurveCalibrationSpecBuilder_& b, const std::map<PeriodLength_, std::shared_ptr<DiscountCurve_>>& curves) {
                              b.forwardCurves_.clear();
                              for (auto& kv : curves)
                                  b.forwardCurves_[kv.first] = Handle_<DiscountCurve_>(std::const_pointer_cast<const DiscountCurve_>(kv.second));
                          })
            .def_property("baseCurve_", nullptr,
                          [](CurveCalibrationSpecBuilder_& b, const std::shared_ptr<DiscountCurve_>& curve) {
                              b.baseCurve_ = Handle_<DiscountCurve_>(std::const_pointer_cast<const DiscountCurve_>(curve));
                          })
            .def("Build", &CurveCalibrationSpecBuilder_::Build);
    }

    void init_bindings_curve_calibration_diagnostics(py::module_& m) {
        py::class_<CurveCalibrationDiagnostics_>(m, "CurveCalibrationDiagnostics_")
            .def_property_readonly("curveName_", [](const CurveCalibrationDiagnostics_& d) { return d.curveName_; })
            .def_property_readonly("curve_name", [](const CurveCalibrationDiagnostics_& d) { return d.curveName_; })
            .def_property_readonly("instrumentNames_",
                                   [](const CurveCalibrationDiagnostics_& d) {
                                       py::list result;
                                       for (const auto& value : d.instrumentNames_)
                                           result.append(std::string(value.c_str()));
                                       return result;
                                   })
            .def_property_readonly("instrument_names",
                                   [](const CurveCalibrationDiagnostics_& d) {
                                       py::list result;
                                       for (const auto& value : d.instrumentNames_)
                                           result.append(std::string(value.c_str()));
                                       return result;
                                   })
            .def_property_readonly("marketRates_",
                                   [](const CurveCalibrationDiagnostics_& d) -> py::list {
                                       py::list result;
                                       for (auto& v : d.marketRates_)
                                           result.append(v);
                                       return result;
                                   })
            .def_property_readonly("market_rates", [](const CurveCalibrationDiagnostics_& d) { return DoublesToList(d.marketRates_); })
            .def_property_readonly("modelRates_",
                                   [](const CurveCalibrationDiagnostics_& d) -> py::list {
                                       py::list result;
                                       for (auto& v : d.modelRates_)
                                           result.append(v);
                                       return result;
                                   })
            .def_property_readonly("model_rates", [](const CurveCalibrationDiagnostics_& d) { return DoublesToList(d.modelRates_); })
            .def_property_readonly("residuals_",
                                   [](const CurveCalibrationDiagnostics_& d) -> py::list {
                                       py::list result;
                                       for (auto& v : d.residuals_)
                                           result.append(v);
                                       return result;
                                   })
            .def_property_readonly("residuals", [](const CurveCalibrationDiagnostics_& d) { return DoublesToList(d.residuals_); })
            .def_property_readonly("jacobian_",
                                   py::cpp_function([](const CurveCalibrationDiagnostics_& d) -> const Matrix_<>& { return d.jacobian_; },
                                                    py::return_value_policy::reference_internal))
            .def_property_readonly("jacobian", py::cpp_function([](const CurveCalibrationDiagnostics_& d) -> const Matrix_<>& { return d.jacobian_; },
                                                                py::return_value_policy::reference_internal))
            .def_property_readonly("effJacobianInverse_",
                                   py::cpp_function([](const CurveCalibrationDiagnostics_& d) -> const Matrix_<>& { return d.effJacobianInverse_; },
                                                    py::return_value_policy::reference_internal))
            .def_property_readonly("eff_jacobian_inverse",
                                   py::cpp_function([](const CurveCalibrationDiagnostics_& d) -> const Matrix_<>& { return d.effJacobianInverse_; },
                                                    py::return_value_policy::reference_internal))
            .def_property_readonly("maxAbsResidual_", [](const CurveCalibrationDiagnostics_& d) { return d.maxAbsResidual_; })
            .def_property_readonly("max_abs_residual", [](const CurveCalibrationDiagnostics_& d) { return d.maxAbsResidual_; })
            .def_property_readonly("rmsResidual_", [](const CurveCalibrationDiagnostics_& d) { return d.rmsResidual_; })
            .def_property_readonly("rms_residual", [](const CurveCalibrationDiagnostics_& d) { return d.rmsResidual_; })
            .def_property_readonly("usedApproximateFit_", [](const CurveCalibrationDiagnostics_& d) { return d.usedApproximateFit_; })
            .def_property_readonly("used_approximate_fit", [](const CurveCalibrationDiagnostics_& d) { return d.usedApproximateFit_; });

        py::class_<CalibrationResult_>(m, "CalibrationResult_")
            .def_property_readonly(
                "curve_",
                [](const CalibrationResult_& r) -> std::shared_ptr<DiscountCurve_> { return std::const_pointer_cast<DiscountCurve_>(r.curve_); })
            .def_property_readonly("diagnostics_",
                                   py::cpp_function([](const CalibrationResult_& r) -> const CurveCalibrationDiagnostics_& { return r.diagnostics_; },
                                                    py::return_value_policy::reference_internal));
    }

    void init_bindings_curve_calibration_results(py::module_& m) {
        py::class_<MultiCurveCalibrationResult_>(m, "MultiCurveCalibrationResult_")
            .def_property_readonly("discountCurves_",
                                   [](const MultiCurveCalibrationResult_& r) -> std::map<CollateralType_, std::shared_ptr<DiscountCurve_>> {
                                       std::map<CollateralType_, std::shared_ptr<DiscountCurve_>> result;
                                       for (auto& kv : r.discountCurves_)
                                           result[kv.first] = std::const_pointer_cast<DiscountCurve_>(kv.second);
                                       return result;
                                   })
            .def_property_readonly("forwardCurves_",
                                   [](const MultiCurveCalibrationResult_& r) -> std::map<PeriodLength_, std::shared_ptr<DiscountCurve_>> {
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
            .def_property(
                "stages_",
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

        m.def(
            "CalibrateSingleCurve",
            [](const CurveCalibrationSpec_& spec) {
                py::gil_scoped_release release;
                RunCurveCalibrationGilBarrierForTesting();
                return CalibrateSingleCurve(spec);
            },
            py::arg("spec"));

        m.def(
            "CalibrateSingleCurve",
            [](const CurveCalibrationSpec_& spec, CurveJacobianMode_::Value_ jacobianMode) {
                py::gil_scoped_release release;
                return CalibrateSingleCurve(spec, CurveJacobianMode_(jacobianMode));
            },
            py::arg("spec"), py::arg("jacobian_mode"));

        m.def("CalibrateSingleCurve", py::overload_cast<const CurveCalibrationSpec_&, const CurveCalibrationOptions_&>(&CalibrateSingleCurve),
              py::arg("spec"), py::arg("options"), py::call_guard<py::gil_scoped_release>());

        m.def("CalibrateMultiCurveBundle", &CalibrateMultiCurveBundle, py::arg("spec"), py::call_guard<py::gil_scoped_release>());
    }

    void init_bindings_curve_xccy(py::module_& m) {
        py::register_exception<Underdetermined::ConvergenceError_>(m, "_CalibrationConvergenceError", PyExc_RuntimeError);
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
        DefPropertyAliases(
            xccyBuilder, "initialGuessPerNode_", "initial_guess_per_node",
            [](const CrossCurrencyCalibrationSpecBuilder_& value) { return DoublesToList(value.initialGuessPerNode_); },
            [](CrossCurrencyCalibrationSpecBuilder_& value, const py::iterable& values) { SetDoubles(&value.initialGuessPerNode_, values); });
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
        const auto jacobian = py::cpp_function([](const CrossCurrencyCalibrationDiagnostics_& value) -> const Matrix_<>& { return value.jacobian_; },
                                               py::return_value_policy::reference_internal);
        const auto effJacobianInverse =
            py::cpp_function([](const CrossCurrencyCalibrationDiagnostics_& value) -> const Matrix_<>& { return value.effJacobianInverse_; },
                             py::return_value_policy::reference_internal);
        xccyDiagnostics.def_property_readonly("instrumentNames_", instrumentNames)
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
            .def_property_readonly("residualTolerance_", [](const CrossCurrencyCalibrationDiagnostics_& value) { return value.residualTolerance_; })
            .def_property_readonly("residual_tolerance", [](const CrossCurrencyCalibrationDiagnostics_& value) { return value.residualTolerance_; })
            .def_property_readonly("jacobianScaling_",
                                   [](const CrossCurrencyCalibrationDiagnostics_& value) { return std::string(value.jacobianScaling_.c_str()); })
            .def_property_readonly("jacobian_scaling",
                                   [](const CrossCurrencyCalibrationDiagnostics_& value) { return std::string(value.jacobianScaling_.c_str()); })
            .def_property_readonly(
                "effJacobianInverseScaling_",
                [](const CrossCurrencyCalibrationDiagnostics_& value) { return std::string(value.effJacobianInverseScaling_.c_str()); })
            .def_property_readonly(
                "eff_jacobian_inverse_scaling",
                [](const CrossCurrencyCalibrationDiagnostics_& value) { return std::string(value.effJacobianInverseScaling_.c_str()); })
            .def_property_readonly("jacobianAvailability_",
                                   [](const CrossCurrencyCalibrationDiagnostics_& value) { return std::string(value.jacobianAvailability_.c_str()); })
            .def_property_readonly("jacobian_availability",
                                   [](const CrossCurrencyCalibrationDiagnostics_& value) { return std::string(value.jacobianAvailability_.c_str()); })
            .def_property_readonly(
                "effJacobianInverseAvailability_",
                [](const CrossCurrencyCalibrationDiagnostics_& value) { return std::string(value.effJacobianInverseAvailability_.c_str()); })
            .def_property_readonly(
                "eff_jacobian_inverse_availability",
                [](const CrossCurrencyCalibrationDiagnostics_& value) { return std::string(value.effJacobianInverseAvailability_.c_str()); })
            .def_property_readonly("usedApproximateFit_", [](const CrossCurrencyCalibrationDiagnostics_& value) { return value.usedApproximateFit_; })
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
        xccyResult.def_property_readonly("basis_curve", [](const CrossCurrencyCalibrationResult_& value) {
            return std::const_pointer_cast<DiscountCurve_>(XccyResultBasisCurve(value));
        });

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
        DefPropertyAliases(
            basisDeclaration, "logDfScheme_", "log_df_scheme", [](const XccyBasisCurveDeclaration_& value) { return value.logDfScheme_.Switch(); },
            [](XccyBasisCurveDeclaration_& value, LogDfScheme_::Value_ scheme) { value.logDfScheme_ = LogDfScheme_(scheme); });
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
            py::cpp_function([](const JointXccyCalibrationResult_& value) -> const Matrix_<>& { return JointXccyResultEffJacobianInverse(value); },
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
              py::overload_cast<const CrossCurrencyCalibrationSpec_&, const CrossCurrencyCalibrationOptions_&>(&CalibrateXccyMarket), py::arg("spec"),
              py::arg("options"), py::call_guard<py::gil_scoped_release>());
        m.def("CalibrateJointXccyMarket", py::overload_cast<const JointXccyCalibrationSpec_&>(&CalibrateJointXccyMarket), py::arg("spec"),
              py::call_guard<py::gil_scoped_release>());
        m.def("CalibrateJointXccyMarket",
              py::overload_cast<const JointXccyCalibrationSpec_&, const JointXccyCalibrationOptions_&>(&CalibrateJointXccyMarket), py::arg("spec"),
              py::arg("options"), py::call_guard<py::gil_scoped_release>());
        m.def("ValidateCrossCurrencyAnalyticEligibility", &ValidateCrossCurrencyAnalyticEligibility, py::arg("spec"),
              py::call_guard<py::gil_scoped_release>());
        m.def("ValidateJointXccyAnalyticEligibility", &ValidateJointXccyAnalyticEligibility, py::arg("spec"),
              py::call_guard<py::gil_scoped_release>());
        m.def("_RequiredHistoricalXccyFixings", &RequiredHistoricalXccyFixingsForPython, py::arg("instruments"), py::arg("valuation_time"));

        AddMatrixSnakeCaseAliases(m);
    }
} // anonymous namespace

void init_bindings_curve(py::module_& m) {
    init_bindings_curve_handles(m);
    init_bindings_curve_protocol(m);
    init_bindings_curve_enums(m);
    init_bindings_curve_planning(m);
    init_bindings_curve_instruments(m);
    init_bindings_curve_data(m);
    init_bindings_curve_calibration_builder(m);
    init_bindings_curve_calibration_diagnostics(m);
    init_bindings_curve_calibration_results(m);
    init_bindings_curve_xccy(m);
}

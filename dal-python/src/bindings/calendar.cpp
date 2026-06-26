//
// calendar.cpp — calendar and holiday type bindings (Holidays_, BizDayConvention_, CountBusDays_)
//

#include "bindings.h"

#include <dal/platform/platform.hpp>
#include <dal/time/date.hpp>
#include <dal/time/holidays.hpp>
#include <dal/utilities/exceptions.hpp>

namespace Dal {
#include <dal/auto/MG_BizDayConvention_enum.hpp>
} // namespace Dal

using namespace Dal;

void init_bindings_calendar(py::module_& m) {
    py::enum_<BizDayConvention_::Value_>(m, "BizDayConvention_")
        .value("UNADJUSTED", BizDayConvention_::Value_::Unadjusted)
        .value("FOLLOWING", BizDayConvention_::Value_::Following)
        .value("MODIFIED_FOLLOWING", BizDayConvention_::Value_::ModifiedFollowing)
        .export_values();

    py::class_<Holidays_>(m, "Holidays_")
        .def(py::init<const String_&>(), py::arg("name"))
        .def(py::init([](const char* name) { return Holidays_(String_(name)); }),
             py::arg("name"))
        .def("IsHoliday", &Holidays_::IsHoliday, py::arg("date"))
        .def("IsWorkWeekends", &Holidays_::IsWorkWeekends, py::arg("date"))
        .def("String", &Holidays_::String)
        .def("__repr__", [](const Holidays_& h) -> std::string {
            return std::string("Holidays_(\"") + h.String().c_str() + "\")";
        });

    py::class_<CountBusDays_>(m, "CountBusDays_")
        .def(py::init<const Holidays_&>(), py::arg("holidays"))
        .def("__call__", &CountBusDays_::operator(), py::arg("begin"), py::arg("end"));

    m.def("Is_BizDay", &Holidays::IsBusinessDay,
          py::arg("holidays"), py::arg("date"));
    m.def("NextBizDay", &Holidays::NextBus,
          py::arg("holidays"), py::arg("date"));
    m.def("PrevBizDay", &Holidays::PrevBus,
          py::arg("holidays"), py::arg("date"));
    m.def("Adjust", [](const Holidays_& hols, const Date_& date,
                          BizDayConvention_::Value_ convention) -> Date_ {
            return Holidays::Adjust(hols, date, BizDayConvention_(convention));
        },
        py::arg("holidays"), py::arg("date"),
        py::arg("convention"));
}

#pragma once

#include <dal/calibration/validation_internal.hpp>
#include <dal/curve/compoundedois.hpp>

namespace Dal::CompoundedOisInternal {
    CalibrationInternal::Json_ CalendarJson(const HolidayCenterData_& calendar);
    CalibrationInternal::Json_ ContractJson(const CompoundedOisScriptSpec_& spec);
    CalibrationInternal::Json_ ValuationJson(const Script::ScriptValuationSettings_& valuation);
    void Validate(const CompoundedOisScriptSpec_& spec, const Script::ScriptValuationSettings_& valuation);
} // namespace Dal::CompoundedOisInternal

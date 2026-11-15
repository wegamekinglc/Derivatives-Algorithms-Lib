#pragma once

#include <dal/calibration/validation_internal.hpp>
#include <dal/curve/compoundedois.hpp>

namespace Dal::CompoundedOisInternal {
    CalibrationInternal::Json_ CalendarJson(const HolidayCenterData_& calendar);
    CalibrationInternal::Json_ ContractJson(const CompoundedOisScriptSpec_& spec);
    CalibrationInternal::Json_ ValuationJson(const Script::ScriptValuationSettings_& valuation);
    CalibrationInternal::Json_ ScheduleJson(const Vector_<CompoundedOisResolvedObservation_>& schedule, bool includeValuation = false);
    CalibrationInternal::Json_ ConventionJson(const CompoundedOisObservationConvention_& convention);
    bool IsVersionTwo(const CompoundedOisScriptSpec_& spec);
    bool IsBusiness(const HolidayCenterData_& calendar, const Date_& day);
    void ValidateObservationConvention(const CompoundedOisScriptSpec_& spec);
    Vector_<CompoundedOisResolvedObservation_> ResolveSchedule(const CompoundedOisScriptSpec_& spec,
                                                               const Script::ScriptValuationSettings_& valuation);
    void Validate(const CompoundedOisScriptSpec_& spec, const Script::ScriptValuationSettings_& valuation);
} // namespace Dal::CompoundedOisInternal

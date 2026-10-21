#pragma once

#include <dal/calibration/validation_internal.hpp>
#include <dal/script/instrument.hpp>

namespace Dal::CalibrationInternal {
    Json_ SourceJson(const ScriptInstrumentSource_& source);
    Json_ DefinitionJson(const ScriptInstrumentDefinitionSpec_& spec);
    Json_ BindingValuesJson(const Vector_<CalibrationEffectiveParameter_>& binding, bool provenance);
    Json_ InstrumentJson(const CalibrationInstrument_& instrument);
    ScriptInstrumentDefinitionSpec_ ReadDefinition(const Json_& value, const String_& path);
    Vector_<CalibrationEffectiveParameter_> ReadBinding(const Json_& value, const ScriptInstrumentDefinition_& definition);
    void ValidateParameter(double value,
                           const ScriptParameterDeclaration_& declaration,
                           const String_& path,
                           const CalibrationDiagnostic_& context,
                           const String_& code = "InvalidCalibrationParameterBinding");
} // namespace Dal::CalibrationInternal

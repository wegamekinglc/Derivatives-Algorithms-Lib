#pragma once

#include <dal/script/calibrationpreparation.hpp>

namespace Dal::Script {
    struct NodePays_;
} // namespace Dal::Script

namespace Dal::CalibrationRateOptionInternal {
    struct Payment_ {
        const Script::NodePays_* node_;
        Date_ date_;
        CalibrationDiagnostic_ source_;
    };
    std::optional<CalibrationRateOptionDescriptor_>
    Match(const Vector_<Payment_>& payments, const CalibrationInstrument_& instrument, const CalibrationPreparationContext_& context);
    String_ Rule(const CalibrationRateOptionDescriptor_& descriptor);
} // namespace Dal::CalibrationRateOptionInternal

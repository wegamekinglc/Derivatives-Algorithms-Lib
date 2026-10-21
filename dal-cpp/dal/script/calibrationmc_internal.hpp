#pragma once

#include <dal/script/calibrationmc.hpp>

namespace Dal::CalibrationMcInternal {
    CalibrationMcAffineEstimate_ AffineEstimate(const Vector_<CalibrationAffineCoefficients_>& batches,
                                                int annuitySign,
                                                double minAbsAnnuity,
                                                double sigma,
                                                const CalibrationDiagnostic_& diagnostic = {});
} // namespace Dal::CalibrationMcInternal

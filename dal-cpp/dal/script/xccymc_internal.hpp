#pragma once

#include <dal/script/xccymc.hpp>

namespace Dal::XccyMcInternal {
    void Summarize(XccyMcResult_* result, const PreparedCalibrationInstrument_& admitted, double coupon);
    [[nodiscard]] Handle_<CalibrationReport_> Report(const XccyMcResult_& result,
                                                     const PreparedCalibrationInstrument_& admitted,
                                                     const XccyGaussianModelData_& model,
                                                     const XccyMcSettings_& settings);
} // namespace Dal::XccyMcInternal

#pragma once

#include <dal/model/detail/dupirequoteplan.hpp>

namespace TapePerfDupire {
    using namespace Dal::DupireSecondOrderDetail;
    inline constexpr double QUOTE_STEP = 4e-4;

    [[nodiscard]] Dal::Vector_<> QuoteSpreads();
    [[nodiscard]] Dal::Matrix_<> Directions();
    [[nodiscard]] Dal::DupireCalibrationSnapshot_ Snapshot(const Dal::Vector_<>& quotes);
    void Qualify(const Kernel_& kernel, const Dal::Vector_<>& quotes, const Dal::Matrix_<>& directions);
} // namespace TapePerfDupire

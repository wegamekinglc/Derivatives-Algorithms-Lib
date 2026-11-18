#pragma once

#include <dal-public/src/calibrationrisk.hpp>
#include <dal/curve/quoteriskprovenance_internal.hpp>

namespace Dal::CalibrationRiskInternal {
    inline bool LiveSource(const CalibrationPullback_& calibration) {
        const auto* curve = std::get_if<RateQuoteRiskProvenance_>(&calibration.Source());
        return !curve || SupportsLiveQuoteRiskState(*curve);
    }

    inline void RequireLiveSource(const CalibrationPullback_& calibration, const char* field) {
        if (const auto* curve = std::get_if<RateQuoteRiskProvenance_>(&calibration.Source()))
            RequireLiveQuoteRiskState(*curve, field);
    }

    inline String_ SourceIdentity(const CalibrationPullback_& calibration) {
        if (const auto* curve = std::get_if<RateQuoteRiskProvenance_>(&calibration.Source()))
            return String_(curve->State().fingerprint_.substr(0, 23));
        return calibration.Domain();
    }
} // namespace Dal::CalibrationRiskInternal

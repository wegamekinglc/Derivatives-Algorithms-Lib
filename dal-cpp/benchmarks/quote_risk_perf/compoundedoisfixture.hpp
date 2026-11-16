#pragma once

#include <dal/curve/scriptcurvequoterisk.hpp>

struct CompoundedOisRiskCase_ {
    Dal::CurveInstrumentCalibrationSpec_ spec_;
    int pvs_ = 1, factors_ = 0;
    Dal::String_ mode_;
};
Dal::Vector_<CompoundedOisRiskCase_> CompoundedOisRiskCases();

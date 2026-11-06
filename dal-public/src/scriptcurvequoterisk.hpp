#pragma once

#include <dal/curve/scriptcurvequoterisk.hpp>

namespace Dal {
    Matrix_<Cell_> ScriptCurveQuoteRiskTable(const ScriptCurveQuoteRiskSnapshot_& snapshot, const String_& table);
    Matrix_<Cell_> ScriptCurveQuoteRiskTable(const ScriptCurveQuoteRiskResult_& result, const String_& table);
} // namespace Dal

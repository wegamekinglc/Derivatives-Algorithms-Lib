#pragma once

#include <dal/curve/depositscript.hpp>

namespace Dal {
    using CurveScriptImportSettings_ = DepositScriptImportSettings_;

    String_ CurveScriptProjectionRoute(const Ccy_& currency, const PeriodLength_& tenor);
    Handle_<CalibrationInstrument_> NewFraScriptInstrument(const FRA_& native, const Date_& payment, const CurveScriptImportSettings_& settings);
    Handle_<CalibrationInstrument_> NewSwapScriptInstrument(const Swap_& native, const CurveScriptImportSettings_& settings);
    Handle_<CalibrationInstrument_> NewPeriodForwardOisScriptInstrument(const OISSwap_& native, const CurveScriptImportSettings_& settings);
    Handle_<CalibrationInstrument_> NewBasisSwapScriptInstrument(const BasisSwap_& native, const CurveScriptImportSettings_& settings);
} // namespace Dal

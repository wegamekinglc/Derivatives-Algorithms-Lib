#pragma once

#include <dal/curve/xccypricing.hpp>
#include <dal/script/calibrationpreparation.hpp>

namespace Dal {
    struct XccyScriptImportSettings_ {
        String_ instrumentId_, discountRoute_;
        bool receiveNonSpreadPaySpread_ = true;
        double minAbsAnnuity_ = 1e-12;
    };
    // NativeXccyTimestamp is separate from the generic date-only today policy.
    struct XccyScriptContext_ {
        DateTime_ valuationTime_;
        CurrencyPair_ pair_;
        Ccy_ collateralCurrency_;
        String_ discountRoute_;
        Handle_<MarketFixingSnapshot_> fixings_;
        std::optional<TodayFixingPolicy_> todayFixingOverride_;
    };
    struct CalibrationXccyDescriptor_ {
        XccyCashflowPlan_ plan_;
        Handle_<MarketFixingSnapshot_> fixings_;
        double coupon_ = 0.;
        bool receiveNonSpreadPaySpread_ = true;
        DateTime_ valuationTime_;
    };
    Handle_<CalibrationInstrument_> NewXccyScriptInstrument(const CrossCurrencySwap_& native, const XccyScriptImportSettings_& settings);
    template <class T_> T_ EvaluateXccyScript(const PreparedCalibrationInstrument_& prepared, const XccyMarketView_<T_>& market);
} // namespace Dal

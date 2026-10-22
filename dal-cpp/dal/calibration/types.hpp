#pragma once

#include <optional>

#include <dal/currency/currency.hpp>
#include <dal/protocol/collateraltype.hpp>
#include <dal/time/date.hpp>

/*IF--------------------------------------------------------------------------
enumeration CalibrationUnitKind
    CalibrationUnitKind protocol tokens
switchable
alternative Rate
alternative BasisSpread
alternative Price
alternative FxOutright
alternative BondPrice
alternative YearFraction
alternative Dimensionless
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
enumeration CalibrationQuoteConventionId
    CalibrationQuoteConventionId protocol tokens
switchable
alternative DecimalRate
alternative DecimalSpread
alternative PresentValueAmount
alternative QuotePerBase
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
enumeration CalibrationMeasureKind
    CalibrationMeasureKind protocol tokens
switchable
alternative Price
alternative ParAffineRate
alternative ParAffineSpread
alternative FxOutright
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
enumeration CalibrationBindingOrigin
    CalibrationBindingOrigin protocol tokens
switchable
alternative SourceDefault
alternative Literal
alternative QuoteCopy
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
enumeration CalibrationStage
    CalibrationStage protocol tokens
switchable
alternative Input
alternative Prepare
alternative Evaluate
alternative Solve
alternative Validate
alternative Load
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
enumeration ScriptSourceRowKind
    ScriptSourceRowKind protocol tokens
switchable
alternative Definition
alternative Event
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
enumeration ScriptParameterType
    ScriptParameterType protocol tokens
switchable
alternative Real
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
enumeration ScriptParameterRole
    ScriptParameterRole protocol tokens
switchable
alternative Notional
alternative Coupon
alternative Strike
alternative Accrual
alternative RateStrike
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
enumeration ScriptSettlement
    ScriptSettlement protocol tokens
switchable
alternative AtEvent
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
enumeration ScriptValuationCashflows
    ScriptValuationCashflows protocol tokens
switchable
alternative IncludeUnsettledAtValuation
-IF-------------------------------------------------------------------------*/

namespace Dal {
#include <dal/auto/MG_CalibrationBindingOrigin_enum.hpp>
#include <dal/auto/MG_CalibrationMeasureKind_enum.hpp>
#include <dal/auto/MG_CalibrationQuoteConventionId_enum.hpp>
#include <dal/auto/MG_CalibrationStage_enum.hpp>
#include <dal/auto/MG_CalibrationUnitKind_enum.hpp>
#include <dal/auto/MG_ScriptParameterRole_enum.hpp>
#include <dal/auto/MG_ScriptParameterType_enum.hpp>
#include <dal/auto/MG_ScriptSettlement_enum.hpp>
#include <dal/auto/MG_ScriptSourceRowKind_enum.hpp>
#include <dal/auto/MG_ScriptValuationCashflows_enum.hpp>
    struct CalibrationValueUnit_ {
        CalibrationUnitKind_ kind_;
        std::optional<Ccy_> currency_, baseCurrency_, quoteCurrency_;
        [[nodiscard]] static CalibrationValueUnit_ Parse(const String_& token);
        [[nodiscard]] String_ Token() const;
    };
    bool operator==(const CalibrationValueUnit_& lhs, const CalibrationValueUnit_& rhs);
    inline bool operator!=(const CalibrationValueUnit_& lhs, const CalibrationValueUnit_& rhs) { return !(lhs == rhs); }

    struct CalibrationQuoteConvention_ {
        CalibrationQuoteConventionId_ id_;
        int version_ = 1;
    };
    struct CalibrationMeasureSpec_ {
        String_ id_;
        CalibrationMeasureKind_ kind_;
        String_ receiver_;
        std::optional<String_> parameter_;
        CalibrationValueUnit_ unit_;
        std::optional<int> annuitySign_;
        std::optional<double> minAbsAnnuity_;
    };
    struct CalibrationDiagnostic_ {
        String_ code_, message_;
        CalibrationStage_ stage_ = CalibrationStage_::Value_::Input;
        std::optional<String_> field_, observationId_, instrumentId_, quoteId_, measureId_, parameterKey_;
        std::optional<int> sourceRow_;
        std::optional<Date_> eventDate_;
        std::optional<int> statement_;
        std::optional<String_> actual_, expected_;
    };
} // namespace Dal

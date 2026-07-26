#pragma once

class  AnalyticIneligibilityReason_
{
public:
    enum class Value_ : char
    {
     _NOT_SET=-1,
     DISCOUNT_TARGET_REQUIRED,
     TEMPLATED_RATE_UNAVAILABLE,
     PROJECTION_NOT_ALLOWED,
     PROJECTION_REQUIRED,
     TRADE_DATE_MISMATCH,
     LIBOR_BASIS_UNSUPPORTED,
     DISCOUNT_ROUTE_MISSING,
     PROJECTION_ROUTE_MISSING,
     PAIR_CURRENCY_MISMATCH,
     COUPON_PLAN_EMPTY,
     NOTIONAL_MODE_UNSUPPORTED,
     RESET_MAPPING_INVALID,
     CASHFLOW_PLAN_UNSUPPORTED,
     _N_VALUES
    } val_;
      
    AnalyticIneligibilityReason_(Value_ val) : val_(val) {
        REQUIRE(val < Value_::_N_VALUES, "val is not valid");
    }
private:
    friend bool operator==(const AnalyticIneligibilityReason_& lhs, const AnalyticIneligibilityReason_& rhs);
    friend struct ReadStringAnalyticIneligibilityReason_;
    friend Vector_<AnalyticIneligibilityReason_> AnalyticIneligibilityReasonListAll();
    friend bool operator<(const AnalyticIneligibilityReason_& lhs, const AnalyticIneligibilityReason_& rhs) {
        return lhs.val_ < rhs.val_;
    }
public:
    explicit AnalyticIneligibilityReason_(const String_& src);
    const char* String() const;
    Value_ Switch() const {return val_;}
    AnalyticIneligibilityReason_() : val_(Value_::_NOT_SET) {};
};

Vector_<AnalyticIneligibilityReason_> AnalyticIneligibilityReasonListAll();

bool operator==(const AnalyticIneligibilityReason_& lhs, const AnalyticIneligibilityReason_& rhs);
inline bool operator!=(const AnalyticIneligibilityReason_& lhs, const AnalyticIneligibilityReason_& rhs) {return !(lhs == rhs);}
inline bool operator==(const AnalyticIneligibilityReason_& lhs, AnalyticIneligibilityReason_::Value_ rhs) {return lhs.Switch() == rhs;}
inline bool operator!=(const AnalyticIneligibilityReason_& lhs, AnalyticIneligibilityReason_::Value_ rhs) {return lhs.Switch() != rhs;}

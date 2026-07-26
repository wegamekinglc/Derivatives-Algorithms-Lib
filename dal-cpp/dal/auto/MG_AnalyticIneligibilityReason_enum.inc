
bool operator==(const AnalyticIneligibilityReason_& lhs, const AnalyticIneligibilityReason_& rhs) {return lhs.val_ == rhs.val_;}
namespace {
    Vector_<AnalyticIneligibilityReason_>& TheAnalyticIneligibilityReasonList() {
        RETURN_STATIC(Vector_<AnalyticIneligibilityReason_>);
    }
}    // leave local

Vector_<AnalyticIneligibilityReason_> AnalyticIneligibilityReasonListAll() {
   if (TheAnalyticIneligibilityReasonList().empty()) {
        TheAnalyticIneligibilityReasonList().emplace_back(AnalyticIneligibilityReason_::Value_::DISCOUNT_TARGET_REQUIRED);
        TheAnalyticIneligibilityReasonList().emplace_back(AnalyticIneligibilityReason_::Value_::TEMPLATED_RATE_UNAVAILABLE);
        TheAnalyticIneligibilityReasonList().emplace_back(AnalyticIneligibilityReason_::Value_::PROJECTION_NOT_ALLOWED);
        TheAnalyticIneligibilityReasonList().emplace_back(AnalyticIneligibilityReason_::Value_::PROJECTION_REQUIRED);
        TheAnalyticIneligibilityReasonList().emplace_back(AnalyticIneligibilityReason_::Value_::TRADE_DATE_MISMATCH);
        TheAnalyticIneligibilityReasonList().emplace_back(AnalyticIneligibilityReason_::Value_::LIBOR_BASIS_UNSUPPORTED);
        TheAnalyticIneligibilityReasonList().emplace_back(AnalyticIneligibilityReason_::Value_::DISCOUNT_ROUTE_MISSING);
        TheAnalyticIneligibilityReasonList().emplace_back(AnalyticIneligibilityReason_::Value_::PROJECTION_ROUTE_MISSING);
        TheAnalyticIneligibilityReasonList().emplace_back(AnalyticIneligibilityReason_::Value_::PAIR_CURRENCY_MISMATCH);
        TheAnalyticIneligibilityReasonList().emplace_back(AnalyticIneligibilityReason_::Value_::COUPON_PLAN_EMPTY);
        TheAnalyticIneligibilityReasonList().emplace_back(AnalyticIneligibilityReason_::Value_::NOTIONAL_MODE_UNSUPPORTED);
        TheAnalyticIneligibilityReasonList().emplace_back(AnalyticIneligibilityReason_::Value_::RESET_MAPPING_INVALID);
        TheAnalyticIneligibilityReasonList().emplace_back(AnalyticIneligibilityReason_::Value_::CASHFLOW_PLAN_UNSUPPORTED);
   }
   return TheAnalyticIneligibilityReasonList();
}


const char* AnalyticIneligibilityReason_::String() const {
    switch (val_)
    {
    default:
    case Value_::_NOT_SET:
        return 0;
        case Value_::DISCOUNT_TARGET_REQUIRED:
        return "DISCOUNT_TARGET_REQUIRED";
    case Value_::TEMPLATED_RATE_UNAVAILABLE:
        return "TEMPLATED_RATE_UNAVAILABLE";
    case Value_::PROJECTION_NOT_ALLOWED:
        return "PROJECTION_NOT_ALLOWED";
    case Value_::PROJECTION_REQUIRED:
        return "PROJECTION_REQUIRED";
    case Value_::TRADE_DATE_MISMATCH:
        return "TRADE_DATE_MISMATCH";
    case Value_::LIBOR_BASIS_UNSUPPORTED:
        return "LIBOR_BASIS_UNSUPPORTED";
    case Value_::DISCOUNT_ROUTE_MISSING:
        return "DISCOUNT_ROUTE_MISSING";
    case Value_::PROJECTION_ROUTE_MISSING:
        return "PROJECTION_ROUTE_MISSING";
    case Value_::PAIR_CURRENCY_MISMATCH:
        return "PAIR_CURRENCY_MISMATCH";
    case Value_::COUPON_PLAN_EMPTY:
        return "COUPON_PLAN_EMPTY";
    case Value_::NOTIONAL_MODE_UNSUPPORTED:
        return "NOTIONAL_MODE_UNSUPPORTED";
    case Value_::RESET_MAPPING_INVALID:
        return "RESET_MAPPING_INVALID";
    case Value_::CASHFLOW_PLAN_UNSUPPORTED:
        return "CASHFLOW_PLAN_UNSUPPORTED";
        
    }}

struct ReadStringAnalyticIneligibilityReason_
{
    ReadStringAnalyticIneligibilityReason_() {}

    bool operator()(const String_& src, AnalyticIneligibilityReason_::Value_* val) const    // returns true iff recognized input
    {
        bool ret_val = true;
        if (0);	// otiose code to allow regular else-if structure
        else if (src.empty())
        { ret_val = false; }
        
	else if (String::Equivalent(src, "DISCOUNTTARGETREQUIRED"))
        *val = AnalyticIneligibilityReason_::Value_::DISCOUNT_TARGET_REQUIRED;

	else if (String::Equivalent(src, "TEMPLATEDRATEUNAVAILABLE"))
        *val = AnalyticIneligibilityReason_::Value_::TEMPLATED_RATE_UNAVAILABLE;

	else if (String::Equivalent(src, "PROJECTIONNOTALLOWED"))
        *val = AnalyticIneligibilityReason_::Value_::PROJECTION_NOT_ALLOWED;

	else if (String::Equivalent(src, "PROJECTIONREQUIRED"))
        *val = AnalyticIneligibilityReason_::Value_::PROJECTION_REQUIRED;

	else if (String::Equivalent(src, "TRADEDATEMISMATCH"))
        *val = AnalyticIneligibilityReason_::Value_::TRADE_DATE_MISMATCH;

	else if (String::Equivalent(src, "LIBORBASISUNSUPPORTED"))
        *val = AnalyticIneligibilityReason_::Value_::LIBOR_BASIS_UNSUPPORTED;

	else if (String::Equivalent(src, "DISCOUNTROUTEMISSING"))
        *val = AnalyticIneligibilityReason_::Value_::DISCOUNT_ROUTE_MISSING;

	else if (String::Equivalent(src, "PROJECTIONROUTEMISSING"))
        *val = AnalyticIneligibilityReason_::Value_::PROJECTION_ROUTE_MISSING;

	else if (String::Equivalent(src, "PAIRCURRENCYMISMATCH"))
        *val = AnalyticIneligibilityReason_::Value_::PAIR_CURRENCY_MISMATCH;

	else if (String::Equivalent(src, "COUPONPLANEMPTY"))
        *val = AnalyticIneligibilityReason_::Value_::COUPON_PLAN_EMPTY;

	else if (String::Equivalent(src, "NOTIONALMODEUNSUPPORTED"))
        *val = AnalyticIneligibilityReason_::Value_::NOTIONAL_MODE_UNSUPPORTED;

	else if (String::Equivalent(src, "RESETMAPPINGINVALID"))
        *val = AnalyticIneligibilityReason_::Value_::RESET_MAPPING_INVALID;

	else if (String::Equivalent(src, "CASHFLOWPLANUNSUPPORTED"))
        *val = AnalyticIneligibilityReason_::Value_::CASHFLOW_PLAN_UNSUPPORTED;
        else
            ret_val = false;
        return ret_val;
    }
};

AnalyticIneligibilityReason_::AnalyticIneligibilityReason_(const String_& src) {
    static const ReadStringAnalyticIneligibilityReason_ READ_FIXED;    // allows precomputation for speed, in constructor
    if (READ_FIXED(src, &val_))
        return;
    THROW("'" + src + "' is not a recognizable AnalyticIneligibilityReason");
}





bool operator==(const ScriptParameterRole_& lhs, const ScriptParameterRole_& rhs) {return lhs.val_ == rhs.val_;}
namespace {
    Vector_<ScriptParameterRole_>& TheScriptParameterRoleList() {
        RETURN_STATIC(Vector_<ScriptParameterRole_>);
    }
}    // leave local

Vector_<ScriptParameterRole_> ScriptParameterRoleListAll() {
   if (TheScriptParameterRoleList().empty()) {
        TheScriptParameterRoleList().emplace_back(ScriptParameterRole_::Value_::Notional);
        TheScriptParameterRoleList().emplace_back(ScriptParameterRole_::Value_::Coupon);
        TheScriptParameterRoleList().emplace_back(ScriptParameterRole_::Value_::Strike);
        TheScriptParameterRoleList().emplace_back(ScriptParameterRole_::Value_::Accrual);
        TheScriptParameterRoleList().emplace_back(ScriptParameterRole_::Value_::RateStrike);
   }
   return TheScriptParameterRoleList();
}


const char* ScriptParameterRole_::String() const {
    switch (val_)
    {
    default:
    case Value_::_NOT_SET:
        return 0;
        case Value_::Notional:
        return "Notional";
    case Value_::Coupon:
        return "Coupon";
    case Value_::Strike:
        return "Strike";
    case Value_::Accrual:
        return "Accrual";
    case Value_::RateStrike:
        return "RateStrike";
        
    }}

struct ReadStringScriptParameterRole_
{
    ReadStringScriptParameterRole_() {}

    bool operator()(const String_& src, ScriptParameterRole_::Value_* val) const    // returns true iff recognized input
    {
        bool ret_val = true;
        if (0);	// otiose code to allow regular else-if structure
        else if (src.empty())
        { ret_val = false; }
        
	else if (String::Equivalent(src, "NOTIONAL"))
        *val = ScriptParameterRole_::Value_::Notional;

	else if (String::Equivalent(src, "COUPON"))
        *val = ScriptParameterRole_::Value_::Coupon;

	else if (String::Equivalent(src, "STRIKE"))
        *val = ScriptParameterRole_::Value_::Strike;

	else if (String::Equivalent(src, "ACCRUAL"))
        *val = ScriptParameterRole_::Value_::Accrual;

	else if (String::Equivalent(src, "RATESTRIKE"))
        *val = ScriptParameterRole_::Value_::RateStrike;
        else
            ret_val = false;
        return ret_val;
    }
};

ScriptParameterRole_::ScriptParameterRole_(const String_& src) {
    static const ReadStringScriptParameterRole_ READ_FIXED;    // allows precomputation for speed, in constructor
    if (READ_FIXED(src, &val_))
        return;
    THROW("'" + src + "' is not a recognizable ScriptParameterRole");
}




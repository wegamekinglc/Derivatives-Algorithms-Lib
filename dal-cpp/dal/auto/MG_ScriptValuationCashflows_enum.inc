
bool operator==(const ScriptValuationCashflows_& lhs, const ScriptValuationCashflows_& rhs) {return lhs.val_ == rhs.val_;}
namespace {
    Vector_<ScriptValuationCashflows_>& TheScriptValuationCashflowsList() {
        RETURN_STATIC(Vector_<ScriptValuationCashflows_>);
    }
}    // leave local

Vector_<ScriptValuationCashflows_> ScriptValuationCashflowsListAll() {
   if (TheScriptValuationCashflowsList().empty()) {
        TheScriptValuationCashflowsList().emplace_back(ScriptValuationCashflows_::Value_::IncludeUnsettledAtValuation);
   }
   return TheScriptValuationCashflowsList();
}


const char* ScriptValuationCashflows_::String() const {
    switch (val_)
    {
    default:
    case Value_::_NOT_SET:
        return 0;
        case Value_::IncludeUnsettledAtValuation:
        return "IncludeUnsettledAtValuation";
        
    }}

struct ReadStringScriptValuationCashflows_
{
    ReadStringScriptValuationCashflows_() {}

    bool operator()(const String_& src, ScriptValuationCashflows_::Value_* val) const    // returns true iff recognized input
    {
        bool ret_val = true;
        if (0);	// otiose code to allow regular else-if structure
        else if (src.empty())
        { ret_val = false; }
        
	else if (String::Equivalent(src, "INCLUDEUNSETTLEDATVALUATION"))
        *val = ScriptValuationCashflows_::Value_::IncludeUnsettledAtValuation;
        else
            ret_val = false;
        return ret_val;
    }
};

ScriptValuationCashflows_::ScriptValuationCashflows_(const String_& src) {
    static const ReadStringScriptValuationCashflows_ READ_FIXED;    // allows precomputation for speed, in constructor
    if (READ_FIXED(src, &val_))
        return;
    THROW("'" + src + "' is not a recognizable ScriptValuationCashflows");
}




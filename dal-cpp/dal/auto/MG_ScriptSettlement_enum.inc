
bool operator==(const ScriptSettlement_& lhs, const ScriptSettlement_& rhs) {return lhs.val_ == rhs.val_;}
namespace {
    Vector_<ScriptSettlement_>& TheScriptSettlementList() {
        RETURN_STATIC(Vector_<ScriptSettlement_>);
    }
}    // leave local

Vector_<ScriptSettlement_> ScriptSettlementListAll() {
   if (TheScriptSettlementList().empty()) {
        TheScriptSettlementList().emplace_back(ScriptSettlement_::Value_::AtEvent);
   }
   return TheScriptSettlementList();
}


const char* ScriptSettlement_::String() const {
    switch (val_)
    {
    default:
    case Value_::_NOT_SET:
        return 0;
        case Value_::AtEvent:
        return "AtEvent";
        
    }}

struct ReadStringScriptSettlement_
{
    ReadStringScriptSettlement_() {}

    bool operator()(const String_& src, ScriptSettlement_::Value_* val) const    // returns true iff recognized input
    {
        bool ret_val = true;
        if (0);	// otiose code to allow regular else-if structure
        else if (src.empty())
        { ret_val = false; }
        
	else if (String::Equivalent(src, "ATEVENT"))
        *val = ScriptSettlement_::Value_::AtEvent;
        else
            ret_val = false;
        return ret_val;
    }
};

ScriptSettlement_::ScriptSettlement_(const String_& src) {
    static const ReadStringScriptSettlement_ READ_FIXED;    // allows precomputation for speed, in constructor
    if (READ_FIXED(src, &val_))
        return;
    THROW("'" + src + "' is not a recognizable ScriptSettlement");
}




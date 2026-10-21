
bool operator==(const ScriptParameterType_& lhs, const ScriptParameterType_& rhs) {return lhs.val_ == rhs.val_;}
namespace {
    Vector_<ScriptParameterType_>& TheScriptParameterTypeList() {
        RETURN_STATIC(Vector_<ScriptParameterType_>);
    }
}    // leave local

Vector_<ScriptParameterType_> ScriptParameterTypeListAll() {
   if (TheScriptParameterTypeList().empty()) {
        TheScriptParameterTypeList().emplace_back(ScriptParameterType_::Value_::Real);
   }
   return TheScriptParameterTypeList();
}


const char* ScriptParameterType_::String() const {
    switch (val_)
    {
    default:
    case Value_::_NOT_SET:
        return 0;
        case Value_::Real:
        return "Real";
        
    }}

struct ReadStringScriptParameterType_
{
    ReadStringScriptParameterType_() {}

    bool operator()(const String_& src, ScriptParameterType_::Value_* val) const    // returns true iff recognized input
    {
        bool ret_val = true;
        if (0);	// otiose code to allow regular else-if structure
        else if (src.empty())
        { ret_val = false; }
        
	else if (String::Equivalent(src, "REAL"))
        *val = ScriptParameterType_::Value_::Real;
        else
            ret_val = false;
        return ret_val;
    }
};

ScriptParameterType_::ScriptParameterType_(const String_& src) {
    static const ReadStringScriptParameterType_ READ_FIXED;    // allows precomputation for speed, in constructor
    if (READ_FIXED(src, &val_))
        return;
    THROW("'" + src + "' is not a recognizable ScriptParameterType");
}




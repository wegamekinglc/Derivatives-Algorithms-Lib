
bool operator==(const ScriptSourceRowKind_& lhs, const ScriptSourceRowKind_& rhs) {return lhs.val_ == rhs.val_;}
namespace {
    Vector_<ScriptSourceRowKind_>& TheScriptSourceRowKindList() {
        RETURN_STATIC(Vector_<ScriptSourceRowKind_>);
    }
}    // leave local

Vector_<ScriptSourceRowKind_> ScriptSourceRowKindListAll() {
   if (TheScriptSourceRowKindList().empty()) {
        TheScriptSourceRowKindList().emplace_back(ScriptSourceRowKind_::Value_::Definition);
        TheScriptSourceRowKindList().emplace_back(ScriptSourceRowKind_::Value_::Event);
   }
   return TheScriptSourceRowKindList();
}


const char* ScriptSourceRowKind_::String() const {
    switch (val_)
    {
    default:
    case Value_::_NOT_SET:
        return 0;
        case Value_::Definition:
        return "Definition";
    case Value_::Event:
        return "Event";
        
    }}

struct ReadStringScriptSourceRowKind_
{
    ReadStringScriptSourceRowKind_() {}

    bool operator()(const String_& src, ScriptSourceRowKind_::Value_* val) const    // returns true iff recognized input
    {
        bool ret_val = true;
        if (0);	// otiose code to allow regular else-if structure
        else if (src.empty())
        { ret_val = false; }
        
	else if (String::Equivalent(src, "DEFINITION"))
        *val = ScriptSourceRowKind_::Value_::Definition;

	else if (String::Equivalent(src, "EVENT"))
        *val = ScriptSourceRowKind_::Value_::Event;
        else
            ret_val = false;
        return ret_val;
    }
};

ScriptSourceRowKind_::ScriptSourceRowKind_(const String_& src) {
    static const ReadStringScriptSourceRowKind_ READ_FIXED;    // allows precomputation for speed, in constructor
    if (READ_FIXED(src, &val_))
        return;
    THROW("'" + src + "' is not a recognizable ScriptSourceRowKind");
}





bool operator==(const EarlyExercisePdeLevelKind_& lhs, const EarlyExercisePdeLevelKind_& rhs) {return lhs.val_ == rhs.val_;}
namespace {
    Vector_<EarlyExercisePdeLevelKind_>& TheEarlyExercisePdeLevelKindList() {
        RETURN_STATIC(Vector_<EarlyExercisePdeLevelKind_>);
    }
}    // leave local

Vector_<EarlyExercisePdeLevelKind_> EarlyExercisePdeLevelKindListAll() {
   if (TheEarlyExercisePdeLevelKindList().empty()) {
        TheEarlyExercisePdeLevelKindList().emplace_back(EarlyExercisePdeLevelKind_::Value_::BERMUDAN_PROJECTION);
        TheEarlyExercisePdeLevelKindList().emplace_back(EarlyExercisePdeLevelKind_::Value_::AMERICAN_LCP);
        TheEarlyExercisePdeLevelKindList().emplace_back(EarlyExercisePdeLevelKind_::Value_::TERMINAL);
   }
   return TheEarlyExercisePdeLevelKindList();
}


const char* EarlyExercisePdeLevelKind_::String() const {
    switch (val_)
    {
    default:
    case Value_::_NOT_SET:
        return 0;
        case Value_::BERMUDAN_PROJECTION:
        return "BERMUDAN_PROJECTION";
    case Value_::AMERICAN_LCP:
        return "AMERICAN_LCP";
    case Value_::TERMINAL:
        return "TERMINAL";
        
    }}

struct ReadStringEarlyExercisePdeLevelKind_
{
    ReadStringEarlyExercisePdeLevelKind_() {}

    bool operator()(const String_& src, EarlyExercisePdeLevelKind_::Value_* val) const    // returns true iff recognized input
    {
        bool ret_val = true;
        if (0);	// otiose code to allow regular else-if structure
        else if (src.empty())
        { ret_val = false; }
        
	else if (String::Equivalent(src, "BERMUDANPROJECTION"))
        *val = EarlyExercisePdeLevelKind_::Value_::BERMUDAN_PROJECTION;

	else if (String::Equivalent(src, "AMERICANLCP"))
        *val = EarlyExercisePdeLevelKind_::Value_::AMERICAN_LCP;

	else if (String::Equivalent(src, "TERMINAL"))
        *val = EarlyExercisePdeLevelKind_::Value_::TERMINAL;
        else
            ret_val = false;
        return ret_val;
    }
};

EarlyExercisePdeLevelKind_::EarlyExercisePdeLevelKind_(const String_& src) {
    static const ReadStringEarlyExercisePdeLevelKind_ READ_FIXED;    // allows precomputation for speed, in constructor
    if (READ_FIXED(src, &val_))
        return;
    THROW("'" + src + "' is not a recognizable EarlyExercisePdeLevelKind");
}




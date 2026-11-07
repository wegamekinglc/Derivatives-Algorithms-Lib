
bool operator==(const EarlyExercisePdeRiskBumpOrigin_& lhs, const EarlyExercisePdeRiskBumpOrigin_& rhs) {return lhs.val_ == rhs.val_;}
namespace {
    Vector_<EarlyExercisePdeRiskBumpOrigin_>& TheEarlyExercisePdeRiskBumpOriginList() {
        RETURN_STATIC(Vector_<EarlyExercisePdeRiskBumpOrigin_>);
    }
}    // leave local

Vector_<EarlyExercisePdeRiskBumpOrigin_> EarlyExercisePdeRiskBumpOriginListAll() {
   if (TheEarlyExercisePdeRiskBumpOriginList().empty()) {
        TheEarlyExercisePdeRiskBumpOriginList().emplace_back(EarlyExercisePdeRiskBumpOrigin_::Value_::DEFAULT);
        TheEarlyExercisePdeRiskBumpOriginList().emplace_back(EarlyExercisePdeRiskBumpOrigin_::Value_::EXPLICIT);
   }
   return TheEarlyExercisePdeRiskBumpOriginList();
}


const char* EarlyExercisePdeRiskBumpOrigin_::String() const {
    switch (val_)
    {
    default:
    case Value_::_NOT_SET:
        return 0;
        case Value_::DEFAULT:
        return "DEFAULT";
    case Value_::EXPLICIT:
        return "EXPLICIT";
        
    }}

struct ReadStringEarlyExercisePdeRiskBumpOrigin_
{
    ReadStringEarlyExercisePdeRiskBumpOrigin_() {}

    bool operator()(const String_& src, EarlyExercisePdeRiskBumpOrigin_::Value_* val) const    // returns true iff recognized input
    {
        bool ret_val = true;
        if (0);	// otiose code to allow regular else-if structure
        else if (src.empty())
        { ret_val = false; }
        
	else if (String::Equivalent(src, "DEFAULT"))
        *val = EarlyExercisePdeRiskBumpOrigin_::Value_::DEFAULT;

	else if (String::Equivalent(src, "EXPLICIT"))
        *val = EarlyExercisePdeRiskBumpOrigin_::Value_::EXPLICIT;
        else
            ret_val = false;
        return ret_val;
    }
};

EarlyExercisePdeRiskBumpOrigin_::EarlyExercisePdeRiskBumpOrigin_(const String_& src) {
    static const ReadStringEarlyExercisePdeRiskBumpOrigin_ READ_FIXED;    // allows precomputation for speed, in constructor
    if (READ_FIXED(src, &val_))
        return;
    THROW("'" + src + "' is not a recognizable EarlyExercisePdeRiskBumpOrigin");
}





bool operator==(const EarlyExercisePdeRiskWorkLimitSource_& lhs, const EarlyExercisePdeRiskWorkLimitSource_& rhs) {return lhs.val_ == rhs.val_;}
namespace {
    Vector_<EarlyExercisePdeRiskWorkLimitSource_>& TheEarlyExercisePdeRiskWorkLimitSourceList() {
        RETURN_STATIC(Vector_<EarlyExercisePdeRiskWorkLimitSource_>);
    }
}    // leave local

Vector_<EarlyExercisePdeRiskWorkLimitSource_> EarlyExercisePdeRiskWorkLimitSourceListAll() {
   if (TheEarlyExercisePdeRiskWorkLimitSourceList().empty()) {
        TheEarlyExercisePdeRiskWorkLimitSourceList().emplace_back(EarlyExercisePdeRiskWorkLimitSource_::Value_::NONE);
        TheEarlyExercisePdeRiskWorkLimitSourceList().emplace_back(EarlyExercisePdeRiskWorkLimitSource_::Value_::SCENARIO);
        TheEarlyExercisePdeRiskWorkLimitSourceList().emplace_back(EarlyExercisePdeRiskWorkLimitSource_::Value_::GLOBAL);
   }
   return TheEarlyExercisePdeRiskWorkLimitSourceList();
}


const char* EarlyExercisePdeRiskWorkLimitSource_::String() const {
    switch (val_)
    {
    default:
    case Value_::_NOT_SET:
        return 0;
        case Value_::NONE:
        return "NONE";
    case Value_::SCENARIO:
        return "SCENARIO";
    case Value_::GLOBAL:
        return "GLOBAL";
        
    }}

struct ReadStringEarlyExercisePdeRiskWorkLimitSource_
{
    ReadStringEarlyExercisePdeRiskWorkLimitSource_() {}

    bool operator()(const String_& src, EarlyExercisePdeRiskWorkLimitSource_::Value_* val) const    // returns true iff recognized input
    {
        bool ret_val = true;
        if (0);	// otiose code to allow regular else-if structure
        else if (src.empty())
        { ret_val = false; }
        
	else if (String::Equivalent(src, "NONE"))
        *val = EarlyExercisePdeRiskWorkLimitSource_::Value_::NONE;

	else if (String::Equivalent(src, "SCENARIO"))
        *val = EarlyExercisePdeRiskWorkLimitSource_::Value_::SCENARIO;

	else if (String::Equivalent(src, "GLOBAL"))
        *val = EarlyExercisePdeRiskWorkLimitSource_::Value_::GLOBAL;
        else
            ret_val = false;
        return ret_val;
    }
};

EarlyExercisePdeRiskWorkLimitSource_::EarlyExercisePdeRiskWorkLimitSource_(const String_& src) {
    static const ReadStringEarlyExercisePdeRiskWorkLimitSource_ READ_FIXED;    // allows precomputation for speed, in constructor
    if (READ_FIXED(src, &val_))
        return;
    THROW("'" + src + "' is not a recognizable EarlyExercisePdeRiskWorkLimitSource");
}




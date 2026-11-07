
bool operator==(const EarlyExercisePdeRiskStatus_& lhs, const EarlyExercisePdeRiskStatus_& rhs) {return lhs.val_ == rhs.val_;}
namespace {
    Vector_<EarlyExercisePdeRiskStatus_>& TheEarlyExercisePdeRiskStatusList() {
        RETURN_STATIC(Vector_<EarlyExercisePdeRiskStatus_>);
    }
}    // leave local

Vector_<EarlyExercisePdeRiskStatus_> EarlyExercisePdeRiskStatusListAll() {
   if (TheEarlyExercisePdeRiskStatusList().empty()) {
        TheEarlyExercisePdeRiskStatusList().emplace_back(EarlyExercisePdeRiskStatus_::Value_::COMPLETE);
        TheEarlyExercisePdeRiskStatusList().emplace_back(EarlyExercisePdeRiskStatus_::Value_::PARTIAL);
        TheEarlyExercisePdeRiskStatusList().emplace_back(EarlyExercisePdeRiskStatus_::Value_::BASE_FAILED);
        TheEarlyExercisePdeRiskStatusList().emplace_back(EarlyExercisePdeRiskStatus_::Value_::WORK_LIMIT);
   }
   return TheEarlyExercisePdeRiskStatusList();
}


const char* EarlyExercisePdeRiskStatus_::String() const {
    switch (val_)
    {
    default:
    case Value_::_NOT_SET:
        return 0;
        case Value_::COMPLETE:
        return "COMPLETE";
    case Value_::PARTIAL:
        return "PARTIAL";
    case Value_::BASE_FAILED:
        return "BASE_FAILED";
    case Value_::WORK_LIMIT:
        return "WORK_LIMIT";
        
    }}

struct ReadStringEarlyExercisePdeRiskStatus_
{
    ReadStringEarlyExercisePdeRiskStatus_() {}

    bool operator()(const String_& src, EarlyExercisePdeRiskStatus_::Value_* val) const    // returns true iff recognized input
    {
        bool ret_val = true;
        if (0);	// otiose code to allow regular else-if structure
        else if (src.empty())
        { ret_val = false; }
        
	else if (String::Equivalent(src, "COMPLETE"))
        *val = EarlyExercisePdeRiskStatus_::Value_::COMPLETE;

	else if (String::Equivalent(src, "PARTIAL"))
        *val = EarlyExercisePdeRiskStatus_::Value_::PARTIAL;

	else if (String::Equivalent(src, "BASEFAILED"))
        *val = EarlyExercisePdeRiskStatus_::Value_::BASE_FAILED;

	else if (String::Equivalent(src, "WORKLIMIT"))
        *val = EarlyExercisePdeRiskStatus_::Value_::WORK_LIMIT;
        else
            ret_val = false;
        return ret_val;
    }
};

EarlyExercisePdeRiskStatus_::EarlyExercisePdeRiskStatus_(const String_& src) {
    static const ReadStringEarlyExercisePdeRiskStatus_ READ_FIXED;    // allows precomputation for speed, in constructor
    if (READ_FIXED(src, &val_))
        return;
    THROW("'" + src + "' is not a recognizable EarlyExercisePdeRiskStatus");
}




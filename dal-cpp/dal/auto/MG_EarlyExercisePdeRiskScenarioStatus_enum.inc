
bool operator==(const EarlyExercisePdeRiskScenarioStatus_& lhs, const EarlyExercisePdeRiskScenarioStatus_& rhs) {return lhs.val_ == rhs.val_;}
namespace {
    Vector_<EarlyExercisePdeRiskScenarioStatus_>& TheEarlyExercisePdeRiskScenarioStatusList() {
        RETURN_STATIC(Vector_<EarlyExercisePdeRiskScenarioStatus_>);
    }
}    // leave local

Vector_<EarlyExercisePdeRiskScenarioStatus_> EarlyExercisePdeRiskScenarioStatusListAll() {
   if (TheEarlyExercisePdeRiskScenarioStatusList().empty()) {
        TheEarlyExercisePdeRiskScenarioStatusList().emplace_back(EarlyExercisePdeRiskScenarioStatus_::Value_::PLANNED);
        TheEarlyExercisePdeRiskScenarioStatusList().emplace_back(EarlyExercisePdeRiskScenarioStatus_::Value_::SUCCESS);
        TheEarlyExercisePdeRiskScenarioStatusList().emplace_back(EarlyExercisePdeRiskScenarioStatus_::Value_::NUMERICAL_FAILURE);
        TheEarlyExercisePdeRiskScenarioStatusList().emplace_back(EarlyExercisePdeRiskScenarioStatus_::Value_::NOT_RUN_WORK_LIMIT);
        TheEarlyExercisePdeRiskScenarioStatusList().emplace_back(EarlyExercisePdeRiskScenarioStatus_::Value_::NOT_RUN_BASE_FAILED);
   }
   return TheEarlyExercisePdeRiskScenarioStatusList();
}


const char* EarlyExercisePdeRiskScenarioStatus_::String() const {
    switch (val_)
    {
    default:
    case Value_::_NOT_SET:
        return 0;
        case Value_::PLANNED:
        return "PLANNED";
    case Value_::SUCCESS:
        return "SUCCESS";
    case Value_::NUMERICAL_FAILURE:
        return "NUMERICAL_FAILURE";
    case Value_::NOT_RUN_WORK_LIMIT:
        return "NOT_RUN_WORK_LIMIT";
    case Value_::NOT_RUN_BASE_FAILED:
        return "NOT_RUN_BASE_FAILED";
        
    }}

struct ReadStringEarlyExercisePdeRiskScenarioStatus_
{
    ReadStringEarlyExercisePdeRiskScenarioStatus_() {}

    bool operator()(const String_& src, EarlyExercisePdeRiskScenarioStatus_::Value_* val) const    // returns true iff recognized input
    {
        bool ret_val = true;
        if (0);	// otiose code to allow regular else-if structure
        else if (src.empty())
        { ret_val = false; }
        
	else if (String::Equivalent(src, "PLANNED"))
        *val = EarlyExercisePdeRiskScenarioStatus_::Value_::PLANNED;

	else if (String::Equivalent(src, "SUCCESS"))
        *val = EarlyExercisePdeRiskScenarioStatus_::Value_::SUCCESS;

	else if (String::Equivalent(src, "NUMERICALFAILURE"))
        *val = EarlyExercisePdeRiskScenarioStatus_::Value_::NUMERICAL_FAILURE;

	else if (String::Equivalent(src, "NOTRUNWORKLIMIT"))
        *val = EarlyExercisePdeRiskScenarioStatus_::Value_::NOT_RUN_WORK_LIMIT;

	else if (String::Equivalent(src, "NOTRUNBASEFAILED"))
        *val = EarlyExercisePdeRiskScenarioStatus_::Value_::NOT_RUN_BASE_FAILED;
        else
            ret_val = false;
        return ret_val;
    }
};

EarlyExercisePdeRiskScenarioStatus_::EarlyExercisePdeRiskScenarioStatus_(const String_& src) {
    static const ReadStringEarlyExercisePdeRiskScenarioStatus_ READ_FIXED;    // allows precomputation for speed, in constructor
    if (READ_FIXED(src, &val_))
        return;
    THROW("'" + src + "' is not a recognizable EarlyExercisePdeRiskScenarioStatus");
}





bool operator==(const EarlyExercisePdeRiskAvailability_& lhs, const EarlyExercisePdeRiskAvailability_& rhs) {return lhs.val_ == rhs.val_;}
namespace {
    Vector_<EarlyExercisePdeRiskAvailability_>& TheEarlyExercisePdeRiskAvailabilityList() {
        RETURN_STATIC(Vector_<EarlyExercisePdeRiskAvailability_>);
    }
}    // leave local

Vector_<EarlyExercisePdeRiskAvailability_> EarlyExercisePdeRiskAvailabilityListAll() {
   if (TheEarlyExercisePdeRiskAvailabilityList().empty()) {
        TheEarlyExercisePdeRiskAvailabilityList().emplace_back(EarlyExercisePdeRiskAvailability_::Value_::AVAILABLE);
        TheEarlyExercisePdeRiskAvailabilityList().emplace_back(EarlyExercisePdeRiskAvailability_::Value_::AVAILABLE_EXERCISE_REGION);
        TheEarlyExercisePdeRiskAvailabilityList().emplace_back(EarlyExercisePdeRiskAvailability_::Value_::UNAVAILABLE_BASE_FAILED);
        TheEarlyExercisePdeRiskAvailabilityList().emplace_back(EarlyExercisePdeRiskAvailability_::Value_::UNAVAILABLE_WORK_LIMIT);
        TheEarlyExercisePdeRiskAvailabilityList().emplace_back(EarlyExercisePdeRiskAvailability_::Value_::UNAVAILABLE_SCENARIO_FAILED);
        TheEarlyExercisePdeRiskAvailabilityList().emplace_back(EarlyExercisePdeRiskAvailability_::Value_::UNAVAILABLE_DEGENERATE_DIFFUSION);
        TheEarlyExercisePdeRiskAvailabilityList().emplace_back(EarlyExercisePdeRiskAvailability_::Value_::UNAVAILABLE_DOMAIN_BOUNDARY);
        TheEarlyExercisePdeRiskAvailabilityList().emplace_back(EarlyExercisePdeRiskAvailability_::Value_::UNAVAILABLE_NEAR_EXERCISE_BOUNDARY);
        TheEarlyExercisePdeRiskAvailabilityList().emplace_back(EarlyExercisePdeRiskAvailability_::Value_::UNAVAILABLE_EXERCISE_SWITCH);
        TheEarlyExercisePdeRiskAvailabilityList().emplace_back(EarlyExercisePdeRiskAvailability_::Value_::UNAVAILABLE_NONFINITE);
        TheEarlyExercisePdeRiskAvailabilityList().emplace_back(EarlyExercisePdeRiskAvailability_::Value_::UNAVAILABLE_STEP_UNSTABLE);
   }
   return TheEarlyExercisePdeRiskAvailabilityList();
}


const char* EarlyExercisePdeRiskAvailability_::String() const {
    switch (val_)
    {
    default:
    case Value_::_NOT_SET:
        return 0;
        case Value_::AVAILABLE:
        return "AVAILABLE";
    case Value_::AVAILABLE_EXERCISE_REGION:
        return "AVAILABLE_EXERCISE_REGION";
    case Value_::UNAVAILABLE_BASE_FAILED:
        return "UNAVAILABLE_BASE_FAILED";
    case Value_::UNAVAILABLE_WORK_LIMIT:
        return "UNAVAILABLE_WORK_LIMIT";
    case Value_::UNAVAILABLE_SCENARIO_FAILED:
        return "UNAVAILABLE_SCENARIO_FAILED";
    case Value_::UNAVAILABLE_DEGENERATE_DIFFUSION:
        return "UNAVAILABLE_DEGENERATE_DIFFUSION";
    case Value_::UNAVAILABLE_DOMAIN_BOUNDARY:
        return "UNAVAILABLE_DOMAIN_BOUNDARY";
    case Value_::UNAVAILABLE_NEAR_EXERCISE_BOUNDARY:
        return "UNAVAILABLE_NEAR_EXERCISE_BOUNDARY";
    case Value_::UNAVAILABLE_EXERCISE_SWITCH:
        return "UNAVAILABLE_EXERCISE_SWITCH";
    case Value_::UNAVAILABLE_NONFINITE:
        return "UNAVAILABLE_NONFINITE";
    case Value_::UNAVAILABLE_STEP_UNSTABLE:
        return "UNAVAILABLE_STEP_UNSTABLE";
        
    }}

struct ReadStringEarlyExercisePdeRiskAvailability_
{
    ReadStringEarlyExercisePdeRiskAvailability_() {}

    bool operator()(const String_& src, EarlyExercisePdeRiskAvailability_::Value_* val) const    // returns true iff recognized input
    {
        bool ret_val = true;
        if (0);	// otiose code to allow regular else-if structure
        else if (src.empty())
        { ret_val = false; }
        
	else if (String::Equivalent(src, "AVAILABLE"))
        *val = EarlyExercisePdeRiskAvailability_::Value_::AVAILABLE;

	else if (String::Equivalent(src, "AVAILABLEEXERCISEREGION"))
        *val = EarlyExercisePdeRiskAvailability_::Value_::AVAILABLE_EXERCISE_REGION;

	else if (String::Equivalent(src, "UNAVAILABLEBASEFAILED"))
        *val = EarlyExercisePdeRiskAvailability_::Value_::UNAVAILABLE_BASE_FAILED;

	else if (String::Equivalent(src, "UNAVAILABLEWORKLIMIT"))
        *val = EarlyExercisePdeRiskAvailability_::Value_::UNAVAILABLE_WORK_LIMIT;

	else if (String::Equivalent(src, "UNAVAILABLESCENARIOFAILED"))
        *val = EarlyExercisePdeRiskAvailability_::Value_::UNAVAILABLE_SCENARIO_FAILED;

	else if (String::Equivalent(src, "UNAVAILABLEDEGENERATEDIFFUSION"))
        *val = EarlyExercisePdeRiskAvailability_::Value_::UNAVAILABLE_DEGENERATE_DIFFUSION;

	else if (String::Equivalent(src, "UNAVAILABLEDOMAINBOUNDARY"))
        *val = EarlyExercisePdeRiskAvailability_::Value_::UNAVAILABLE_DOMAIN_BOUNDARY;

	else if (String::Equivalent(src, "UNAVAILABLENEAREXERCISEBOUNDARY"))
        *val = EarlyExercisePdeRiskAvailability_::Value_::UNAVAILABLE_NEAR_EXERCISE_BOUNDARY;

	else if (String::Equivalent(src, "UNAVAILABLEEXERCISESWITCH"))
        *val = EarlyExercisePdeRiskAvailability_::Value_::UNAVAILABLE_EXERCISE_SWITCH;

	else if (String::Equivalent(src, "UNAVAILABLENONFINITE"))
        *val = EarlyExercisePdeRiskAvailability_::Value_::UNAVAILABLE_NONFINITE;

	else if (String::Equivalent(src, "UNAVAILABLESTEPUNSTABLE"))
        *val = EarlyExercisePdeRiskAvailability_::Value_::UNAVAILABLE_STEP_UNSTABLE;
        else
            ret_val = false;
        return ret_val;
    }
};

EarlyExercisePdeRiskAvailability_::EarlyExercisePdeRiskAvailability_(const String_& src) {
    static const ReadStringEarlyExercisePdeRiskAvailability_ READ_FIXED;    // allows precomputation for speed, in constructor
    if (READ_FIXED(src, &val_))
        return;
    THROW("'" + src + "' is not a recognizable EarlyExercisePdeRiskAvailability");
}




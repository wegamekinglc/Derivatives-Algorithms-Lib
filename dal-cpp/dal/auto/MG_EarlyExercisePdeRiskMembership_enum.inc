
bool operator==(const EarlyExercisePdeRiskMembership_& lhs, const EarlyExercisePdeRiskMembership_& rhs) {return lhs.val_ == rhs.val_;}
namespace {
    Vector_<EarlyExercisePdeRiskMembership_>& TheEarlyExercisePdeRiskMembershipList() {
        RETURN_STATIC(Vector_<EarlyExercisePdeRiskMembership_>);
    }
}    // leave local

Vector_<EarlyExercisePdeRiskMembership_> EarlyExercisePdeRiskMembershipListAll() {
   if (TheEarlyExercisePdeRiskMembershipList().empty()) {
        TheEarlyExercisePdeRiskMembershipList().emplace_back(EarlyExercisePdeRiskMembership_::Value_::NOT_EXERCISABLE);
        TheEarlyExercisePdeRiskMembershipList().emplace_back(EarlyExercisePdeRiskMembership_::Value_::CONTINUATION);
        TheEarlyExercisePdeRiskMembershipList().emplace_back(EarlyExercisePdeRiskMembership_::Value_::EXERCISE);
        TheEarlyExercisePdeRiskMembershipList().emplace_back(EarlyExercisePdeRiskMembership_::Value_::BOUNDARY);
   }
   return TheEarlyExercisePdeRiskMembershipList();
}


const char* EarlyExercisePdeRiskMembership_::String() const {
    switch (val_)
    {
    default:
    case Value_::_NOT_SET:
        return 0;
        case Value_::NOT_EXERCISABLE:
        return "NOT_EXERCISABLE";
    case Value_::CONTINUATION:
        return "CONTINUATION";
    case Value_::EXERCISE:
        return "EXERCISE";
    case Value_::BOUNDARY:
        return "BOUNDARY";
        
    }}

struct ReadStringEarlyExercisePdeRiskMembership_
{
    ReadStringEarlyExercisePdeRiskMembership_() {}

    bool operator()(const String_& src, EarlyExercisePdeRiskMembership_::Value_* val) const    // returns true iff recognized input
    {
        bool ret_val = true;
        if (0);	// otiose code to allow regular else-if structure
        else if (src.empty())
        { ret_val = false; }
        
	else if (String::Equivalent(src, "NOTEXERCISABLE"))
        *val = EarlyExercisePdeRiskMembership_::Value_::NOT_EXERCISABLE;

	else if (String::Equivalent(src, "CONTINUATION"))
        *val = EarlyExercisePdeRiskMembership_::Value_::CONTINUATION;

	else if (String::Equivalent(src, "EXERCISE"))
        *val = EarlyExercisePdeRiskMembership_::Value_::EXERCISE;

	else if (String::Equivalent(src, "BOUNDARY"))
        *val = EarlyExercisePdeRiskMembership_::Value_::BOUNDARY;
        else
            ret_val = false;
        return ret_val;
    }
};

EarlyExercisePdeRiskMembership_::EarlyExercisePdeRiskMembership_(const String_& src) {
    static const ReadStringEarlyExercisePdeRiskMembership_ READ_FIXED;    // allows precomputation for speed, in constructor
    if (READ_FIXED(src, &val_))
        return;
    THROW("'" + src + "' is not a recognizable EarlyExercisePdeRiskMembership");
}




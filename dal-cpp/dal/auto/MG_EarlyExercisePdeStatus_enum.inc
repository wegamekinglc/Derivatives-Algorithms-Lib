
bool operator==(const EarlyExercisePdeStatus_& lhs, const EarlyExercisePdeStatus_& rhs) {return lhs.val_ == rhs.val_;}
namespace {
    Vector_<EarlyExercisePdeStatus_>& TheEarlyExercisePdeStatusList() {
        RETURN_STATIC(Vector_<EarlyExercisePdeStatus_>);
    }
}    // leave local

Vector_<EarlyExercisePdeStatus_> EarlyExercisePdeStatusListAll() {
   if (TheEarlyExercisePdeStatusList().empty()) {
        TheEarlyExercisePdeStatusList().emplace_back(EarlyExercisePdeStatus_::Value_::SUCCESS);
        TheEarlyExercisePdeStatusList().emplace_back(EarlyExercisePdeStatus_::Value_::MAX_ITERATIONS);
        TheEarlyExercisePdeStatusList().emplace_back(EarlyExercisePdeStatus_::Value_::LCP_RESIDUAL_LIMIT);
        TheEarlyExercisePdeStatusList().emplace_back(EarlyExercisePdeStatus_::Value_::LINEAR_SOLVE_FAILURE);
        TheEarlyExercisePdeStatusList().emplace_back(EarlyExercisePdeStatus_::Value_::NON_MONOTONE_OPERATOR);
        TheEarlyExercisePdeStatusList().emplace_back(EarlyExercisePdeStatus_::Value_::NONFINITE);
        TheEarlyExercisePdeStatusList().emplace_back(EarlyExercisePdeStatus_::Value_::WORK_LIMIT);
   }
   return TheEarlyExercisePdeStatusList();
}


const char* EarlyExercisePdeStatus_::String() const {
    switch (val_)
    {
    default:
    case Value_::_NOT_SET:
        return 0;
        case Value_::SUCCESS:
        return "SUCCESS";
    case Value_::MAX_ITERATIONS:
        return "MAX_ITERATIONS";
    case Value_::LCP_RESIDUAL_LIMIT:
        return "LCP_RESIDUAL_LIMIT";
    case Value_::LINEAR_SOLVE_FAILURE:
        return "LINEAR_SOLVE_FAILURE";
    case Value_::NON_MONOTONE_OPERATOR:
        return "NON_MONOTONE_OPERATOR";
    case Value_::NONFINITE:
        return "NONFINITE";
    case Value_::WORK_LIMIT:
        return "WORK_LIMIT";
        
    }}

struct ReadStringEarlyExercisePdeStatus_
{
    ReadStringEarlyExercisePdeStatus_() {}

    bool operator()(const String_& src, EarlyExercisePdeStatus_::Value_* val) const    // returns true iff recognized input
    {
        bool ret_val = true;
        if (0);	// otiose code to allow regular else-if structure
        else if (src.empty())
        { ret_val = false; }
        
	else if (String::Equivalent(src, "SUCCESS"))
        *val = EarlyExercisePdeStatus_::Value_::SUCCESS;

	else if (String::Equivalent(src, "MAXITERATIONS"))
        *val = EarlyExercisePdeStatus_::Value_::MAX_ITERATIONS;

	else if (String::Equivalent(src, "LCPRESIDUALLIMIT"))
        *val = EarlyExercisePdeStatus_::Value_::LCP_RESIDUAL_LIMIT;

	else if (String::Equivalent(src, "LINEARSOLVEFAILURE"))
        *val = EarlyExercisePdeStatus_::Value_::LINEAR_SOLVE_FAILURE;

	else if (String::Equivalent(src, "NONMONOTONEOPERATOR"))
        *val = EarlyExercisePdeStatus_::Value_::NON_MONOTONE_OPERATOR;

	else if (String::Equivalent(src, "NONFINITE"))
        *val = EarlyExercisePdeStatus_::Value_::NONFINITE;

	else if (String::Equivalent(src, "WORKLIMIT"))
        *val = EarlyExercisePdeStatus_::Value_::WORK_LIMIT;
        else
            ret_val = false;
        return ret_val;
    }
};

EarlyExercisePdeStatus_::EarlyExercisePdeStatus_(const String_& src) {
    static const ReadStringEarlyExercisePdeStatus_ READ_FIXED;    // allows precomputation for speed, in constructor
    if (READ_FIXED(src, &val_))
        return;
    THROW("'" + src + "' is not a recognizable EarlyExercisePdeStatus");
}




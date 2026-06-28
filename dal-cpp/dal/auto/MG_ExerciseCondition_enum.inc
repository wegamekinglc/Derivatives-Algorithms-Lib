
bool operator==(const ExerciseCondition_& lhs, const ExerciseCondition_& rhs) {return lhs.val_ == rhs.val_;}
namespace {
    Vector_<ExerciseCondition_>& TheExerciseConditionList() {
        RETURN_STATIC(Vector_<ExerciseCondition_>);
    }
}    // leave local

Vector_<ExerciseCondition_> ExerciseConditionListAll() {
   if (TheExerciseConditionList().empty()) {
        TheExerciseConditionList().emplace_back(ExerciseCondition_::Value_::UNCONDITIONAL);
        TheExerciseConditionList().emplace_back(ExerciseCondition_::Value_::ON_EXERCISE);
        TheExerciseConditionList().emplace_back(ExerciseCondition_::Value_::ON_BARRIER_HIT);
        TheExerciseConditionList().emplace_back(ExerciseCondition_::Value_::ON_CONTINUATION);
   }
   return TheExerciseConditionList();
}


const char* ExerciseCondition_::String() const {
    switch (val_)
    {
    default:
    case Value_::_NOT_SET:
        return 0;
        case Value_::UNCONDITIONAL:
        return "UNCONDITIONAL";
    case Value_::ON_EXERCISE:
        return "ON_EXERCISE";
    case Value_::ON_BARRIER_HIT:
        return "ON_BARRIER_HIT";
    case Value_::ON_CONTINUATION:
        return "ON_CONTINUATION";
        
    }}

struct ReadStringExerciseCondition_
{
    ReadStringExerciseCondition_() {}

    bool operator()(const String_& src, ExerciseCondition_::Value_* val) const    // returns true iff recognized input
    {
        bool ret_val = true;
        if (0);	// otiose code to allow regular else-if structure
        else if (src.empty())
        { ret_val = false; }
        
	else if (String::Equivalent(src, "UNCONDITIONAL"))
        *val = ExerciseCondition_::Value_::UNCONDITIONAL;

	else if (String::Equivalent(src, "ONEXERCISE"))
        *val = ExerciseCondition_::Value_::ON_EXERCISE;

	else if (String::Equivalent(src, "ONBARRIERHIT"))
        *val = ExerciseCondition_::Value_::ON_BARRIER_HIT;

	else if (String::Equivalent(src, "ONCONTINUATION"))
        *val = ExerciseCondition_::Value_::ON_CONTINUATION;
        else
            ret_val = false;
        return ret_val;
    }
};

ExerciseCondition_::ExerciseCondition_(const String_& src) {
    static const ReadStringExerciseCondition_ READ_FIXED;    // allows precomputation for speed, in constructor
    if (READ_FIXED(src, &val_))
        return;
    THROW("'" + src + "' is not a recognizable ExerciseCondition");
}




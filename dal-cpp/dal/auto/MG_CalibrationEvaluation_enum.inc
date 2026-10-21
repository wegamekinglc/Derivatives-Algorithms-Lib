
bool operator==(const CalibrationEvaluation_& lhs, const CalibrationEvaluation_& rhs) {return lhs.val_ == rhs.val_;}
namespace {
    Vector_<CalibrationEvaluation_>& TheCalibrationEvaluationList() {
        RETURN_STATIC(Vector_<CalibrationEvaluation_>);
    }
}    // leave local

Vector_<CalibrationEvaluation_> CalibrationEvaluationListAll() {
   if (TheCalibrationEvaluationList().empty()) {
        TheCalibrationEvaluationList().emplace_back(CalibrationEvaluation_::Value_::AutoFast);
        TheCalibrationEvaluationList().emplace_back(CalibrationEvaluation_::Value_::RequireFast);
        TheCalibrationEvaluationList().emplace_back(CalibrationEvaluation_::Value_::GenericDeterministic);
        TheCalibrationEvaluationList().emplace_back(CalibrationEvaluation_::Value_::ScriptMC);
   }
   return TheCalibrationEvaluationList();
}


const char* CalibrationEvaluation_::String() const {
    switch (val_)
    {
    default:
    case Value_::_NOT_SET:
        return 0;
        case Value_::AutoFast:
        return "AutoFast";
    case Value_::RequireFast:
        return "RequireFast";
    case Value_::GenericDeterministic:
        return "GenericDeterministic";
    case Value_::ScriptMC:
        return "ScriptMC";
        
    }}

struct ReadStringCalibrationEvaluation_
{
    ReadStringCalibrationEvaluation_() {}

    bool operator()(const String_& src, CalibrationEvaluation_::Value_* val) const    // returns true iff recognized input
    {
        bool ret_val = true;
        if (0);	// otiose code to allow regular else-if structure
        else if (src.empty())
        { ret_val = false; }
        
	else if (String::Equivalent(src, "AUTOFAST"))
        *val = CalibrationEvaluation_::Value_::AutoFast;

	else if (String::Equivalent(src, "REQUIREFAST"))
        *val = CalibrationEvaluation_::Value_::RequireFast;

	else if (String::Equivalent(src, "GENERICDETERMINISTIC"))
        *val = CalibrationEvaluation_::Value_::GenericDeterministic;

	else if (String::Equivalent(src, "SCRIPTMC"))
        *val = CalibrationEvaluation_::Value_::ScriptMC;
        else
            ret_val = false;
        return ret_val;
    }
};

CalibrationEvaluation_::CalibrationEvaluation_(const String_& src) {
    static const ReadStringCalibrationEvaluation_ READ_FIXED;    // allows precomputation for speed, in constructor
    if (READ_FIXED(src, &val_))
        return;
    THROW("'" + src + "' is not a recognizable CalibrationEvaluation");
}




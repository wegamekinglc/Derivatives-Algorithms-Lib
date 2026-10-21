
bool operator==(const CalibrationStage_& lhs, const CalibrationStage_& rhs) {return lhs.val_ == rhs.val_;}
namespace {
    Vector_<CalibrationStage_>& TheCalibrationStageList() {
        RETURN_STATIC(Vector_<CalibrationStage_>);
    }
}    // leave local

Vector_<CalibrationStage_> CalibrationStageListAll() {
   if (TheCalibrationStageList().empty()) {
        TheCalibrationStageList().emplace_back(CalibrationStage_::Value_::Input);
        TheCalibrationStageList().emplace_back(CalibrationStage_::Value_::Prepare);
        TheCalibrationStageList().emplace_back(CalibrationStage_::Value_::Evaluate);
        TheCalibrationStageList().emplace_back(CalibrationStage_::Value_::Solve);
        TheCalibrationStageList().emplace_back(CalibrationStage_::Value_::Validate);
        TheCalibrationStageList().emplace_back(CalibrationStage_::Value_::Load);
   }
   return TheCalibrationStageList();
}


const char* CalibrationStage_::String() const {
    switch (val_)
    {
    default:
    case Value_::_NOT_SET:
        return 0;
        case Value_::Input:
        return "Input";
    case Value_::Prepare:
        return "Prepare";
    case Value_::Evaluate:
        return "Evaluate";
    case Value_::Solve:
        return "Solve";
    case Value_::Validate:
        return "Validate";
    case Value_::Load:
        return "Load";
        
    }}

struct ReadStringCalibrationStage_
{
    ReadStringCalibrationStage_() {}

    bool operator()(const String_& src, CalibrationStage_::Value_* val) const    // returns true iff recognized input
    {
        bool ret_val = true;
        if (0);	// otiose code to allow regular else-if structure
        else if (src.empty())
        { ret_val = false; }
        
	else if (String::Equivalent(src, "INPUT"))
        *val = CalibrationStage_::Value_::Input;

	else if (String::Equivalent(src, "PREPARE"))
        *val = CalibrationStage_::Value_::Prepare;

	else if (String::Equivalent(src, "EVALUATE"))
        *val = CalibrationStage_::Value_::Evaluate;

	else if (String::Equivalent(src, "SOLVE"))
        *val = CalibrationStage_::Value_::Solve;

	else if (String::Equivalent(src, "VALIDATE"))
        *val = CalibrationStage_::Value_::Validate;

	else if (String::Equivalent(src, "LOAD"))
        *val = CalibrationStage_::Value_::Load;
        else
            ret_val = false;
        return ret_val;
    }
};

CalibrationStage_::CalibrationStage_(const String_& src) {
    static const ReadStringCalibrationStage_ READ_FIXED;    // allows precomputation for speed, in constructor
    if (READ_FIXED(src, &val_))
        return;
    THROW("'" + src + "' is not a recognizable CalibrationStage");
}




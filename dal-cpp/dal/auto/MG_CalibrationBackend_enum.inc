
bool operator==(const CalibrationBackend_& lhs, const CalibrationBackend_& rhs) {return lhs.val_ == rhs.val_;}
namespace {
    Vector_<CalibrationBackend_>& TheCalibrationBackendList() {
        RETURN_STATIC(Vector_<CalibrationBackend_>);
    }
}    // leave local

Vector_<CalibrationBackend_> CalibrationBackendListAll() {
   if (TheCalibrationBackendList().empty()) {
        TheCalibrationBackendList().emplace_back(CalibrationBackend_::Value_::FastDeterministic);
        TheCalibrationBackendList().emplace_back(CalibrationBackend_::Value_::FastGaussian);
        TheCalibrationBackendList().emplace_back(CalibrationBackend_::Value_::GenericDeterministic);
        TheCalibrationBackendList().emplace_back(CalibrationBackend_::Value_::ScriptMC);
   }
   return TheCalibrationBackendList();
}


const char* CalibrationBackend_::String() const {
    switch (val_)
    {
    default:
    case Value_::_NOT_SET:
        return 0;
        case Value_::FastDeterministic:
        return "FastDeterministic";
    case Value_::FastGaussian:
        return "FastGaussian";
    case Value_::GenericDeterministic:
        return "GenericDeterministic";
    case Value_::ScriptMC:
        return "ScriptMC";
        
    }}

struct ReadStringCalibrationBackend_
{
    ReadStringCalibrationBackend_() {}

    bool operator()(const String_& src, CalibrationBackend_::Value_* val) const    // returns true iff recognized input
    {
        bool ret_val = true;
        if (0);	// otiose code to allow regular else-if structure
        else if (src.empty())
        { ret_val = false; }
        
	else if (String::Equivalent(src, "FASTDETERMINISTIC"))
        *val = CalibrationBackend_::Value_::FastDeterministic;

	else if (String::Equivalent(src, "FASTGAUSSIAN"))
        *val = CalibrationBackend_::Value_::FastGaussian;

	else if (String::Equivalent(src, "GENERICDETERMINISTIC"))
        *val = CalibrationBackend_::Value_::GenericDeterministic;

	else if (String::Equivalent(src, "SCRIPTMC"))
        *val = CalibrationBackend_::Value_::ScriptMC;
        else
            ret_val = false;
        return ret_val;
    }
};

CalibrationBackend_::CalibrationBackend_(const String_& src) {
    static const ReadStringCalibrationBackend_ READ_FIXED;    // allows precomputation for speed, in constructor
    if (READ_FIXED(src, &val_))
        return;
    THROW("'" + src + "' is not a recognizable CalibrationBackend");
}




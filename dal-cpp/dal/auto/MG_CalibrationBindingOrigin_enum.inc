
bool operator==(const CalibrationBindingOrigin_& lhs, const CalibrationBindingOrigin_& rhs) {return lhs.val_ == rhs.val_;}
namespace {
    Vector_<CalibrationBindingOrigin_>& TheCalibrationBindingOriginList() {
        RETURN_STATIC(Vector_<CalibrationBindingOrigin_>);
    }
}    // leave local

Vector_<CalibrationBindingOrigin_> CalibrationBindingOriginListAll() {
   if (TheCalibrationBindingOriginList().empty()) {
        TheCalibrationBindingOriginList().emplace_back(CalibrationBindingOrigin_::Value_::SourceDefault);
        TheCalibrationBindingOriginList().emplace_back(CalibrationBindingOrigin_::Value_::Literal);
        TheCalibrationBindingOriginList().emplace_back(CalibrationBindingOrigin_::Value_::QuoteCopy);
   }
   return TheCalibrationBindingOriginList();
}


const char* CalibrationBindingOrigin_::String() const {
    switch (val_)
    {
    default:
    case Value_::_NOT_SET:
        return 0;
        case Value_::SourceDefault:
        return "SourceDefault";
    case Value_::Literal:
        return "Literal";
    case Value_::QuoteCopy:
        return "QuoteCopy";
        
    }}

struct ReadStringCalibrationBindingOrigin_
{
    ReadStringCalibrationBindingOrigin_() {}

    bool operator()(const String_& src, CalibrationBindingOrigin_::Value_* val) const    // returns true iff recognized input
    {
        bool ret_val = true;
        if (0);	// otiose code to allow regular else-if structure
        else if (src.empty())
        { ret_val = false; }
        
	else if (String::Equivalent(src, "SOURCEDEFAULT"))
        *val = CalibrationBindingOrigin_::Value_::SourceDefault;

	else if (String::Equivalent(src, "LITERAL"))
        *val = CalibrationBindingOrigin_::Value_::Literal;

	else if (String::Equivalent(src, "QUOTECOPY"))
        *val = CalibrationBindingOrigin_::Value_::QuoteCopy;
        else
            ret_val = false;
        return ret_val;
    }
};

CalibrationBindingOrigin_::CalibrationBindingOrigin_(const String_& src) {
    static const ReadStringCalibrationBindingOrigin_ READ_FIXED;    // allows precomputation for speed, in constructor
    if (READ_FIXED(src, &val_))
        return;
    THROW("'" + src + "' is not a recognizable CalibrationBindingOrigin");
}




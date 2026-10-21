
bool operator==(const CalibrationMeasureKind_& lhs, const CalibrationMeasureKind_& rhs) {return lhs.val_ == rhs.val_;}
namespace {
    Vector_<CalibrationMeasureKind_>& TheCalibrationMeasureKindList() {
        RETURN_STATIC(Vector_<CalibrationMeasureKind_>);
    }
}    // leave local

Vector_<CalibrationMeasureKind_> CalibrationMeasureKindListAll() {
   if (TheCalibrationMeasureKindList().empty()) {
        TheCalibrationMeasureKindList().emplace_back(CalibrationMeasureKind_::Value_::Price);
        TheCalibrationMeasureKindList().emplace_back(CalibrationMeasureKind_::Value_::ParAffineRate);
        TheCalibrationMeasureKindList().emplace_back(CalibrationMeasureKind_::Value_::ParAffineSpread);
        TheCalibrationMeasureKindList().emplace_back(CalibrationMeasureKind_::Value_::FxOutright);
   }
   return TheCalibrationMeasureKindList();
}


const char* CalibrationMeasureKind_::String() const {
    switch (val_)
    {
    default:
    case Value_::_NOT_SET:
        return 0;
        case Value_::Price:
        return "Price";
    case Value_::ParAffineRate:
        return "ParAffineRate";
    case Value_::ParAffineSpread:
        return "ParAffineSpread";
    case Value_::FxOutright:
        return "FxOutright";
        
    }}

struct ReadStringCalibrationMeasureKind_
{
    ReadStringCalibrationMeasureKind_() {}

    bool operator()(const String_& src, CalibrationMeasureKind_::Value_* val) const    // returns true iff recognized input
    {
        bool ret_val = true;
        if (0);	// otiose code to allow regular else-if structure
        else if (src.empty())
        { ret_val = false; }
        
	else if (String::Equivalent(src, "PRICE"))
        *val = CalibrationMeasureKind_::Value_::Price;

	else if (String::Equivalent(src, "PARAFFINERATE"))
        *val = CalibrationMeasureKind_::Value_::ParAffineRate;

	else if (String::Equivalent(src, "PARAFFINESPREAD"))
        *val = CalibrationMeasureKind_::Value_::ParAffineSpread;

	else if (String::Equivalent(src, "FXOUTRIGHT"))
        *val = CalibrationMeasureKind_::Value_::FxOutright;
        else
            ret_val = false;
        return ret_val;
    }
};

CalibrationMeasureKind_::CalibrationMeasureKind_(const String_& src) {
    static const ReadStringCalibrationMeasureKind_ READ_FIXED;    // allows precomputation for speed, in constructor
    if (READ_FIXED(src, &val_))
        return;
    THROW("'" + src + "' is not a recognizable CalibrationMeasureKind");
}




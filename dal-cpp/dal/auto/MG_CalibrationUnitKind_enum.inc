
bool operator==(const CalibrationUnitKind_& lhs, const CalibrationUnitKind_& rhs) {return lhs.val_ == rhs.val_;}
namespace {
    Vector_<CalibrationUnitKind_>& TheCalibrationUnitKindList() {
        RETURN_STATIC(Vector_<CalibrationUnitKind_>);
    }
}    // leave local

Vector_<CalibrationUnitKind_> CalibrationUnitKindListAll() {
   if (TheCalibrationUnitKindList().empty()) {
        TheCalibrationUnitKindList().emplace_back(CalibrationUnitKind_::Value_::Rate);
        TheCalibrationUnitKindList().emplace_back(CalibrationUnitKind_::Value_::BasisSpread);
        TheCalibrationUnitKindList().emplace_back(CalibrationUnitKind_::Value_::Price);
        TheCalibrationUnitKindList().emplace_back(CalibrationUnitKind_::Value_::FxOutright);
        TheCalibrationUnitKindList().emplace_back(CalibrationUnitKind_::Value_::BondPrice);
        TheCalibrationUnitKindList().emplace_back(CalibrationUnitKind_::Value_::YearFraction);
        TheCalibrationUnitKindList().emplace_back(CalibrationUnitKind_::Value_::Dimensionless);
   }
   return TheCalibrationUnitKindList();
}


const char* CalibrationUnitKind_::String() const {
    switch (val_)
    {
    default:
    case Value_::_NOT_SET:
        return 0;
        case Value_::Rate:
        return "Rate";
    case Value_::BasisSpread:
        return "BasisSpread";
    case Value_::Price:
        return "Price";
    case Value_::FxOutright:
        return "FxOutright";
    case Value_::BondPrice:
        return "BondPrice";
    case Value_::YearFraction:
        return "YearFraction";
    case Value_::Dimensionless:
        return "Dimensionless";
        
    }}

struct ReadStringCalibrationUnitKind_
{
    ReadStringCalibrationUnitKind_() {}

    bool operator()(const String_& src, CalibrationUnitKind_::Value_* val) const    // returns true iff recognized input
    {
        bool ret_val = true;
        if (0);	// otiose code to allow regular else-if structure
        else if (src.empty())
        { ret_val = false; }
        
	else if (String::Equivalent(src, "RATE"))
        *val = CalibrationUnitKind_::Value_::Rate;

	else if (String::Equivalent(src, "BASISSPREAD"))
        *val = CalibrationUnitKind_::Value_::BasisSpread;

	else if (String::Equivalent(src, "PRICE"))
        *val = CalibrationUnitKind_::Value_::Price;

	else if (String::Equivalent(src, "FXOUTRIGHT"))
        *val = CalibrationUnitKind_::Value_::FxOutright;

	else if (String::Equivalent(src, "BONDPRICE"))
        *val = CalibrationUnitKind_::Value_::BondPrice;

	else if (String::Equivalent(src, "YEARFRACTION"))
        *val = CalibrationUnitKind_::Value_::YearFraction;

	else if (String::Equivalent(src, "DIMENSIONLESS"))
        *val = CalibrationUnitKind_::Value_::Dimensionless;
        else
            ret_val = false;
        return ret_val;
    }
};

CalibrationUnitKind_::CalibrationUnitKind_(const String_& src) {
    static const ReadStringCalibrationUnitKind_ READ_FIXED;    // allows precomputation for speed, in constructor
    if (READ_FIXED(src, &val_))
        return;
    THROW("'" + src + "' is not a recognizable CalibrationUnitKind");
}




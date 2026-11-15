
bool operator==(const CompoundedOisObservationMode_& lhs, const CompoundedOisObservationMode_& rhs) {return lhs.val_ == rhs.val_;}
namespace {
    Vector_<CompoundedOisObservationMode_>& TheCompoundedOisObservationModeList() {
        RETURN_STATIC(Vector_<CompoundedOisObservationMode_>);
    }
}    // leave local

Vector_<CompoundedOisObservationMode_> CompoundedOisObservationModeListAll() {
   if (TheCompoundedOisObservationModeList().empty()) {
        TheCompoundedOisObservationModeList().emplace_back(CompoundedOisObservationMode_::Value_::None);
        TheCompoundedOisObservationModeList().emplace_back(CompoundedOisObservationMode_::Value_::Lookback);
        TheCompoundedOisObservationModeList().emplace_back(CompoundedOisObservationMode_::Value_::ObservationShift);
        TheCompoundedOisObservationModeList().emplace_back(CompoundedOisObservationMode_::Value_::Lockout);
   }
   return TheCompoundedOisObservationModeList();
}


const char* CompoundedOisObservationMode_::String() const {
    switch (val_)
    {
    default:
    case Value_::_NOT_SET:
        return 0;
        case Value_::None:
        return "None";
    case Value_::Lookback:
        return "Lookback";
    case Value_::ObservationShift:
        return "ObservationShift";
    case Value_::Lockout:
        return "Lockout";
        
    }}

struct ReadStringCompoundedOisObservationMode_
{
    ReadStringCompoundedOisObservationMode_() {}

    bool operator()(const String_& src, CompoundedOisObservationMode_::Value_* val) const    // returns true iff recognized input
    {
        bool ret_val = true;
        if (0);	// otiose code to allow regular else-if structure
        else if (src.empty())
        { ret_val = false; }
        
	else if (String::Equivalent(src, "NONE"))
        *val = CompoundedOisObservationMode_::Value_::None;

	else if (String::Equivalent(src, "LOOKBACK"))
        *val = CompoundedOisObservationMode_::Value_::Lookback;

	else if (String::Equivalent(src, "OBSERVATIONSHIFT"))
        *val = CompoundedOisObservationMode_::Value_::ObservationShift;

	else if (String::Equivalent(src, "LOCKOUT"))
        *val = CompoundedOisObservationMode_::Value_::Lockout;
        else
            ret_val = false;
        return ret_val;
    }
};

CompoundedOisObservationMode_::CompoundedOisObservationMode_(const String_& src) {
    static const ReadStringCompoundedOisObservationMode_ READ_FIXED;    // allows precomputation for speed, in constructor
    if (READ_FIXED(src, &val_))
        return;
    THROW("'" + src + "' is not a recognizable CompoundedOisObservationMode");
}





bool operator==(const DateStepSize_& lhs, const DateStepSize_& rhs) {return lhs.val_ == rhs.val_;}
namespace {
    Vector_<DateStepSize_>& TheDateStepSizeList() {
        RETURN_STATIC(Vector_<DateStepSize_>);
    }
}    // leave local

Vector_<DateStepSize_> DateStepSizeListAll() {
   if (TheDateStepSizeList().empty()) {
        TheDateStepSizeList().emplace_back("Y");
        TheDateStepSizeList().emplace_back("M");
        TheDateStepSizeList().emplace_back("BD");
        TheDateStepSizeList().emplace_back("CD");
   }
   return TheDateStepSizeList();
}


const char* DateStepSize_::String() const {
    switch (val_)
    {
    default:
    case Value_::_NOT_SET:
        return 0;
        case Value_::Y:
        return "Y";
    case Value_::M:
        return "M";
    case Value_::BD:
        return "BD";
    case Value_::CD:
        return "CD";
        
    }}

struct ReadStringDateStepSize_
{
    ReadStringDateStepSize_() {}

    bool operator()(const String_& src, DateStepSize_::Value_* val) const    // returns true iff recognized input
    {
        bool ret_val = true;
        if (0);	// otiose code to allow regular else-if structure
        else if (src.empty())
        { ret_val = false; }
        
	else if (String::Equivalent(src, "Y"))
        *val = DateStepSize_::Value_::Y;
	else if (String::Equivalent(src, "YEAR"))
        *val = DateStepSize_::Value_::Y;
	else if (String::Equivalent(src, "YEARS"))
        *val = DateStepSize_::Value_::Y;

	else if (String::Equivalent(src, "M"))
        *val = DateStepSize_::Value_::M;
	else if (String::Equivalent(src, "MONTH"))
        *val = DateStepSize_::Value_::M;
	else if (String::Equivalent(src, "MONTHS"))
        *val = DateStepSize_::Value_::M;

	else if (String::Equivalent(src, "BD"))
        *val = DateStepSize_::Value_::BD;
	else if (String::Equivalent(src, "BUSDAY"))
        *val = DateStepSize_::Value_::BD;
	else if (String::Equivalent(src, "BUSINESSDAY"))
        *val = DateStepSize_::Value_::BD;

	else if (String::Equivalent(src, "CD"))
        *val = DateStepSize_::Value_::CD;
	else if (String::Equivalent(src, "CALDAY"))
        *val = DateStepSize_::Value_::CD;
	else if (String::Equivalent(src, "CALENDARDAY"))
        *val = DateStepSize_::Value_::CD;
        else
            ret_val = false;
        return ret_val;
    }
};

DateStepSize_::DateStepSize_(const String_& src) {
    static const ReadStringDateStepSize_ READ_FIXED;    // allows precomputation for speed, in constructor
    if (READ_FIXED(src, &val_))
        return;
    THROW("'" + src + "' is not a recognizable DateStepSize");
}




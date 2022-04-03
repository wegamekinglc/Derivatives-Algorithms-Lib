
bool operator==(const SpecialDay_& lhs, const SpecialDay_& rhs) {return lhs.val_ == rhs.val_;}
namespace {
    Vector_<SpecialDay_>& TheSpecialDayList() {
        RETURN_STATIC(Vector_<SpecialDay_>);
    }
}    // leave local

Vector_<SpecialDay_> SpecialDayListAll() {
   if (TheSpecialDayList().empty()) {
        TheSpecialDayList().emplace_back("IMM");
        TheSpecialDayList().emplace_back("IMM1");
        TheSpecialDayList().emplace_back("CDS");
        TheSpecialDayList().emplace_back("EOM");
   }
   return TheSpecialDayList();
}


const char* SpecialDay_::String() const {
    switch (val_)
    {
    default:
    case Value_::_NOT_SET:
        return 0;
        case Value_::IMM:
        return "IMM";
    case Value_::IMM1:
        return "IMM1";
    case Value_::CDS:
        return "CDS";
    case Value_::EOM:
        return "EOM";
        
    }}

struct ReadStringSpecialDay_
{
    ReadStringSpecialDay_() {}

    bool operator()(const String_& src, SpecialDay_::Value_* val) const    // returns true iff recognized input
    {
        bool ret_val = true;
        if (0);	// otiose code to allow regular else-if structure
        else if (src.empty())
        { ret_val = false; }
        
	else if (String::Equivalent(src, "IMM"))
        *val = SpecialDay_::Value_::IMM;
	else if (String::Equivalent(src, "IMM3"))
        *val = SpecialDay_::Value_::IMM;
	else if (String::Equivalent(src, "IMMQUARTERLY"))
        *val = SpecialDay_::Value_::IMM;

	else if (String::Equivalent(src, "IMM1"))
        *val = SpecialDay_::Value_::IMM1;
	else if (String::Equivalent(src, "IMMMONTHLY"))
        *val = SpecialDay_::Value_::IMM1;

	else if (String::Equivalent(src, "CDS"))
        *val = SpecialDay_::Value_::CDS;
	else if (String::Equivalent(src, "CDS3"))
        *val = SpecialDay_::Value_::CDS;
	else if (String::Equivalent(src, "CDSQUARTERLY"))
        *val = SpecialDay_::Value_::CDS;

	else if (String::Equivalent(src, "EOM"))
        *val = SpecialDay_::Value_::EOM;
        else
            ret_val = false;
        return ret_val;
    }
};

SpecialDay_::SpecialDay_(const String_& src) {
    static const ReadStringSpecialDay_ READ_FIXED;    // allows precomputation for speed, in constructor
    if (READ_FIXED(src, &val_))
        return;
    THROW("'" + src + "' is not a recognizable SpecialDay");
}





bool operator==(const PeriodLength_& lhs, const PeriodLength_& rhs) {return lhs.val_ == rhs.val_;}
namespace {
    Vector_<PeriodLength_>& ThePeriodLengthList() {
        RETURN_STATIC(Vector_<PeriodLength_>);
    }
}    // leave local

Vector_<PeriodLength_> PeriodLengthListAll() {
   if (ThePeriodLengthList().empty()) {
        ThePeriodLengthList().emplace_back("ANNUAL");
        ThePeriodLengthList().emplace_back("SEMIANNUAL");
        ThePeriodLengthList().emplace_back("QUARTERLY");
        ThePeriodLengthList().emplace_back("MONTHLY");
   }
   return ThePeriodLengthList();
}


const char* PeriodLength_::String() const {
    switch (val_)
    {
    default:
    case Value_::_NOT_SET:
        return 0;
        case Value_::ANNUAL:
        return "ANNUAL";
	case Value_::SEMIANNUAL:
        return "SEMIANNUAL";
	case Value_::QUARTERLY:
        return "QUARTERLY";
	case Value_::MONTHLY:
        return "MONTHLY";
	    
    }}

struct ReadStringPeriodLength_
{
    ReadStringPeriodLength_() {}

    bool operator()(const String_& src, PeriodLength_::Value_* val) const    // returns true iff recognized input
    {
        bool ret_val = true;
        if (0);	// otiose code to allow regular else-if structure
        else if (src.empty())
        { ret_val = false; }
        
	else if (String::Equivalent(src, "ANNUAL"))
		*val = PeriodLength_::Value_::ANNUAL;
	else if (String::Equivalent(src, "12M"))
		*val = PeriodLength_::Value_::ANNUAL;

	else if (String::Equivalent(src, "SEMIANNUAL"))
		*val = PeriodLength_::Value_::SEMIANNUAL;
	else if (String::Equivalent(src, "SEMI"))
		*val = PeriodLength_::Value_::SEMIANNUAL;
	else if (String::Equivalent(src, "6M"))
		*val = PeriodLength_::Value_::SEMIANNUAL;

	else if (String::Equivalent(src, "QUARTERLY"))
		*val = PeriodLength_::Value_::QUARTERLY;
	else if (String::Equivalent(src, "3M"))
		*val = PeriodLength_::Value_::QUARTERLY;

	else if (String::Equivalent(src, "MONTHLY"))
		*val = PeriodLength_::Value_::MONTHLY;
	else if (String::Equivalent(src, "1M"))
		*val = PeriodLength_::Value_::MONTHLY;
        else
            ret_val = false;
        return ret_val;
    }
};

PeriodLength_::PeriodLength_(const String_& src) {
    static const ReadStringPeriodLength_ READ_FIXED;    // allows precomputation for speed, in constructor
    if (READ_FIXED(src, &val_))
        return;
    THROW("'" + src + "' is not a recognizable PeriodLength");
}





bool operator==(const AnalyticEligibility_& lhs, const AnalyticEligibility_& rhs) {return lhs.val_ == rhs.val_;}
namespace {
    Vector_<AnalyticEligibility_>& TheAnalyticEligibilityList() {
        RETURN_STATIC(Vector_<AnalyticEligibility_>);
    }
}    // leave local

Vector_<AnalyticEligibility_> AnalyticEligibilityListAll() {
   if (TheAnalyticEligibilityList().empty()) {
        TheAnalyticEligibilityList().emplace_back(AnalyticEligibility_::Value_::UNKNOWN);
        TheAnalyticEligibilityList().emplace_back(AnalyticEligibility_::Value_::ELIGIBLE);
        TheAnalyticEligibilityList().emplace_back(AnalyticEligibility_::Value_::INELIGIBLE);
   }
   return TheAnalyticEligibilityList();
}


const char* AnalyticEligibility_::String() const {
    switch (val_)
    {
    default:
    case Value_::_NOT_SET:
        return 0;
        case Value_::UNKNOWN:
        return "UNKNOWN";
    case Value_::ELIGIBLE:
        return "ELIGIBLE";
    case Value_::INELIGIBLE:
        return "INELIGIBLE";
        
    }}

struct ReadStringAnalyticEligibility_
{
    ReadStringAnalyticEligibility_() {}

    bool operator()(const String_& src, AnalyticEligibility_::Value_* val) const    // returns true iff recognized input
    {
        bool ret_val = true;
        if (0);	// otiose code to allow regular else-if structure
        else if (src.empty())
        { ret_val = false; }
        
	else if (String::Equivalent(src, "UNKNOWN"))
        *val = AnalyticEligibility_::Value_::UNKNOWN;

	else if (String::Equivalent(src, "ELIGIBLE"))
        *val = AnalyticEligibility_::Value_::ELIGIBLE;

	else if (String::Equivalent(src, "INELIGIBLE"))
        *val = AnalyticEligibility_::Value_::INELIGIBLE;
        else
            ret_val = false;
        return ret_val;
    }
};

AnalyticEligibility_::AnalyticEligibility_(const String_& src) {
    static const ReadStringAnalyticEligibility_ READ_FIXED;    // allows precomputation for speed, in constructor
    if (READ_FIXED(src, &val_))
        return;
    THROW("'" + src + "' is not a recognizable AnalyticEligibility");
}




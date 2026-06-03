
bool operator==(const CurveKnotPolicy_& lhs, const CurveKnotPolicy_& rhs) {return lhs.val_ == rhs.val_;}
namespace {
    Vector_<CurveKnotPolicy_>& TheCurveKnotPolicyList() {
        RETURN_STATIC(Vector_<CurveKnotPolicy_>);
    }
}    // leave local

Vector_<CurveKnotPolicy_> CurveKnotPolicyListAll() {
   if (TheCurveKnotPolicyList().empty()) {
        TheCurveKnotPolicyList().emplace_back(CurveKnotPolicy_::Value_::INPUT);
        TheCurveKnotPolicyList().emplace_back(CurveKnotPolicy_::Value_::INSTRUMENTS);
        TheCurveKnotPolicyList().emplace_back(CurveKnotPolicy_::Value_::AUGMENTED);
   }
   return TheCurveKnotPolicyList();
}


const char* CurveKnotPolicy_::String() const {
    switch (val_)
    {
    default:
    case Value_::_NOT_SET:
        return 0;
        case Value_::INPUT:
        return "INPUT";
    case Value_::INSTRUMENTS:
        return "INSTRUMENTS";
    case Value_::AUGMENTED:
        return "AUGMENTED";
        
    }}

struct ReadStringCurveKnotPolicy_
{
    ReadStringCurveKnotPolicy_() {}

    bool operator()(const String_& src, CurveKnotPolicy_::Value_* val) const    // returns true iff recognized input
    {
        bool ret_val = true;
        if (0);	// otiose code to allow regular else-if structure
        else if (src.empty())
        { ret_val = false; }
        
	else if (String::Equivalent(src, "INPUT"))
        *val = CurveKnotPolicy_::Value_::INPUT;

	else if (String::Equivalent(src, "INSTRUMENTS"))
        *val = CurveKnotPolicy_::Value_::INSTRUMENTS;

	else if (String::Equivalent(src, "AUGMENTED"))
        *val = CurveKnotPolicy_::Value_::AUGMENTED;
        else
            ret_val = false;
        return ret_val;
    }
};

CurveKnotPolicy_::CurveKnotPolicy_(const String_& src) {
    static const ReadStringCurveKnotPolicy_ READ_FIXED;    // allows precomputation for speed, in constructor
    if (READ_FIXED(src, &val_))
        return;
    THROW("'" + src + "' is not a recognizable CurveKnotPolicy");
}




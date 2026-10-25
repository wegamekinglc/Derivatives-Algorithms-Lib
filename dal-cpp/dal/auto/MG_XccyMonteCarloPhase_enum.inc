
bool operator==(const XccyMonteCarloPhase_& lhs, const XccyMonteCarloPhase_& rhs) {return lhs.val_ == rhs.val_;}
namespace {
    Vector_<XccyMonteCarloPhase_>& TheXccyMonteCarloPhaseList() {
        RETURN_STATIC(Vector_<XccyMonteCarloPhase_>);
    }
}    // leave local

Vector_<XccyMonteCarloPhase_> XccyMonteCarloPhaseListAll() {
   if (TheXccyMonteCarloPhaseList().empty()) {
        TheXccyMonteCarloPhaseList().emplace_back(XccyMonteCarloPhase_::Value_::Fit);
        TheXccyMonteCarloPhaseList().emplace_back(XccyMonteCarloPhase_::Value_::Validation);
        TheXccyMonteCarloPhaseList().emplace_back(XccyMonteCarloPhase_::Value_::HeldOut);
   }
   return TheXccyMonteCarloPhaseList();
}


const char* XccyMonteCarloPhase_::String() const {
    switch (val_)
    {
    default:
    case Value_::_NOT_SET:
        return 0;
        case Value_::Fit:
        return "Fit";
    case Value_::Validation:
        return "Validation";
    case Value_::HeldOut:
        return "HeldOut";
        
    }}

struct ReadStringXccyMonteCarloPhase_
{
    ReadStringXccyMonteCarloPhase_() {}

    bool operator()(const String_& src, XccyMonteCarloPhase_::Value_* val) const    // returns true iff recognized input
    {
        bool ret_val = true;
        if (0);	// otiose code to allow regular else-if structure
        else if (src.empty())
        { ret_val = false; }
        
	else if (String::Equivalent(src, "FIT"))
        *val = XccyMonteCarloPhase_::Value_::Fit;

	else if (String::Equivalent(src, "VALIDATION"))
        *val = XccyMonteCarloPhase_::Value_::Validation;

	else if (String::Equivalent(src, "HELDOUT"))
        *val = XccyMonteCarloPhase_::Value_::HeldOut;
        else
            ret_val = false;
        return ret_val;
    }
};

XccyMonteCarloPhase_::XccyMonteCarloPhase_(const String_& src) {
    static const ReadStringXccyMonteCarloPhase_ READ_FIXED;    // allows precomputation for speed, in constructor
    if (READ_FIXED(src, &val_))
        return;
    THROW("'" + src + "' is not a recognizable XccyMonteCarloPhase");
}




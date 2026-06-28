
bool operator==(const BrentPhase_& lhs, const BrentPhase_& rhs) {return lhs.val_ == rhs.val_;}
namespace {
    Vector_<BrentPhase_>& TheBrentPhaseList() {
        RETURN_STATIC(Vector_<BrentPhase_>);
    }
}    // leave local

Vector_<BrentPhase_> BrentPhaseListAll() {
   if (TheBrentPhaseList().empty()) {
        TheBrentPhaseList().emplace_back(BrentPhase_::Value_::INITIALIZE);
        TheBrentPhaseList().emplace_back(BrentPhase_::Value_::HUNT);
        TheBrentPhaseList().emplace_back(BrentPhase_::Value_::BRACKETED);
   }
   return TheBrentPhaseList();
}


const char* BrentPhase_::String() const {
    switch (val_)
    {
    default:
    case Value_::_NOT_SET:
        return 0;
        case Value_::INITIALIZE:
        return "INITIALIZE";
    case Value_::HUNT:
        return "HUNT";
    case Value_::BRACKETED:
        return "BRACKETED";
        
    }}

struct ReadStringBrentPhase_
{
    ReadStringBrentPhase_() {}

    bool operator()(const String_& src, BrentPhase_::Value_* val) const    // returns true iff recognized input
    {
        bool ret_val = true;
        if (0);	// otiose code to allow regular else-if structure
        else if (src.empty())
        { ret_val = false; }
        
	else if (String::Equivalent(src, "INITIALIZE"))
        *val = BrentPhase_::Value_::INITIALIZE;

	else if (String::Equivalent(src, "HUNT"))
        *val = BrentPhase_::Value_::HUNT;

	else if (String::Equivalent(src, "BRACKETED"))
        *val = BrentPhase_::Value_::BRACKETED;
        else
            ret_val = false;
        return ret_val;
    }
};

BrentPhase_::BrentPhase_(const String_& src) {
    static const ReadStringBrentPhase_ READ_FIXED;    // allows precomputation for speed, in constructor
    if (READ_FIXED(src, &val_))
        return;
    THROW("'" + src + "' is not a recognizable BrentPhase");
}





bool operator==(const TodayFixingPolicy_& lhs, const TodayFixingPolicy_& rhs) {return lhs.val_ == rhs.val_;}
namespace {
    Vector_<TodayFixingPolicy_>& TheTodayFixingPolicyList() {
        RETURN_STATIC(Vector_<TodayFixingPolicy_>);
    }
}    // leave local

Vector_<TodayFixingPolicy_> TodayFixingPolicyListAll() {
   if (TheTodayFixingPolicyList().empty()) {
        TheTodayFixingPolicyList().emplace_back(TodayFixingPolicy_::Value_::MODEL);
        TheTodayFixingPolicyList().emplace_back(TodayFixingPolicy_::Value_::REQUIREHISTORICAL);
   }
   return TheTodayFixingPolicyList();
}


const char* TodayFixingPolicy_::String() const {
    switch (val_)
    {
    default:
    case Value_::_NOT_SET:
        return 0;
        case Value_::MODEL:
        return "MODEL";
    case Value_::REQUIREHISTORICAL:
        return "REQUIREHISTORICAL";

    }}

struct ReadStringTodayFixingPolicy_
{
    ReadStringTodayFixingPolicy_() {}

    bool operator()(const String_& src, TodayFixingPolicy_::Value_* val) const    // returns true iff recognized input
    {
        bool ret_val = true;
        if (0);	// otiose code to allow regular else-if structure
        else if (src.empty())
        { ret_val = false; }

	else if (String::Equivalent(src, "MODEL"))
        *val = TodayFixingPolicy_::Value_::MODEL;

	else if (String::Equivalent(src, "REQUIREHISTORICAL"))
        *val = TodayFixingPolicy_::Value_::REQUIREHISTORICAL;
        else
            ret_val = false;
        return ret_val;
    }
};

TodayFixingPolicy_::TodayFixingPolicy_(const String_& src) {
    static const ReadStringTodayFixingPolicy_ READ_FIXED;    // allows precomputation for speed, in constructor
    if (READ_FIXED(src, &val_))
        return;
    THROW("'" + src + "' is not a recognizable TodayFixingPolicy");
}

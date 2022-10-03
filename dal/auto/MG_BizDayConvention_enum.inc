
bool operator==(const BizDayConvention_& lhs, const BizDayConvention_& rhs) {return lhs.val_ == rhs.val_;}
namespace {
    Vector_<BizDayConvention_>& TheBizDayConventionList() {
        RETURN_STATIC(Vector_<BizDayConvention_>);
    }
}    // leave local

Vector_<BizDayConvention_> BizDayConventionListAll() {
   if (TheBizDayConventionList().empty()) {
        TheBizDayConventionList().emplace_back(BizDayConvention_::Value_::Following);
        TheBizDayConventionList().emplace_back(BizDayConvention_::Value_::ModifiedFollowing);
   }
   return TheBizDayConventionList();
}


const char* BizDayConvention_::String() const {
    switch (val_)
    {
    default:
    case Value_::_NOT_SET:
        return 0;
        case Value_::Following:
        return "Following";
    case Value_::ModifiedFollowing:
        return "ModifiedFollowing";
        
    }}

struct ReadStringBizDayConvention_
{
    ReadStringBizDayConvention_() {}

    bool operator()(const String_& src, BizDayConvention_::Value_* val) const    // returns true iff recognized input
    {
        bool ret_val = true;
        if (0);	// otiose code to allow regular else-if structure
        else if (src.empty())
        { ret_val = false; }
        
	else if (String::Equivalent(src, "FOLLOWING"))
        *val = BizDayConvention_::Value_::Following;

	else if (String::Equivalent(src, "MODIFIEDFOLLOWING"))
        *val = BizDayConvention_::Value_::ModifiedFollowing;
        else
            ret_val = false;
        return ret_val;
    }
};

BizDayConvention_::BizDayConvention_(const String_& src) {
    static const ReadStringBizDayConvention_ READ_FIXED;    // allows precomputation for speed, in constructor
    if (READ_FIXED(src, &val_))
        return;
    THROW("'" + src + "' is not a recognizable BizDayConvention");
}




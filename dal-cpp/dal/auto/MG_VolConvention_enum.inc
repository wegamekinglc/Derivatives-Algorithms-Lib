
bool operator==(const VolConvention_& lhs, const VolConvention_& rhs) {return lhs.val_ == rhs.val_;}
namespace {
    Vector_<VolConvention_>& TheVolConventionList() {
        RETURN_STATIC(Vector_<VolConvention_>);
    }
}    // leave local

Vector_<VolConvention_> VolConventionListAll() {
   if (TheVolConventionList().empty()) {
        TheVolConventionList().emplace_back(VolConvention_::Value_::NORMAL);
        TheVolConventionList().emplace_back(VolConvention_::Value_::BLACK);
        TheVolConventionList().emplace_back(VolConvention_::Value_::SHIFTED_BLACK);
   }
   return TheVolConventionList();
}


const char* VolConvention_::String() const {
    switch (val_)
    {
    default:
    case Value_::_NOT_SET:
        return 0;
        case Value_::NORMAL:
        return "NORMAL";
    case Value_::BLACK:
        return "BLACK";
    case Value_::SHIFTED_BLACK:
        return "SHIFTED_BLACK";
        
    }}

struct ReadStringVolConvention_
{
    ReadStringVolConvention_() {}

    bool operator()(const String_& src, VolConvention_::Value_* val) const    // returns true iff recognized input
    {
        bool ret_val = true;
        if (0);	// otiose code to allow regular else-if structure
        else if (src.empty())
        { ret_val = false; }
        
	else if (String::Equivalent(src, "NORMAL"))
        *val = VolConvention_::Value_::NORMAL;

	else if (String::Equivalent(src, "BLACK"))
        *val = VolConvention_::Value_::BLACK;

	else if (String::Equivalent(src, "SHIFTEDBLACK"))
        *val = VolConvention_::Value_::SHIFTED_BLACK;
        else
            ret_val = false;
        return ret_val;
    }
};

VolConvention_::VolConvention_(const String_& src) {
    static const ReadStringVolConvention_ READ_FIXED;    // allows precomputation for speed, in constructor
    if (READ_FIXED(src, &val_))
        return;
    THROW("'" + src + "' is not a recognizable VolConvention");
}




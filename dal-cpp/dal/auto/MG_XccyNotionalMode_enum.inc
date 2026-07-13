
bool operator==(const XccyNotionalMode_& lhs, const XccyNotionalMode_& rhs) {return lhs.val_ == rhs.val_;}
namespace {
    Vector_<XccyNotionalMode_>& TheXccyNotionalModeList() {
        RETURN_STATIC(Vector_<XccyNotionalMode_>);
    }
}    // leave local

Vector_<XccyNotionalMode_> XccyNotionalModeListAll() {
   if (TheXccyNotionalModeList().empty()) {
        TheXccyNotionalModeList().emplace_back(XccyNotionalMode_::Value_::FIXED);
        TheXccyNotionalModeList().emplace_back(XccyNotionalMode_::Value_::RESETTABLE);
        TheXccyNotionalModeList().emplace_back(XccyNotionalMode_::Value_::MARK_TO_MARKET);
   }
   return TheXccyNotionalModeList();
}


const char* XccyNotionalMode_::String() const {
    switch (val_)
    {
    default:
    case Value_::_NOT_SET:
        return 0;
        case Value_::FIXED:
        return "FIXED";
    case Value_::RESETTABLE:
        return "RESETTABLE";
    case Value_::MARK_TO_MARKET:
        return "MARK_TO_MARKET";
        
    }}

struct ReadStringXccyNotionalMode_
{
    ReadStringXccyNotionalMode_() {}

    bool operator()(const String_& src, XccyNotionalMode_::Value_* val) const    // returns true iff recognized input
    {
        bool ret_val = true;
        if (0);	// otiose code to allow regular else-if structure
        else if (src.empty())
        { ret_val = false; }
        
	else if (String::Equivalent(src, "FIXED"))
        *val = XccyNotionalMode_::Value_::FIXED;

	else if (String::Equivalent(src, "RESETTABLE"))
        *val = XccyNotionalMode_::Value_::RESETTABLE;

	else if (String::Equivalent(src, "MARKTOMARKET"))
        *val = XccyNotionalMode_::Value_::MARK_TO_MARKET;
        else
            ret_val = false;
        return ret_val;
    }
};

XccyNotionalMode_::XccyNotionalMode_(const String_& src) {
    static const ReadStringXccyNotionalMode_ READ_FIXED;    // allows precomputation for speed, in constructor
    if (READ_FIXED(src, &val_))
        return;
    THROW("'" + src + "' is not a recognizable XccyNotionalMode");
}




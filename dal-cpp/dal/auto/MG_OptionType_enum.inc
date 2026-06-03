
bool operator==(const OptionType_& lhs, const OptionType_& rhs) {return lhs.val_ == rhs.val_;}
namespace {
    Vector_<OptionType_>& TheOptionTypeList() {
        RETURN_STATIC(Vector_<OptionType_>);
    }
}    // leave local

Vector_<OptionType_> OptionTypeListAll() {
   if (TheOptionTypeList().empty()) {
        TheOptionTypeList().emplace_back(OptionType_::Value_::CALL);
        TheOptionTypeList().emplace_back(OptionType_::Value_::PUT);
        TheOptionTypeList().emplace_back(OptionType_::Value_::STRADDLE);
   }
   return TheOptionTypeList();
}


const char* OptionType_::String() const {
    switch (val_)
    {
    default:
    case Value_::_NOT_SET:
        return 0;
        case Value_::CALL:
        return "CALL";
    case Value_::PUT:
        return "PUT";
    case Value_::STRADDLE:
        return "STRADDLE";
        
    }}

struct ReadStringOptionType_
{
    ReadStringOptionType_() {}

    bool operator()(const String_& src, OptionType_::Value_* val) const    // returns true iff recognized input
    {
        bool ret_val = true;
        if (0);	// otiose code to allow regular else-if structure
        else if (src.empty())
        { ret_val = false; }
        
	else if (String::Equivalent(src, "CALL"))
        *val = OptionType_::Value_::CALL;
	else if (String::Equivalent(src, "C"))
        *val = OptionType_::Value_::CALL;

	else if (String::Equivalent(src, "PUT"))
        *val = OptionType_::Value_::PUT;
	else if (String::Equivalent(src, "P"))
        *val = OptionType_::Value_::PUT;

	else if (String::Equivalent(src, "STRADDLE"))
        *val = OptionType_::Value_::STRADDLE;
	else if (String::Equivalent(src, "V"))
        *val = OptionType_::Value_::STRADDLE;
	else if (String::Equivalent(src, "C+P"))
        *val = OptionType_::Value_::STRADDLE;
        else
            ret_val = false;
        return ret_val;
    }
};

OptionType_::OptionType_(const String_& src) {
    static const ReadStringOptionType_ READ_FIXED;    // allows precomputation for speed, in constructor
    if (READ_FIXED(src, &val_))
        return;
    THROW("'" + src + "' is not a recognizable OptionType");
}




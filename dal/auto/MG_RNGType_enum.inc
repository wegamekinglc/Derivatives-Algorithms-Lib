
bool operator==(const RNGType_& lhs, const RNGType_& rhs) {return lhs.val_ == rhs.val_;}
namespace {
    Vector_<RNGType_>& TheRNGTypeList() {
        RETURN_STATIC(Vector_<RNGType_>);
    }
}    // leave local

Vector_<RNGType_> RNGTypeListAll() {
   if (TheRNGTypeList().empty()) {
        TheRNGTypeList().emplace_back("IRN");
        TheRNGTypeList().emplace_back("MRG32");
   }
   return TheRNGTypeList();
}


const char* RNGType_::String() const {
    switch (val_)
    {
    default:
    case Value_::_NOT_SET:
        return 0;
        case Value_::IRN:
        return "IRN";
	case Value_::MRG32:
        return "MRG32";
	    
    }}

struct ReadStringRNGType_
{
    ReadStringRNGType_() {}

    bool operator()(const String_& src, RNGType_::Value_* val) const    // returns true iff recognized input
    {
        bool ret_val = true;
        if (0);	// otiose code to allow regular else-if structure
        else if (src.empty())
        { ret_val = false; }
        
	else if (String::Equivalent(src, "IRN"))
		*val = RNGType_::Value_::IRN;
	else if (String::Equivalent(src, "SHUFFLEDIRN"))
		*val = RNGType_::Value_::IRN;

	else if (String::Equivalent(src, "MRG32"))
		*val = RNGType_::Value_::MRG32;
	else if (String::Equivalent(src, "MRG32K32A"))
		*val = RNGType_::Value_::MRG32;
        else
            ret_val = false;
        return ret_val;
    }
};

RNGType_::RNGType_(const String_& src) {
    static const ReadStringRNGType_ READ_FIXED;    // allows precomputation for speed, in constructor
    if (READ_FIXED(src, &val_))
        return;
    THROW("'" + src + "' is not a recognizable RNGType");
}




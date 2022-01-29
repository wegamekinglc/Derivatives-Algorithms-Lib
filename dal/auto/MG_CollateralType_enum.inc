
bool operator==(const CollateralType_& lhs, const CollateralType_& rhs) {return lhs.val_ == rhs.val_;}
namespace {
    Vector_<CollateralType_>& TheCollateralTypeList() {
        RETURN_STATIC(Vector_<CollateralType_>);
    }
}    // leave local

Vector_<CollateralType_> CollateralTypeListAll() {
   if (TheCollateralTypeList().empty()) {
        TheCollateralTypeList().emplace_back(CollateralType_::Value_::OIS);
        TheCollateralTypeList().emplace_back(CollateralType_::Value_::GC);
        TheCollateralTypeList().emplace_back(CollateralType_::Value_::NONE);
   }
   return TheCollateralTypeList();
}


const char* CollateralType_::String() const {
    switch (val_)
    {
    default:
    case Value_::_NOT_SET:
        return 0;
        case Value_::OIS:
        return "OIS";
	case Value_::GC:
        return "GC";
	case Value_::NONE:
        return "NONE";
	    
    }}

struct ReadStringCollateralType_
{
    ReadStringCollateralType_() {}

    bool operator()(const String_& src, CollateralType_::Value_* val) const    // returns true iff recognized input
    {
        bool ret_val = true;
        if (0);	// otiose code to allow regular else-if structure
        else if (src.empty())
        { ret_val = false; }
        
	else if (String::Equivalent(src, "OIS"))
		*val = CollateralType_::Value_::OIS;

	else if (String::Equivalent(src, "GC"))
		*val = CollateralType_::Value_::GC;

	else if (String::Equivalent(src, "NONE"))
		*val = CollateralType_::Value_::NONE;
        else
            ret_val = false;
        return ret_val;
    }
};

CollateralType_::CollateralType_(const String_& src) {
    static const ReadStringCollateralType_ READ_FIXED;    // allows precomputation for speed, in constructor
    if (READ_FIXED(src, &val_))
        return;
    THROW("'" + src + "' is not a recognizable CollateralType");
}




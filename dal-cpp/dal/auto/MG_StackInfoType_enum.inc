
bool operator==(const StackInfoType_& lhs, const StackInfoType_& rhs) {return lhs.val_ == rhs.val_;}
namespace {
    Vector_<StackInfoType_>& TheStackInfoTypeList() {
        RETURN_STATIC(Vector_<StackInfoType_>);
    }
}    // leave local

Vector_<StackInfoType_> StackInfoTypeListAll() {
   if (TheStackInfoTypeList().empty()) {
        TheStackInfoTypeList().emplace_back(StackInfoType_::Value_::INT);
        TheStackInfoTypeList().emplace_back(StackInfoType_::Value_::DBL);
        TheStackInfoTypeList().emplace_back(StackInfoType_::Value_::CSTR);
        TheStackInfoTypeList().emplace_back(StackInfoType_::Value_::STR);
        TheStackInfoTypeList().emplace_back(StackInfoType_::Value_::DATE);
        TheStackInfoTypeList().emplace_back(StackInfoType_::Value_::DATETIME);
        TheStackInfoTypeList().emplace_back(StackInfoType_::Value_::VOID);
   }
   return TheStackInfoTypeList();
}


const char* StackInfoType_::String() const {
    switch (val_)
    {
    default:
    case Value_::_NOT_SET:
        return 0;
        case Value_::INT:
        return "INT";
    case Value_::DBL:
        return "DBL";
    case Value_::CSTR:
        return "CSTR";
    case Value_::STR:
        return "STR";
    case Value_::DATE:
        return "DATE";
    case Value_::DATETIME:
        return "DATETIME";
    case Value_::VOID:
        return "VOID";
        
    }}

struct ReadStringStackInfoType_
{
    ReadStringStackInfoType_() {}

    bool operator()(const String_& src, StackInfoType_::Value_* val) const    // returns true iff recognized input
    {
        bool ret_val = true;
        if (0);	// otiose code to allow regular else-if structure
        else if (src.empty())
        { ret_val = false; }
        
	else if (String::Equivalent(src, "INT"))
        *val = StackInfoType_::Value_::INT;

	else if (String::Equivalent(src, "DBL"))
        *val = StackInfoType_::Value_::DBL;

	else if (String::Equivalent(src, "CSTR"))
        *val = StackInfoType_::Value_::CSTR;

	else if (String::Equivalent(src, "STR"))
        *val = StackInfoType_::Value_::STR;

	else if (String::Equivalent(src, "DATE"))
        *val = StackInfoType_::Value_::DATE;

	else if (String::Equivalent(src, "DATETIME"))
        *val = StackInfoType_::Value_::DATETIME;

	else if (String::Equivalent(src, "VOID"))
        *val = StackInfoType_::Value_::VOID;
        else
            ret_val = false;
        return ret_val;
    }
};

StackInfoType_::StackInfoType_(const String_& src) {
    static const ReadStringStackInfoType_ READ_FIXED;    // allows precomputation for speed, in constructor
    if (READ_FIXED(src, &val_))
        return;
    THROW("'" + src + "' is not a recognizable StackInfoType");
}




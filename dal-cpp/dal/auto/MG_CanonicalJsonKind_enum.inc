
bool operator==(const CanonicalJsonKind_& lhs, const CanonicalJsonKind_& rhs) {return lhs.val_ == rhs.val_;}
namespace {
    Vector_<CanonicalJsonKind_>& TheCanonicalJsonKindList() {
        RETURN_STATIC(Vector_<CanonicalJsonKind_>);
    }
}    // leave local

Vector_<CanonicalJsonKind_> CanonicalJsonKindListAll() {
   if (TheCanonicalJsonKindList().empty()) {
        TheCanonicalJsonKindList().emplace_back(CanonicalJsonKind_::Value_::NIL);
        TheCanonicalJsonKindList().emplace_back(CanonicalJsonKind_::Value_::BOOLEAN);
        TheCanonicalJsonKindList().emplace_back(CanonicalJsonKind_::Value_::NUMBER);
        TheCanonicalJsonKindList().emplace_back(CanonicalJsonKind_::Value_::STRING);
        TheCanonicalJsonKindList().emplace_back(CanonicalJsonKind_::Value_::ARRAY);
        TheCanonicalJsonKindList().emplace_back(CanonicalJsonKind_::Value_::OBJECT);
   }
   return TheCanonicalJsonKindList();
}


const char* CanonicalJsonKind_::String() const {
    switch (val_)
    {
    default:
    case Value_::_NOT_SET:
        return 0;
        case Value_::NIL:
        return "NIL";
    case Value_::BOOLEAN:
        return "BOOLEAN";
    case Value_::NUMBER:
        return "NUMBER";
    case Value_::STRING:
        return "STRING";
    case Value_::ARRAY:
        return "ARRAY";
    case Value_::OBJECT:
        return "OBJECT";
        
    }}

struct ReadStringCanonicalJsonKind_
{
    ReadStringCanonicalJsonKind_() {}

    bool operator()(const String_& src, CanonicalJsonKind_::Value_* val) const    // returns true iff recognized input
    {
        bool ret_val = true;
        if (0);	// otiose code to allow regular else-if structure
        else if (src.empty())
        { ret_val = false; }
        
	else if (String::Equivalent(src, "NIL"))
        *val = CanonicalJsonKind_::Value_::NIL;

	else if (String::Equivalent(src, "BOOLEAN"))
        *val = CanonicalJsonKind_::Value_::BOOLEAN;

	else if (String::Equivalent(src, "NUMBER"))
        *val = CanonicalJsonKind_::Value_::NUMBER;

	else if (String::Equivalent(src, "STRING"))
        *val = CanonicalJsonKind_::Value_::STRING;

	else if (String::Equivalent(src, "ARRAY"))
        *val = CanonicalJsonKind_::Value_::ARRAY;

	else if (String::Equivalent(src, "OBJECT"))
        *val = CanonicalJsonKind_::Value_::OBJECT;
        else
            ret_val = false;
        return ret_val;
    }
};

CanonicalJsonKind_::CanonicalJsonKind_(const String_& src) {
    static const ReadStringCanonicalJsonKind_ READ_FIXED;    // allows precomputation for speed, in constructor
    if (READ_FIXED(src, &val_))
        return;
    THROW("'" + src + "' is not a recognizable CanonicalJsonKind");
}





bool operator==(const Clearer_& lhs, const Clearer_& rhs) {return lhs.val_ == rhs.val_;}
namespace {
    Vector_<Clearer_>& TheClearerList() {
        RETURN_STATIC(Vector_<Clearer_>);
    }
}    // leave local

Vector_<Clearer_> ClearerListAll() {
   if (TheClearerList().empty()) {
        TheClearerList().emplace_back(Clearer_::Value_::CME);
        TheClearerList().emplace_back(Clearer_::Value_::LCH);
   }
   return TheClearerList();
}


const char* Clearer_::String() const {
    switch (val_)
    {
    default:
    case Value_::_NOT_SET:
        return 0;
        case Value_::CME:
        return "CME";
    case Value_::LCH:
        return "LCH";
        
    }}

struct ReadStringClearer_
{
    ReadStringClearer_() {}

    bool operator()(const String_& src, Clearer_::Value_* val) const    // returns true iff recognized input
    {
        bool ret_val = true;
        if (0);	// otiose code to allow regular else-if structure
        else if (src.empty())
        { ret_val = false; }
        
	else if (String::Equivalent(src, "CME"))
        *val = Clearer_::Value_::CME;

	else if (String::Equivalent(src, "LCH"))
        *val = Clearer_::Value_::LCH;
        else
            ret_val = false;
        return ret_val;
    }
};

Clearer_::Clearer_(const String_& src) {
    static const ReadStringClearer_ READ_FIXED;    // allows precomputation for speed, in constructor
    if (READ_FIXED(src, &val_))
        return;
    THROW("'" + src + "' is not a recognizable Clearer");
}




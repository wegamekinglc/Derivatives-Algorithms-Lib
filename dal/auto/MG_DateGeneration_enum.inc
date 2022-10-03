
bool operator==(const DateGeneration_& lhs, const DateGeneration_& rhs) {return lhs.val_ == rhs.val_;}
namespace {
    Vector_<DateGeneration_>& TheDateGenerationList() {
        RETURN_STATIC(Vector_<DateGeneration_>);
    }
}    // leave local

Vector_<DateGeneration_> DateGenerationListAll() {
   if (TheDateGenerationList().empty()) {
        TheDateGenerationList().emplace_back(DateGeneration_::Value_::Backward);
        TheDateGenerationList().emplace_back(DateGeneration_::Value_::Forward);
   }
   return TheDateGenerationList();
}


const char* DateGeneration_::String() const {
    switch (val_)
    {
    default:
    case Value_::_NOT_SET:
        return 0;
        case Value_::Backward:
        return "Backward";
    case Value_::Forward:
        return "Forward";
        
    }}

struct ReadStringDateGeneration_
{
    ReadStringDateGeneration_() {}

    bool operator()(const String_& src, DateGeneration_::Value_* val) const    // returns true iff recognized input
    {
        bool ret_val = true;
        if (0);	// otiose code to allow regular else-if structure
        else if (src.empty())
        { ret_val = false; }
        
	else if (String::Equivalent(src, "BACKWARD"))
        *val = DateGeneration_::Value_::Backward;

	else if (String::Equivalent(src, "FORWARD"))
        *val = DateGeneration_::Value_::Forward;
        else
            ret_val = false;
        return ret_val;
    }
};

DateGeneration_::DateGeneration_(const String_& src) {
    static const ReadStringDateGeneration_ READ_FIXED;    // allows precomputation for speed, in constructor
    if (READ_FIXED(src, &val_))
        return;
    THROW("'" + src + "' is not a recognizable DateGeneration");
}




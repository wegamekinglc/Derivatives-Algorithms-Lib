
bool operator==(const EarlyExerciseStyle_& lhs, const EarlyExerciseStyle_& rhs) {return lhs.val_ == rhs.val_;}
namespace {
    Vector_<EarlyExerciseStyle_>& TheEarlyExerciseStyleList() {
        RETURN_STATIC(Vector_<EarlyExerciseStyle_>);
    }
}    // leave local

Vector_<EarlyExerciseStyle_> EarlyExerciseStyleListAll() {
   if (TheEarlyExerciseStyleList().empty()) {
        TheEarlyExerciseStyleList().emplace_back(EarlyExerciseStyle_::Value_::BERMUDAN);
        TheEarlyExerciseStyleList().emplace_back(EarlyExerciseStyle_::Value_::AMERICAN);
   }
   return TheEarlyExerciseStyleList();
}


const char* EarlyExerciseStyle_::String() const {
    switch (val_)
    {
    default:
    case Value_::_NOT_SET:
        return 0;
        case Value_::BERMUDAN:
        return "BERMUDAN";
    case Value_::AMERICAN:
        return "AMERICAN";
        
    }}

struct ReadStringEarlyExerciseStyle_
{
    ReadStringEarlyExerciseStyle_() {}

    bool operator()(const String_& src, EarlyExerciseStyle_::Value_* val) const    // returns true iff recognized input
    {
        bool ret_val = true;
        if (0);	// otiose code to allow regular else-if structure
        else if (src.empty())
        { ret_val = false; }
        
	else if (String::Equivalent(src, "BERMUDAN"))
        *val = EarlyExerciseStyle_::Value_::BERMUDAN;

	else if (String::Equivalent(src, "AMERICAN"))
        *val = EarlyExerciseStyle_::Value_::AMERICAN;
        else
            ret_val = false;
        return ret_val;
    }
};

EarlyExerciseStyle_::EarlyExerciseStyle_(const String_& src) {
    static const ReadStringEarlyExerciseStyle_ READ_FIXED;    // allows precomputation for speed, in constructor
    if (READ_FIXED(src, &val_))
        return;
    THROW("'" + src + "' is not a recognizable EarlyExerciseStyle");
}




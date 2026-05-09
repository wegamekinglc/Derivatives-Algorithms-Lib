
bool operator==(const CurveSolveMode_& lhs, const CurveSolveMode_& rhs) {return lhs.val_ == rhs.val_;}
namespace {
    Vector_<CurveSolveMode_>& TheCurveSolveModeList() {
        RETURN_STATIC(Vector_<CurveSolveMode_>);
    }
}    // leave local

Vector_<CurveSolveMode_> CurveSolveModeListAll() {
   if (TheCurveSolveModeList().empty()) {
        TheCurveSolveModeList().emplace_back(CurveSolveMode_::Value_::EXACT);
        TheCurveSolveModeList().emplace_back(CurveSolveMode_::Value_::APPROXIMATE);
   }
   return TheCurveSolveModeList();
}


const char* CurveSolveMode_::String() const {
    switch (val_)
    {
    default:
    case Value_::_NOT_SET:
        return 0;
        case Value_::EXACT:
        return "EXACT";
    case Value_::APPROXIMATE:
        return "APPROXIMATE";
        
    }}

struct ReadStringCurveSolveMode_
{
    ReadStringCurveSolveMode_() {}

    bool operator()(const String_& src, CurveSolveMode_::Value_* val) const    // returns true iff recognized input
    {
        bool ret_val = true;
        if (0);	// otiose code to allow regular else-if structure
        else if (src.empty())
        { ret_val = false; }
        
	else if (String::Equivalent(src, "EXACT"))
        *val = CurveSolveMode_::Value_::EXACT;

	else if (String::Equivalent(src, "APPROXIMATE"))
        *val = CurveSolveMode_::Value_::APPROXIMATE;
        else
            ret_val = false;
        return ret_val;
    }
};

CurveSolveMode_::CurveSolveMode_(const String_& src) {
    static const ReadStringCurveSolveMode_ READ_FIXED;    // allows precomputation for speed, in constructor
    if (READ_FIXED(src, &val_))
        return;
    THROW("'" + src + "' is not a recognizable CurveSolveMode");
}





bool operator==(const CurveKnotCandidateDisposition_& lhs, const CurveKnotCandidateDisposition_& rhs) {return lhs.val_ == rhs.val_;}
namespace {
    Vector_<CurveKnotCandidateDisposition_>& TheCurveKnotCandidateDispositionList() {
        RETURN_STATIC(Vector_<CurveKnotCandidateDisposition_>);
    }
}    // leave local

Vector_<CurveKnotCandidateDisposition_> CurveKnotCandidateDispositionListAll() {
   if (TheCurveKnotCandidateDispositionList().empty()) {
        TheCurveKnotCandidateDispositionList().emplace_back(CurveKnotCandidateDisposition_::Value_::ADDED);
        TheCurveKnotCandidateDispositionList().emplace_back(CurveKnotCandidateDisposition_::Value_::DUPLICATE);
        TheCurveKnotCandidateDispositionList().emplace_back(CurveKnotCandidateDisposition_::Value_::FILTERED_NOT_AFTER_TODAY);
   }
   return TheCurveKnotCandidateDispositionList();
}


const char* CurveKnotCandidateDisposition_::String() const {
    switch (val_)
    {
    default:
    case Value_::_NOT_SET:
        return 0;
        case Value_::ADDED:
        return "ADDED";
    case Value_::DUPLICATE:
        return "DUPLICATE";
    case Value_::FILTERED_NOT_AFTER_TODAY:
        return "FILTERED_NOT_AFTER_TODAY";
        
    }}

struct ReadStringCurveKnotCandidateDisposition_
{
    ReadStringCurveKnotCandidateDisposition_() {}

    bool operator()(const String_& src, CurveKnotCandidateDisposition_::Value_* val) const    // returns true iff recognized input
    {
        bool ret_val = true;
        if (0);	// otiose code to allow regular else-if structure
        else if (src.empty())
        { ret_val = false; }
        
	else if (String::Equivalent(src, "ADDED"))
        *val = CurveKnotCandidateDisposition_::Value_::ADDED;

	else if (String::Equivalent(src, "DUPLICATE"))
        *val = CurveKnotCandidateDisposition_::Value_::DUPLICATE;

	else if (String::Equivalent(src, "FILTEREDNOTAFTERTODAY"))
        *val = CurveKnotCandidateDisposition_::Value_::FILTERED_NOT_AFTER_TODAY;
        else
            ret_val = false;
        return ret_val;
    }
};

CurveKnotCandidateDisposition_::CurveKnotCandidateDisposition_(const String_& src) {
    static const ReadStringCurveKnotCandidateDisposition_ READ_FIXED;    // allows precomputation for speed, in constructor
    if (READ_FIXED(src, &val_))
        return;
    THROW("'" + src + "' is not a recognizable CurveKnotCandidateDisposition");
}




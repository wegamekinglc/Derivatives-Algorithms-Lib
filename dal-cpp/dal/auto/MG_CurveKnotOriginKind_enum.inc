
bool operator==(const CurveKnotOriginKind_& lhs, const CurveKnotOriginKind_& rhs) {return lhs.val_ == rhs.val_;}
namespace {
    Vector_<CurveKnotOriginKind_>& TheCurveKnotOriginKindList() {
        RETURN_STATIC(Vector_<CurveKnotOriginKind_>);
    }
}    // leave local

Vector_<CurveKnotOriginKind_> CurveKnotOriginKindListAll() {
   if (TheCurveKnotOriginKindList().empty()) {
        TheCurveKnotOriginKindList().emplace_back(CurveKnotOriginKind_::Value_::INPUT);
        TheCurveKnotOriginKindList().emplace_back(CurveKnotOriginKind_::Value_::INSTRUMENT_START);
        TheCurveKnotOriginKindList().emplace_back(CurveKnotOriginKind_::Value_::INSTRUMENT_END);
        TheCurveKnotOriginKindList().emplace_back(CurveKnotOriginKind_::Value_::SYNTHETIC_ANCHOR);
   }
   return TheCurveKnotOriginKindList();
}


const char* CurveKnotOriginKind_::String() const {
    switch (val_)
    {
    default:
    case Value_::_NOT_SET:
        return 0;
        case Value_::INPUT:
        return "INPUT";
    case Value_::INSTRUMENT_START:
        return "INSTRUMENT_START";
    case Value_::INSTRUMENT_END:
        return "INSTRUMENT_END";
    case Value_::SYNTHETIC_ANCHOR:
        return "SYNTHETIC_ANCHOR";
        
    }}

struct ReadStringCurveKnotOriginKind_
{
    ReadStringCurveKnotOriginKind_() {}

    bool operator()(const String_& src, CurveKnotOriginKind_::Value_* val) const    // returns true iff recognized input
    {
        bool ret_val = true;
        if (0);	// otiose code to allow regular else-if structure
        else if (src.empty())
        { ret_val = false; }
        
	else if (String::Equivalent(src, "INPUT"))
        *val = CurveKnotOriginKind_::Value_::INPUT;

	else if (String::Equivalent(src, "INSTRUMENTSTART"))
        *val = CurveKnotOriginKind_::Value_::INSTRUMENT_START;

	else if (String::Equivalent(src, "INSTRUMENTEND"))
        *val = CurveKnotOriginKind_::Value_::INSTRUMENT_END;

	else if (String::Equivalent(src, "SYNTHETICANCHOR"))
        *val = CurveKnotOriginKind_::Value_::SYNTHETIC_ANCHOR;
        else
            ret_val = false;
        return ret_val;
    }
};

CurveKnotOriginKind_::CurveKnotOriginKind_(const String_& src) {
    static const ReadStringCurveKnotOriginKind_ READ_FIXED;    // allows precomputation for speed, in constructor
    if (READ_FIXED(src, &val_))
        return;
    THROW("'" + src + "' is not a recognizable CurveKnotOriginKind");
}




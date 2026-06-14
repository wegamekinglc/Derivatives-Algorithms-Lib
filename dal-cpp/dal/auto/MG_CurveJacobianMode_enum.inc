
bool operator==(const CurveJacobianMode_& lhs, const CurveJacobianMode_& rhs) {return lhs.val_ == rhs.val_;}
namespace {
    Vector_<CurveJacobianMode_>& TheCurveJacobianModeList() {
        RETURN_STATIC(Vector_<CurveJacobianMode_>);
    }
}    // leave local

Vector_<CurveJacobianMode_> CurveJacobianModeListAll() {
   if (TheCurveJacobianModeList().empty()) {
        TheCurveJacobianModeList().emplace_back(CurveJacobianMode_::Value_::BUMPED);
        TheCurveJacobianModeList().emplace_back(CurveJacobianMode_::Value_::ANALYTIC_LOG_DISCOUNT);
        TheCurveJacobianModeList().emplace_back(CurveJacobianMode_::Value_::AAD_TAPE);
   }
   return TheCurveJacobianModeList();
}


const char* CurveJacobianMode_::String() const {
    switch (val_)
    {
    default:
    case Value_::_NOT_SET:
        return 0;
        case Value_::BUMPED:
        return "BUMPED";
    case Value_::ANALYTIC_LOG_DISCOUNT:
        return "ANALYTIC_LOG_DISCOUNT";
    case Value_::AAD_TAPE:
        return "AAD_TAPE";
        
    }}

struct ReadStringCurveJacobianMode_
{
    ReadStringCurveJacobianMode_() {}

    bool operator()(const String_& src, CurveJacobianMode_::Value_* val) const    // returns true iff recognized input
    {
        bool ret_val = true;
        if (0);	// otiose code to allow regular else-if structure
        else if (src.empty())
        { ret_val = false; }
        
	else if (String::Equivalent(src, "BUMPED"))
        *val = CurveJacobianMode_::Value_::BUMPED;

	else if (String::Equivalent(src, "ANALYTICLOGDISCOUNT"))
        *val = CurveJacobianMode_::Value_::ANALYTIC_LOG_DISCOUNT;

	else if (String::Equivalent(src, "AADTAPE"))
        *val = CurveJacobianMode_::Value_::AAD_TAPE;
        else
            ret_val = false;
        return ret_val;
    }
};

CurveJacobianMode_::CurveJacobianMode_(const String_& src) {
    static const ReadStringCurveJacobianMode_ READ_FIXED;    // allows precomputation for speed, in constructor
    if (READ_FIXED(src, &val_))
        return;
    THROW("'" + src + "' is not a recognizable CurveJacobianMode");
}




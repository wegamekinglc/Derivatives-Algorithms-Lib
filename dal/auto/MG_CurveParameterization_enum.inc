
bool operator==(const CurveParameterization_& lhs, const CurveParameterization_& rhs) {return lhs.val_ == rhs.val_;}
namespace {
    Vector_<CurveParameterization_>& TheCurveParameterizationList() {
        RETURN_STATIC(Vector_<CurveParameterization_>);
    }
}    // leave local

Vector_<CurveParameterization_> CurveParameterizationListAll() {
   if (TheCurveParameterizationList().empty()) {
        TheCurveParameterizationList().emplace_back(CurveParameterization_::Value_::PIECEWISE_LINEAR_FWD);
        TheCurveParameterizationList().emplace_back(CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD);
        TheCurveParameterizationList().emplace_back(CurveParameterization_::Value_::ZERO_RATE);
        TheCurveParameterizationList().emplace_back(CurveParameterization_::Value_::LOG_DISCOUNT);
   }
   return TheCurveParameterizationList();
}


const char* CurveParameterization_::String() const {
    switch (val_)
    {
    default:
    case Value_::_NOT_SET:
        return 0;
        case Value_::PIECEWISE_LINEAR_FWD:
        return "PIECEWISE_LINEAR_FWD";
    case Value_::PIECEWISE_CONSTANT_FWD:
        return "PIECEWISE_CONSTANT_FWD";
    case Value_::ZERO_RATE:
        return "ZERO_RATE";
    case Value_::LOG_DISCOUNT:
        return "LOG_DISCOUNT";
        
    }}

struct ReadStringCurveParameterization_
{
    ReadStringCurveParameterization_() {}

    bool operator()(const String_& src, CurveParameterization_::Value_* val) const    // returns true iff recognized input
    {
        bool ret_val = true;
        if (0);	// otiose code to allow regular else-if structure
        else if (src.empty())
        { ret_val = false; }
        
	else if (String::Equivalent(src, "PIECEWISELINEARFWD"))
        *val = CurveParameterization_::Value_::PIECEWISE_LINEAR_FWD;

	else if (String::Equivalent(src, "PIECEWISECONSTANTFWD"))
        *val = CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD;

	else if (String::Equivalent(src, "ZERORATE"))
        *val = CurveParameterization_::Value_::ZERO_RATE;

	else if (String::Equivalent(src, "LOGDISCOUNT"))
        *val = CurveParameterization_::Value_::LOG_DISCOUNT;
        else
            ret_val = false;
        return ret_val;
    }
};

CurveParameterization_::CurveParameterization_(const String_& src) {
    static const ReadStringCurveParameterization_ READ_FIXED;    // allows precomputation for speed, in constructor
    if (READ_FIXED(src, &val_))
        return;
    THROW("'" + src + "' is not a recognizable CurveParameterization");
}




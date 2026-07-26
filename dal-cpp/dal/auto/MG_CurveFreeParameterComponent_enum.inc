
bool operator==(const CurveFreeParameterComponent_& lhs, const CurveFreeParameterComponent_& rhs) {return lhs.val_ == rhs.val_;}
namespace {
    Vector_<CurveFreeParameterComponent_>& TheCurveFreeParameterComponentList() {
        RETURN_STATIC(Vector_<CurveFreeParameterComponent_>);
    }
}    // leave local

Vector_<CurveFreeParameterComponent_> CurveFreeParameterComponentListAll() {
   if (TheCurveFreeParameterComponentList().empty()) {
        TheCurveFreeParameterComponentList().emplace_back(CurveFreeParameterComponent_::Value_::RIGHT_FORWARD);
        TheCurveFreeParameterComponentList().emplace_back(CurveFreeParameterComponent_::Value_::LEFT_FORWARD);
        TheCurveFreeParameterComponentList().emplace_back(CurveFreeParameterComponent_::Value_::ZERO_RATE);
        TheCurveFreeParameterComponentList().emplace_back(CurveFreeParameterComponent_::Value_::LOG_DISCOUNT_FACTOR);
   }
   return TheCurveFreeParameterComponentList();
}


const char* CurveFreeParameterComponent_::String() const {
    switch (val_)
    {
    default:
    case Value_::_NOT_SET:
        return 0;
        case Value_::RIGHT_FORWARD:
        return "RIGHT_FORWARD";
    case Value_::LEFT_FORWARD:
        return "LEFT_FORWARD";
    case Value_::ZERO_RATE:
        return "ZERO_RATE";
    case Value_::LOG_DISCOUNT_FACTOR:
        return "LOG_DISCOUNT_FACTOR";

    }}

struct ReadStringCurveFreeParameterComponent_
{
    ReadStringCurveFreeParameterComponent_() {}

    bool operator()(const String_& src, CurveFreeParameterComponent_::Value_* val) const    // returns true iff recognized input
    {
        bool ret_val = true;
        if (0);	// otiose code to allow regular else-if structure
        else if (src.empty())
        { ret_val = false; }

	else if (String::Equivalent(src, "RIGHTFORWARD"))
        *val = CurveFreeParameterComponent_::Value_::RIGHT_FORWARD;

	else if (String::Equivalent(src, "LEFTFORWARD"))
        *val = CurveFreeParameterComponent_::Value_::LEFT_FORWARD;

	else if (String::Equivalent(src, "ZERORATE"))
        *val = CurveFreeParameterComponent_::Value_::ZERO_RATE;

	else if (String::Equivalent(src, "LOGDISCOUNTFACTOR"))
        *val = CurveFreeParameterComponent_::Value_::LOG_DISCOUNT_FACTOR;
        else
            ret_val = false;
        return ret_val;
    }
};

CurveFreeParameterComponent_::CurveFreeParameterComponent_(const String_& src) {
    static const ReadStringCurveFreeParameterComponent_ READ_FIXED;    // allows precomputation for speed, in constructor
    if (READ_FIXED(src, &val_))
        return;
    THROW("'" + src + "' is not a recognizable CurveFreeParameterComponent");
}

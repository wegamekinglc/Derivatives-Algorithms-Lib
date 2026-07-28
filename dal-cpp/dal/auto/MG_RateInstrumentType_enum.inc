
bool operator==(const RateInstrumentType_& lhs, const RateInstrumentType_& rhs) {return lhs.val_ == rhs.val_;}
namespace {
    Vector_<RateInstrumentType_>& TheRateInstrumentTypeList() {
        RETURN_STATIC(Vector_<RateInstrumentType_>);
    }
}    // leave local

Vector_<RateInstrumentType_> RateInstrumentTypeListAll() {
   if (TheRateInstrumentTypeList().empty()) {
        TheRateInstrumentTypeList().emplace_back(RateInstrumentType_::Value_::DEPOSIT);
        TheRateInstrumentTypeList().emplace_back(RateInstrumentType_::Value_::FRA);
        TheRateInstrumentTypeList().emplace_back(RateInstrumentType_::Value_::FUTURE);
        TheRateInstrumentTypeList().emplace_back(RateInstrumentType_::Value_::OIS);
        TheRateInstrumentTypeList().emplace_back(RateInstrumentType_::Value_::IRS);
        TheRateInstrumentTypeList().emplace_back(RateInstrumentType_::Value_::BASIS_SWAP);
        TheRateInstrumentTypeList().emplace_back(RateInstrumentType_::Value_::XCCY);
   }
   return TheRateInstrumentTypeList();
}


const char* RateInstrumentType_::String() const {
    switch (val_)
    {
    default:
    case Value_::_NOT_SET:
        return 0;
        case Value_::DEPOSIT:
        return "DEPOSIT";
    case Value_::FRA:
        return "FRA";
    case Value_::FUTURE:
        return "FUTURE";
    case Value_::OIS:
        return "OIS";
    case Value_::IRS:
        return "IRS";
    case Value_::BASIS_SWAP:
        return "BASIS_SWAP";
    case Value_::XCCY:
        return "XCCY";

    }}

struct ReadStringRateInstrumentType_
{
    ReadStringRateInstrumentType_() {}

    bool operator()(const String_& src, RateInstrumentType_::Value_* val) const    // returns true iff recognized input
    {
        bool ret_val = true;
        if (0);	// otiose code to allow regular else-if structure
        else if (src.empty())
        { ret_val = false; }

	else if (String::Equivalent(src, "DEPOSIT"))
        *val = RateInstrumentType_::Value_::DEPOSIT;

	else if (String::Equivalent(src, "FRA"))
        *val = RateInstrumentType_::Value_::FRA;

	else if (String::Equivalent(src, "FUTURE"))
        *val = RateInstrumentType_::Value_::FUTURE;

	else if (String::Equivalent(src, "OIS"))
        *val = RateInstrumentType_::Value_::OIS;

	else if (String::Equivalent(src, "IRS"))
        *val = RateInstrumentType_::Value_::IRS;

	else if (String::Equivalent(src, "BASISSWAP"))
        *val = RateInstrumentType_::Value_::BASIS_SWAP;

	else if (String::Equivalent(src, "XCCY"))
        *val = RateInstrumentType_::Value_::XCCY;
        else
            ret_val = false;
        return ret_val;
    }
};

RateInstrumentType_::RateInstrumentType_(const String_& src) {
    static const ReadStringRateInstrumentType_ READ_FIXED;    // allows precomputation for speed, in constructor
    if (READ_FIXED(src, &val_))
        return;
    THROW("'" + src + "' is not a recognizable RateInstrumentType");
}

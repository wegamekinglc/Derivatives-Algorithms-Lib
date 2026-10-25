
bool operator==(const XccyObservationKind_& lhs, const XccyObservationKind_& rhs) {return lhs.val_ == rhs.val_;}
namespace {
    Vector_<XccyObservationKind_>& TheXccyObservationKindList() {
        RETURN_STATIC(Vector_<XccyObservationKind_>);
    }
}    // leave local

Vector_<XccyObservationKind_> XccyObservationKindListAll() {
   if (TheXccyObservationKindList().empty()) {
        TheXccyObservationKindList().emplace_back(XccyObservationKind_::Value_::RateFixing);
        TheXccyObservationKindList().emplace_back(XccyObservationKind_::Value_::ResetFxFixing);
        TheXccyObservationKindList().emplace_back(XccyObservationKind_::Value_::PaymentFxSpot);
   }
   return TheXccyObservationKindList();
}


const char* XccyObservationKind_::String() const {
    switch (val_)
    {
    default:
    case Value_::_NOT_SET:
        return 0;
        case Value_::RateFixing:
        return "RateFixing";
    case Value_::ResetFxFixing:
        return "ResetFxFixing";
    case Value_::PaymentFxSpot:
        return "PaymentFxSpot";
        
    }}

struct ReadStringXccyObservationKind_
{
    ReadStringXccyObservationKind_() {}

    bool operator()(const String_& src, XccyObservationKind_::Value_* val) const    // returns true iff recognized input
    {
        bool ret_val = true;
        if (0);	// otiose code to allow regular else-if structure
        else if (src.empty())
        { ret_val = false; }
        
	else if (String::Equivalent(src, "RATEFIXING"))
        *val = XccyObservationKind_::Value_::RateFixing;

	else if (String::Equivalent(src, "RESETFXFIXING"))
        *val = XccyObservationKind_::Value_::ResetFxFixing;

	else if (String::Equivalent(src, "PAYMENTFXSPOT"))
        *val = XccyObservationKind_::Value_::PaymentFxSpot;
        else
            ret_val = false;
        return ret_val;
    }
};

XccyObservationKind_::XccyObservationKind_(const String_& src) {
    static const ReadStringXccyObservationKind_ READ_FIXED;    // allows precomputation for speed, in constructor
    if (READ_FIXED(src, &val_))
        return;
    THROW("'" + src + "' is not a recognizable XccyObservationKind");
}




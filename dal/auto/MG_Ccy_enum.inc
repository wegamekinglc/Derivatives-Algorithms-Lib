
bool operator==(const Ccy_& lhs, const Ccy_& rhs) {return lhs.val_ == rhs.val_;}
namespace {
    Vector_<Ccy_>& TheCcyList() {
        RETURN_STATIC(Vector_<Ccy_>);
    }
}    // leave local

Vector_<Ccy_> CcyListAll() {
   if (TheCcyList().empty()) {
        TheCcyList().emplace_back(Ccy_::Value_::USD);
        TheCcyList().emplace_back(Ccy_::Value_::EUR);
        TheCcyList().emplace_back(Ccy_::Value_::GBP);
        TheCcyList().emplace_back(Ccy_::Value_::JPY);
        TheCcyList().emplace_back(Ccy_::Value_::AUD);
        TheCcyList().emplace_back(Ccy_::Value_::CHF);
        TheCcyList().emplace_back(Ccy_::Value_::CAD);
        TheCcyList().emplace_back(Ccy_::Value_::CNY);
   }
   return TheCcyList();
}


const char* Ccy_::String() const {
    switch (val_)
    {
    default:
    case Value_::_NOT_SET:
        return 0;
        case Value_::USD:
        return "USD";
	case Value_::EUR:
        return "EUR";
	case Value_::GBP:
        return "GBP";
	case Value_::JPY:
        return "JPY";
	case Value_::AUD:
        return "AUD";
	case Value_::CHF:
        return "CHF";
	case Value_::CAD:
        return "CAD";
	case Value_::CNY:
        return "CNY";
	    
    }}

struct ReadStringCcy_
{
    ReadStringCcy_() {}

    bool operator()(const String_& src, Ccy_::Value_* val) const    // returns true iff recognized input
    {
        bool ret_val = true;
        if (0);	// otiose code to allow regular else-if structure
        else if (src.empty())
        { ret_val = false; }
        
	else if (String::Equivalent(src, "USD"))
		*val = Ccy_::Value_::USD;

	else if (String::Equivalent(src, "EUR"))
		*val = Ccy_::Value_::EUR;

	else if (String::Equivalent(src, "GBP"))
		*val = Ccy_::Value_::GBP;

	else if (String::Equivalent(src, "JPY"))
		*val = Ccy_::Value_::JPY;

	else if (String::Equivalent(src, "AUD"))
		*val = Ccy_::Value_::AUD;

	else if (String::Equivalent(src, "CHF"))
		*val = Ccy_::Value_::CHF;

	else if (String::Equivalent(src, "CAD"))
		*val = Ccy_::Value_::CAD;

	else if (String::Equivalent(src, "CNY"))
		*val = Ccy_::Value_::CNY;
        else
            ret_val = false;
        return ret_val;
    }
};

Ccy_::Ccy_(const String_& src) {
    static const ReadStringCcy_ READ_FIXED;    // allows precomputation for speed, in constructor
    if (READ_FIXED(src, &val_))
        return;
    THROW("'" + src + "' is not a recognizable Ccy");
}




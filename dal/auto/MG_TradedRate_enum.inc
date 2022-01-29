
bool operator==(const TradedRate_& lhs, const TradedRate_& rhs) {return lhs.val_ == rhs.val_;}
namespace {
    Vector_<TradedRate_>& TheTradedRateList() {
        RETURN_STATIC(Vector_<TradedRate_>);
    }
}    // leave local

Vector_<TradedRate_> TradedRateListAll() {
   if (TheTradedRateList().empty()) {
        TheTradedRateList().emplace_back(TradedRate_::Value_::LIBOR_3M_CME);
        TheTradedRateList().emplace_back(TradedRate_::Value_::LIBOR_3M_LCH);
        TheTradedRateList().emplace_back(TradedRate_::Value_::LIBOR_3M_FUT);
        TheTradedRateList().emplace_back(TradedRate_::Value_::LIBOR_6M_CME);
        TheTradedRateList().emplace_back(TradedRate_::Value_::LIBOR_6M_LCH);
   }
   return TheTradedRateList();
}


const char* TradedRate_::String() const {
    switch (val_)
    {
    default:
    case Value_::_NOT_SET:
        return 0;
        case Value_::LIBOR_3M_CME:
        return "LIBOR_3M_CME";
	case Value_::LIBOR_3M_LCH:
        return "LIBOR_3M_LCH";
	case Value_::LIBOR_3M_FUT:
        return "LIBOR_3M_FUT";
	case Value_::LIBOR_6M_CME:
        return "LIBOR_6M_CME";
	case Value_::LIBOR_6M_LCH:
        return "LIBOR_6M_LCH";
	    
    }}

struct ReadStringTradedRate_
{
    ReadStringTradedRate_() {}

    bool operator()(const String_& src, TradedRate_::Value_* val) const    // returns true iff recognized input
    {
        bool ret_val = true;
        if (0);	// otiose code to allow regular else-if structure
        else if (src.empty())
        { ret_val = false; }
        
	else if (String::Equivalent(src, "LIBOR3MCME"))
		*val = TradedRate_::Value_::LIBOR_3M_CME;

	else if (String::Equivalent(src, "LIBOR3MLCH"))
		*val = TradedRate_::Value_::LIBOR_3M_LCH;

	else if (String::Equivalent(src, "LIBOR3MFUT"))
		*val = TradedRate_::Value_::LIBOR_3M_FUT;

	else if (String::Equivalent(src, "LIBOR6MCME"))
		*val = TradedRate_::Value_::LIBOR_6M_CME;

	else if (String::Equivalent(src, "LIBOR6MLCH"))
		*val = TradedRate_::Value_::LIBOR_6M_LCH;
        else
            ret_val = false;
        return ret_val;
    }
};

TradedRate_::TradedRate_(const String_& src) {
    static const ReadStringTradedRate_ READ_FIXED;    // allows precomputation for speed, in constructor
    if (READ_FIXED(src, &val_))
        return;
    THROW("'" + src + "' is not a recognizable TradedRate");
}




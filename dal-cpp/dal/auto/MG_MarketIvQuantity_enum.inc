
bool operator==(const MarketIvQuantity_& lhs, const MarketIvQuantity_& rhs) {return lhs.val_ == rhs.val_;}
namespace {
    Vector_<MarketIvQuantity_>& TheMarketIvQuantityList() {
        RETURN_STATIC(Vector_<MarketIvQuantity_>);
    }
}    // leave local

Vector_<MarketIvQuantity_> MarketIvQuantityListAll() {
   if (TheMarketIvQuantityList().empty()) {
        TheMarketIvQuantityList().emplace_back(MarketIvQuantity_::Value_::TOTAL_VARIANCE);
        TheMarketIvQuantityList().emplace_back(MarketIvQuantity_::Value_::IMPLIED_VOL);
        TheMarketIvQuantityList().emplace_back(MarketIvQuantity_::Value_::CALL_PRICE);
        TheMarketIvQuantityList().emplace_back(MarketIvQuantity_::Value_::PUT_PRICE);
        TheMarketIvQuantityList().emplace_back(MarketIvQuantity_::Value_::LOCAL_VOL);
   }
   return TheMarketIvQuantityList();
}


const char* MarketIvQuantity_::String() const {
    switch (val_)
    {
    default:
    case Value_::_NOT_SET:
        return 0;
        case Value_::TOTAL_VARIANCE:
        return "TOTAL_VARIANCE";
    case Value_::IMPLIED_VOL:
        return "IMPLIED_VOL";
    case Value_::CALL_PRICE:
        return "CALL_PRICE";
    case Value_::PUT_PRICE:
        return "PUT_PRICE";
    case Value_::LOCAL_VOL:
        return "LOCAL_VOL";
        
    }}

struct ReadStringMarketIvQuantity_
{
    ReadStringMarketIvQuantity_() {}

    bool operator()(const String_& src, MarketIvQuantity_::Value_* val) const    // returns true iff recognized input
    {
        bool ret_val = true;
        if (0);	// otiose code to allow regular else-if structure
        else if (src.empty())
        { ret_val = false; }
        
	else if (String::Equivalent(src, "TOTALVARIANCE"))
        *val = MarketIvQuantity_::Value_::TOTAL_VARIANCE;

	else if (String::Equivalent(src, "IMPLIEDVOL"))
        *val = MarketIvQuantity_::Value_::IMPLIED_VOL;

	else if (String::Equivalent(src, "CALLPRICE"))
        *val = MarketIvQuantity_::Value_::CALL_PRICE;

	else if (String::Equivalent(src, "PUTPRICE"))
        *val = MarketIvQuantity_::Value_::PUT_PRICE;

	else if (String::Equivalent(src, "LOCALVOL"))
        *val = MarketIvQuantity_::Value_::LOCAL_VOL;
        else
            ret_val = false;
        return ret_val;
    }
};

MarketIvQuantity_::MarketIvQuantity_(const String_& src) {
    static const ReadStringMarketIvQuantity_ READ_FIXED;    // allows precomputation for speed, in constructor
    if (READ_FIXED(src, &val_))
        return;
    THROW("'" + src + "' is not a recognizable MarketIvQuantity");
}




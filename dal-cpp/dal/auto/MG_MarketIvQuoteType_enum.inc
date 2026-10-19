
bool operator==(const MarketIvQuoteType_& lhs, const MarketIvQuoteType_& rhs) {return lhs.val_ == rhs.val_;}
namespace {
    Vector_<MarketIvQuoteType_>& TheMarketIvQuoteTypeList() {
        RETURN_STATIC(Vector_<MarketIvQuoteType_>);
    }
}    // leave local

Vector_<MarketIvQuoteType_> MarketIvQuoteTypeListAll() {
   if (TheMarketIvQuoteTypeList().empty()) {
        TheMarketIvQuoteTypeList().emplace_back(MarketIvQuoteType_::Value_::PRICE);
        TheMarketIvQuoteTypeList().emplace_back(MarketIvQuoteType_::Value_::DECIMAL_IV);
   }
   return TheMarketIvQuoteTypeList();
}


const char* MarketIvQuoteType_::String() const {
    switch (val_)
    {
    default:
    case Value_::_NOT_SET:
        return 0;
        case Value_::PRICE:
        return "PRICE";
    case Value_::DECIMAL_IV:
        return "DECIMAL_IV";
        
    }}

struct ReadStringMarketIvQuoteType_
{
    ReadStringMarketIvQuoteType_() {}

    bool operator()(const String_& src, MarketIvQuoteType_::Value_* val) const    // returns true iff recognized input
    {
        bool ret_val = true;
        if (0);	// otiose code to allow regular else-if structure
        else if (src.empty())
        { ret_val = false; }
        
	else if (String::Equivalent(src, "PRICE"))
        *val = MarketIvQuoteType_::Value_::PRICE;

	else if (String::Equivalent(src, "DECIMALIV"))
        *val = MarketIvQuoteType_::Value_::DECIMAL_IV;
        else
            ret_val = false;
        return ret_val;
    }
};

MarketIvQuoteType_::MarketIvQuoteType_(const String_& src) {
    static const ReadStringMarketIvQuoteType_ READ_FIXED;    // allows precomputation for speed, in constructor
    if (READ_FIXED(src, &val_))
        return;
    THROW("'" + src + "' is not a recognizable MarketIvQuoteType");
}




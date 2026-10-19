#pragma once

class  MarketIvQuoteType_
{
public:
    enum class Value_ : char
    {
     _NOT_SET=-1,
     PRICE,
     DECIMAL_IV,
     _N_VALUES
    } val_;
      
    MarketIvQuoteType_(Value_ val) : val_(val) {
        REQUIRE(val < Value_::_N_VALUES, "val is not valid");
    }
private:
    friend bool operator==(const MarketIvQuoteType_& lhs, const MarketIvQuoteType_& rhs);
    friend struct ReadStringMarketIvQuoteType_;
    friend Vector_<MarketIvQuoteType_> MarketIvQuoteTypeListAll();
    friend bool operator<(const MarketIvQuoteType_& lhs, const MarketIvQuoteType_& rhs) {
        return lhs.val_ < rhs.val_;
    }
public:
    explicit MarketIvQuoteType_(const String_& src);
    const char* String() const;
    Value_ Switch() const {return val_;}
    MarketIvQuoteType_() : val_(Value_::_NOT_SET) {};
};

Vector_<MarketIvQuoteType_> MarketIvQuoteTypeListAll();

bool operator==(const MarketIvQuoteType_& lhs, const MarketIvQuoteType_& rhs);
inline bool operator!=(const MarketIvQuoteType_& lhs, const MarketIvQuoteType_& rhs) {return !(lhs == rhs);}
inline bool operator==(const MarketIvQuoteType_& lhs, MarketIvQuoteType_::Value_ rhs) {return lhs.Switch() == rhs;}
inline bool operator!=(const MarketIvQuoteType_& lhs, MarketIvQuoteType_::Value_ rhs) {return lhs.Switch() != rhs;}

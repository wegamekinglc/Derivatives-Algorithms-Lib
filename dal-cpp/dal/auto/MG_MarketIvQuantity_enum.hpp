#pragma once

class  MarketIvQuantity_
{
public:
    enum class Value_ : char
    {
     _NOT_SET=-1,
     TOTAL_VARIANCE,
     IMPLIED_VOL,
     CALL_PRICE,
     PUT_PRICE,
     LOCAL_VOL,
     _N_VALUES
    } val_;
      
    MarketIvQuantity_(Value_ val) : val_(val) {
        REQUIRE(val < Value_::_N_VALUES, "val is not valid");
    }
private:
    friend bool operator==(const MarketIvQuantity_& lhs, const MarketIvQuantity_& rhs);
    friend struct ReadStringMarketIvQuantity_;
    friend Vector_<MarketIvQuantity_> MarketIvQuantityListAll();
    friend bool operator<(const MarketIvQuantity_& lhs, const MarketIvQuantity_& rhs) {
        return lhs.val_ < rhs.val_;
    }
public:
    explicit MarketIvQuantity_(const String_& src);
    const char* String() const;
    Value_ Switch() const {return val_;}
    MarketIvQuantity_() : val_(Value_::_NOT_SET) {};
};

Vector_<MarketIvQuantity_> MarketIvQuantityListAll();

bool operator==(const MarketIvQuantity_& lhs, const MarketIvQuantity_& rhs);
inline bool operator!=(const MarketIvQuantity_& lhs, const MarketIvQuantity_& rhs) {return !(lhs == rhs);}
inline bool operator==(const MarketIvQuantity_& lhs, MarketIvQuantity_::Value_ rhs) {return lhs.Switch() == rhs;}
inline bool operator!=(const MarketIvQuantity_& lhs, MarketIvQuantity_::Value_ rhs) {return lhs.Switch() != rhs;}

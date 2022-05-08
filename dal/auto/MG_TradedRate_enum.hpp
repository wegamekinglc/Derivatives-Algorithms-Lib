#pragma once

class __declspec(dllexport) TradedRate_
{
public:
    enum class Value_ : char
    {
     _NOT_SET=-1,
     LIBOR_3M_CME,
     LIBOR_3M_LCH,
     LIBOR_3M_FUT,
     LIBOR_6M_CME,
     LIBOR_6M_LCH,
     _N_VALUES
    } val_;
      
    TradedRate_(Value_ val) : val_(val) {
        REQUIRE(val < Value_::_N_VALUES, "val is not valid");
    }
private:
    friend __declspec(dllexport)bool operator==(const TradedRate_& lhs, const TradedRate_& rhs);
    friend struct ReadStringTradedRate_;
    friend Vector_<TradedRate_> TradedRateListAll();
    friend bool operator<(const TradedRate_& lhs, const TradedRate_& rhs) {
        return lhs.val_ < rhs.val_;
    }
public:
    explicit TradedRate_(const String_& src);
    const char* String() const;
    Value_ Switch() const {return val_;}
	// idiosyncratic (hand-written) members:
    PeriodLength_ Period() const;
    Clearer_ Clearer() const;
    TradedRate_() : val_(Value_::_NOT_SET) {};
};

Vector_<TradedRate_> TradedRateListAll();

__declspec(dllexport)bool operator==(const TradedRate_& lhs, const TradedRate_& rhs);
inline bool operator!=(const TradedRate_& lhs, const TradedRate_& rhs) {return !(lhs == rhs);}
inline bool operator==(const TradedRate_& lhs, TradedRate_::Value_ rhs) {return lhs.Switch() == rhs;}
inline bool operator!=(const TradedRate_& lhs, TradedRate_::Value_ rhs) {return lhs.Switch() != rhs;}

#pragma once

class  Ccy_
{
public:
    enum class Value_ : char
    {
     _NOT_SET=-1,
     USD,
     EUR,
     GBP,
     JPY,
     AUD,
     CHF,
     CAD,
     _N_VALUES
    } val_;
      
    Ccy_(Value_ val) : val_(val) {
        REQUIRE(val < Value_::_N_VALUES, "val is not valid");
    }
private:
    friend bool operator==(const Ccy_& lhs, const Ccy_& rhs);
    friend struct ReadStringCcy_;
    friend Vector_<Ccy_> CcyListAll();
    friend bool operator<(const Ccy_& lhs, const Ccy_& rhs) {
        return lhs.val_ < rhs.val_;
    }
public:
    explicit Ccy_(const String_& src);
    const char* String() const;
    Value_ Switch() const {return val_;}
    Ccy_() : val_(Value_::_NOT_SET) {};
};

Vector_<Ccy_> CcyListAll();

bool operator==(const Ccy_& lhs, const Ccy_& rhs);
inline bool operator!=(const Ccy_& lhs, const Ccy_& rhs) {return !(lhs == rhs);}
inline bool operator==(const Ccy_& lhs, Ccy_::Value_ rhs) {return lhs.Switch() == rhs;}
inline bool operator!=(const Ccy_& lhs, Ccy_::Value_ rhs) {return lhs.Switch() != rhs;}

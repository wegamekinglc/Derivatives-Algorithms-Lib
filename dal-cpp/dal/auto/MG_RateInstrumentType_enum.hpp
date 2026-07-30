#pragma once

class  RateInstrumentType_
{
public:
    enum class Value_ : char
    {
     _NOT_SET=-1,
     DEPOSIT,
     FRA,
     FUTURE,
     OIS,
     IRS,
     BASIS_SWAP,
     XCCY,
     _N_VALUES
    } val_;

    RateInstrumentType_(Value_ val) : val_(val) {
        REQUIRE(val < Value_::_N_VALUES, "val is not valid");
    }
private:
    friend bool operator==(const RateInstrumentType_& lhs, const RateInstrumentType_& rhs);
    friend struct ReadStringRateInstrumentType_;
    friend Vector_<RateInstrumentType_> RateInstrumentTypeListAll();
    friend bool operator<(const RateInstrumentType_& lhs, const RateInstrumentType_& rhs) {
        return lhs.val_ < rhs.val_;
    }
public:
    explicit RateInstrumentType_(const String_& src);
    const char* String() const;
    Value_ Switch() const {return val_;}
    RateInstrumentType_() : val_(Value_::_NOT_SET) {};
};

Vector_<RateInstrumentType_> RateInstrumentTypeListAll();

bool operator==(const RateInstrumentType_& lhs, const RateInstrumentType_& rhs);
inline bool operator!=(const RateInstrumentType_& lhs, const RateInstrumentType_& rhs) {return !(lhs == rhs);}
inline bool operator==(const RateInstrumentType_& lhs, RateInstrumentType_::Value_ rhs) {return lhs.Switch() == rhs;}
inline bool operator!=(const RateInstrumentType_& lhs, RateInstrumentType_::Value_ rhs) {return lhs.Switch() != rhs;}

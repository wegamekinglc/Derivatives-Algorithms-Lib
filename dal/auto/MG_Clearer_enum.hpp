#pragma once

class  Clearer_
{
public:
    enum class Value_ : char
    {
     _NOT_SET=-1,
     CME,
     LCH,
     _N_VALUES
    } val_;
      
    Clearer_(Value_ val) : val_(val) {
        REQUIRE(val < Value_::_N_VALUES, "val is not valid");
    }
private:
    friend bool operator==(const Clearer_& lhs, const Clearer_& rhs);
    friend struct ReadStringClearer_;
    friend Vector_<Clearer_> ClearerListAll();
    friend bool operator<(const Clearer_& lhs, const Clearer_& rhs) {
        return lhs.val_ < rhs.val_;
    }
public:
    explicit Clearer_(const String_& src);
    const char* String() const;
    Value_ Switch() const {return val_;}
	// idiosyncratic (hand-written) members:
    CollateralType_ Collateral() const;
    Clearer_() : val_(Value_::_NOT_SET) {};
};

Vector_<Clearer_> ClearerListAll();

bool operator==(const Clearer_& lhs, const Clearer_& rhs);
inline bool operator!=(const Clearer_& lhs, const Clearer_& rhs) {return !(lhs == rhs);}
inline bool operator==(const Clearer_& lhs, Clearer_::Value_ rhs) {return lhs.Switch() == rhs;}
inline bool operator!=(const Clearer_& lhs, Clearer_::Value_ rhs) {return lhs.Switch() != rhs;}

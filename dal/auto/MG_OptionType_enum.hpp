#pragma once

class __declspec(dllexport) OptionType_
{
public:
    enum class Value_ : char
    {
     _NOT_SET=-1,
     CALL,
     PUT,
     STRADDLE,
     _N_VALUES
    } val_;
      
    OptionType_(Value_ val) : val_(val) {
        REQUIRE(val < Value_::_N_VALUES, "val is not valid");
    }
private:
    friend __declspec(dllexport)bool operator==(const OptionType_& lhs, const OptionType_& rhs);
    friend struct ReadStringOptionType_;
    friend Vector_<OptionType_> OptionTypeListAll();
    friend bool operator<(const OptionType_& lhs, const OptionType_& rhs) {
        return lhs.val_ < rhs.val_;
    }
public:
    explicit OptionType_(const String_& src);
    const char* String() const;
    Value_ Switch() const {return val_;}
	// idiosyncratic (hand-written) members:
    double Payout(double spot, double strike) const;
    OptionType_ Opposite() const;
    OptionType_() : val_(Value_::_NOT_SET) {};
};

Vector_<OptionType_> OptionTypeListAll();

__declspec(dllexport)bool operator==(const OptionType_& lhs, const OptionType_& rhs);
inline bool operator!=(const OptionType_& lhs, const OptionType_& rhs) {return !(lhs == rhs);}
inline bool operator==(const OptionType_& lhs, OptionType_::Value_ rhs) {return lhs.Switch() == rhs;}
inline bool operator!=(const OptionType_& lhs, OptionType_::Value_ rhs) {return lhs.Switch() != rhs;}

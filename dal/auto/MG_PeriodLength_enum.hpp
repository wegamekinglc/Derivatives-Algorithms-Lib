#pragma once

class __declspec(dllexport) PeriodLength_
{
    enum class Value_ : char
    {
     _NOT_SET=-1,
     ANNUAL,
     SEMIANNUAL,
     QUARTERLY,
     MONTHLY,
     _N_VALUES
    } val_;
      
    PeriodLength_(Value_ val) : val_(val) {
        REQUIRE(val < Value_::_N_VALUES, "val is not valid");
    }
    friend __declspec(dllexport)bool operator==(const PeriodLength_& lhs, const PeriodLength_& rhs);
    friend struct ReadStringPeriodLength_;
    friend Vector_<PeriodLength_> PeriodLengthListAll();
    friend bool operator<(const PeriodLength_& lhs, const PeriodLength_& rhs) {
        return lhs.val_ < rhs.val_;
    }
public:
    explicit PeriodLength_(const String_& src);
    const char* String() const;
	// idiosyncratic (hand-written) members:
    int Months() const;
    PeriodLength_() : val_(Value_::_NOT_SET) {};
};

Vector_<PeriodLength_> PeriodLengthListAll();

__declspec(dllexport)bool operator==(const PeriodLength_& lhs, const PeriodLength_& rhs);
inline bool operator!=(const PeriodLength_& lhs, const PeriodLength_& rhs) {return !(lhs == rhs);}

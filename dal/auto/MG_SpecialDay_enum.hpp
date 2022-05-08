#pragma once

class __declspec(dllexport) SpecialDay_
{
    enum class Value_ : char
    {
     _NOT_SET=-1,
     IMM,
     IMM1,
     CDS,
     EOM,
     _N_VALUES
    } val_;
      
    SpecialDay_(Value_ val) : val_(val) {
        REQUIRE(val < Value_::_N_VALUES, "val is not valid");
    }
    friend __declspec(dllexport)bool operator==(const SpecialDay_& lhs, const SpecialDay_& rhs);
    friend struct ReadStringSpecialDay_;
    friend Vector_<SpecialDay_> SpecialDayListAll();
    friend bool operator<(const SpecialDay_& lhs, const SpecialDay_& rhs) {
        return lhs.val_ < rhs.val_;
    }
public:
    explicit SpecialDay_(const String_& src);
    const char* String() const;
	// idiosyncratic (hand-written) members:
    Date_ Step(const Date_& base, bool forward) const;
    SpecialDay_() : val_(Value_::_NOT_SET) {};
};

Vector_<SpecialDay_> SpecialDayListAll();

__declspec(dllexport)bool operator==(const SpecialDay_& lhs, const SpecialDay_& rhs);
inline bool operator!=(const SpecialDay_& lhs, const SpecialDay_& rhs) {return !(lhs == rhs);}

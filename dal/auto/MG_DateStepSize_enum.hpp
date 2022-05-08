#pragma once

class  DateStepSize_
{
    enum class Value_ : char
    {
     _NOT_SET=-1,
     Y,
     M,
     BD,
     CD,
     _N_VALUES
    } val_;
      
    DateStepSize_(Value_ val) : val_(val) {
        REQUIRE(val < Value_::_N_VALUES, "val is not valid");
    }
    friend bool operator==(const DateStepSize_& lhs, const DateStepSize_& rhs);
    friend struct ReadStringDateStepSize_;
    friend Vector_<DateStepSize_> DateStepSizeListAll();
    friend bool operator<(const DateStepSize_& lhs, const DateStepSize_& rhs) {
        return lhs.val_ < rhs.val_;
    }
public:
    explicit DateStepSize_(const String_& src);
    const char* String() const;
	// idiosyncratic (hand-written) members:
    Date_ operator() (const Date_& base, bool forward, int n_steps, const Holidays_& holidays) const;
    DateStepSize_() : val_(Value_::_NOT_SET) {};
};

Vector_<DateStepSize_> DateStepSizeListAll();

bool operator==(const DateStepSize_& lhs, const DateStepSize_& rhs);
inline bool operator!=(const DateStepSize_& lhs, const DateStepSize_& rhs) {return !(lhs == rhs);}

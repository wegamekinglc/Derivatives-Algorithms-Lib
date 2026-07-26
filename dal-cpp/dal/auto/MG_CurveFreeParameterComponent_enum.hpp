#pragma once

class  CurveFreeParameterComponent_
{
public:
    enum class Value_ : char
    {
     _NOT_SET=-1,
     RIGHT_FORWARD,
     LEFT_FORWARD,
     ZERO_RATE,
     LOG_DISCOUNT_FACTOR,
     _N_VALUES
    } val_;
      
    CurveFreeParameterComponent_(Value_ val) : val_(val) {
        REQUIRE(val < Value_::_N_VALUES, "val is not valid");
    }
private:
    friend bool operator==(const CurveFreeParameterComponent_& lhs, const CurveFreeParameterComponent_& rhs);
    friend struct ReadStringCurveFreeParameterComponent_;
    friend Vector_<CurveFreeParameterComponent_> CurveFreeParameterComponentListAll();
    friend bool operator<(const CurveFreeParameterComponent_& lhs, const CurveFreeParameterComponent_& rhs) {
        return lhs.val_ < rhs.val_;
    }
public:
    explicit CurveFreeParameterComponent_(const String_& src);
    const char* String() const;
    Value_ Switch() const {return val_;}
    CurveFreeParameterComponent_() : val_(Value_::_NOT_SET) {};
};

Vector_<CurveFreeParameterComponent_> CurveFreeParameterComponentListAll();

bool operator==(const CurveFreeParameterComponent_& lhs, const CurveFreeParameterComponent_& rhs);
inline bool operator!=(const CurveFreeParameterComponent_& lhs, const CurveFreeParameterComponent_& rhs) {return !(lhs == rhs);}
inline bool operator==(const CurveFreeParameterComponent_& lhs, CurveFreeParameterComponent_::Value_ rhs) {return lhs.Switch() == rhs;}
inline bool operator!=(const CurveFreeParameterComponent_& lhs, CurveFreeParameterComponent_::Value_ rhs) {return lhs.Switch() != rhs;}

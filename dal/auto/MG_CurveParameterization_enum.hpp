#pragma once

class  CurveParameterization_
{
public:
    enum class Value_ : char
    {
     _NOT_SET=-1,
     PIECEWISE_LINEAR_FWD,
     PIECEWISE_CONSTANT_FWD,
     ZERO_RATE,
     LOG_DISCOUNT,
     _N_VALUES
    } val_;
      
    CurveParameterization_(Value_ val) : val_(val) {
        REQUIRE(val < Value_::_N_VALUES, "val is not valid");
    }
private:
    friend bool operator==(const CurveParameterization_& lhs, const CurveParameterization_& rhs);
    friend struct ReadStringCurveParameterization_;
    friend Vector_<CurveParameterization_> CurveParameterizationListAll();
    friend bool operator<(const CurveParameterization_& lhs, const CurveParameterization_& rhs) {
        return lhs.val_ < rhs.val_;
    }
public:
    explicit CurveParameterization_(const String_& src);
    const char* String() const;
    Value_ Switch() const {return val_;}
    CurveParameterization_() : val_(Value_::_NOT_SET) {};
};

Vector_<CurveParameterization_> CurveParameterizationListAll();

bool operator==(const CurveParameterization_& lhs, const CurveParameterization_& rhs);
inline bool operator!=(const CurveParameterization_& lhs, const CurveParameterization_& rhs) {return !(lhs == rhs);}
inline bool operator==(const CurveParameterization_& lhs, CurveParameterization_::Value_ rhs) {return lhs.Switch() == rhs;}
inline bool operator!=(const CurveParameterization_& lhs, CurveParameterization_::Value_ rhs) {return lhs.Switch() != rhs;}

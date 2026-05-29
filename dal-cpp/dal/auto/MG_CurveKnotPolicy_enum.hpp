#pragma once

class  CurveKnotPolicy_
{
public:
    enum class Value_ : char
    {
     _NOT_SET=-1,
     INPUT,
     INSTRUMENTS,
     AUGMENTED,
     _N_VALUES
    } val_;
      
    CurveKnotPolicy_(Value_ val) : val_(val) {
        REQUIRE(val < Value_::_N_VALUES, "val is not valid");
    }
private:
    friend bool operator==(const CurveKnotPolicy_& lhs, const CurveKnotPolicy_& rhs);
    friend struct ReadStringCurveKnotPolicy_;
    friend Vector_<CurveKnotPolicy_> CurveKnotPolicyListAll();
    friend bool operator<(const CurveKnotPolicy_& lhs, const CurveKnotPolicy_& rhs) {
        return lhs.val_ < rhs.val_;
    }
public:
    explicit CurveKnotPolicy_(const String_& src);
    const char* String() const;
    Value_ Switch() const {return val_;}
    CurveKnotPolicy_() : val_(Value_::_NOT_SET) {};
};

Vector_<CurveKnotPolicy_> CurveKnotPolicyListAll();

bool operator==(const CurveKnotPolicy_& lhs, const CurveKnotPolicy_& rhs);
inline bool operator!=(const CurveKnotPolicy_& lhs, const CurveKnotPolicy_& rhs) {return !(lhs == rhs);}
inline bool operator==(const CurveKnotPolicy_& lhs, CurveKnotPolicy_::Value_ rhs) {return lhs.Switch() == rhs;}
inline bool operator!=(const CurveKnotPolicy_& lhs, CurveKnotPolicy_::Value_ rhs) {return lhs.Switch() != rhs;}

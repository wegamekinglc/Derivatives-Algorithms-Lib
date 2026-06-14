#pragma once

class  CurveJacobianMode_
{
public:
    enum class Value_ : char
    {
     _NOT_SET=-1,
     BUMPED,
     ANALYTIC_LOG_DISCOUNT,
     AAD_TAPE,
     _N_VALUES
    } val_;
      
    CurveJacobianMode_(Value_ val) : val_(val) {
        REQUIRE(val < Value_::_N_VALUES, "val is not valid");
    }
private:
    friend bool operator==(const CurveJacobianMode_& lhs, const CurveJacobianMode_& rhs);
    friend struct ReadStringCurveJacobianMode_;
    friend Vector_<CurveJacobianMode_> CurveJacobianModeListAll();
    friend bool operator<(const CurveJacobianMode_& lhs, const CurveJacobianMode_& rhs) {
        return lhs.val_ < rhs.val_;
    }
public:
    explicit CurveJacobianMode_(const String_& src);
    const char* String() const;
    Value_ Switch() const {return val_;}
    CurveJacobianMode_() : val_(Value_::_NOT_SET) {};
};

Vector_<CurveJacobianMode_> CurveJacobianModeListAll();

bool operator==(const CurveJacobianMode_& lhs, const CurveJacobianMode_& rhs);
inline bool operator!=(const CurveJacobianMode_& lhs, const CurveJacobianMode_& rhs) {return !(lhs == rhs);}
inline bool operator==(const CurveJacobianMode_& lhs, CurveJacobianMode_::Value_ rhs) {return lhs.Switch() == rhs;}
inline bool operator!=(const CurveJacobianMode_& lhs, CurveJacobianMode_::Value_ rhs) {return lhs.Switch() != rhs;}

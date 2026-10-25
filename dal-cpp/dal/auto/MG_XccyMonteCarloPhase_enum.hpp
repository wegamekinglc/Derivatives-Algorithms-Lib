#pragma once

class  XccyMonteCarloPhase_
{
public:
    enum class Value_ : char
    {
     _NOT_SET=-1,
     Fit,
     Validation,
     HeldOut,
     _N_VALUES
    } val_;
      
    XccyMonteCarloPhase_(Value_ val) : val_(val) {
        REQUIRE(val < Value_::_N_VALUES, "val is not valid");
    }
private:
    friend bool operator==(const XccyMonteCarloPhase_& lhs, const XccyMonteCarloPhase_& rhs);
    friend struct ReadStringXccyMonteCarloPhase_;
    friend Vector_<XccyMonteCarloPhase_> XccyMonteCarloPhaseListAll();
    friend bool operator<(const XccyMonteCarloPhase_& lhs, const XccyMonteCarloPhase_& rhs) {
        return lhs.val_ < rhs.val_;
    }
public:
    explicit XccyMonteCarloPhase_(const String_& src);
    const char* String() const;
    Value_ Switch() const {return val_;}
    XccyMonteCarloPhase_() : val_(Value_::_NOT_SET) {};
};

Vector_<XccyMonteCarloPhase_> XccyMonteCarloPhaseListAll();

bool operator==(const XccyMonteCarloPhase_& lhs, const XccyMonteCarloPhase_& rhs);
inline bool operator!=(const XccyMonteCarloPhase_& lhs, const XccyMonteCarloPhase_& rhs) {return !(lhs == rhs);}
inline bool operator==(const XccyMonteCarloPhase_& lhs, XccyMonteCarloPhase_::Value_ rhs) {return lhs.Switch() == rhs;}
inline bool operator!=(const XccyMonteCarloPhase_& lhs, XccyMonteCarloPhase_::Value_ rhs) {return lhs.Switch() != rhs;}

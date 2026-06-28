#pragma once

class  BrentPhase_
{
public:
    enum class Value_ : char
    {
     _NOT_SET=-1,
     INITIALIZE,
     HUNT,
     BRACKETED,
     _N_VALUES
    } val_;
      
    BrentPhase_(Value_ val) : val_(val) {
        REQUIRE(val < Value_::_N_VALUES, "val is not valid");
    }
private:
    friend bool operator==(const BrentPhase_& lhs, const BrentPhase_& rhs);
    friend struct ReadStringBrentPhase_;
    friend Vector_<BrentPhase_> BrentPhaseListAll();
    friend bool operator<(const BrentPhase_& lhs, const BrentPhase_& rhs) {
        return lhs.val_ < rhs.val_;
    }
public:
    explicit BrentPhase_(const String_& src);
    const char* String() const;
    Value_ Switch() const {return val_;}
    BrentPhase_() : val_(Value_::_NOT_SET) {};
};

Vector_<BrentPhase_> BrentPhaseListAll();

bool operator==(const BrentPhase_& lhs, const BrentPhase_& rhs);
inline bool operator!=(const BrentPhase_& lhs, const BrentPhase_& rhs) {return !(lhs == rhs);}
inline bool operator==(const BrentPhase_& lhs, BrentPhase_::Value_ rhs) {return lhs.Switch() == rhs;}
inline bool operator!=(const BrentPhase_& lhs, BrentPhase_::Value_ rhs) {return lhs.Switch() != rhs;}

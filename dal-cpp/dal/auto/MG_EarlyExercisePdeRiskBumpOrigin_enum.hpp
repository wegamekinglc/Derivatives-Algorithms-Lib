#pragma once

class  EarlyExercisePdeRiskBumpOrigin_
{
public:
    enum class Value_ : char
    {
     _NOT_SET=-1,
     DEFAULT,
     EXPLICIT,
     _N_VALUES
    } val_;
      
    EarlyExercisePdeRiskBumpOrigin_(Value_ val) : val_(val) {
        REQUIRE(val < Value_::_N_VALUES, "val is not valid");
    }
private:
    friend bool operator==(const EarlyExercisePdeRiskBumpOrigin_& lhs, const EarlyExercisePdeRiskBumpOrigin_& rhs);
    friend struct ReadStringEarlyExercisePdeRiskBumpOrigin_;
    friend Vector_<EarlyExercisePdeRiskBumpOrigin_> EarlyExercisePdeRiskBumpOriginListAll();
    friend bool operator<(const EarlyExercisePdeRiskBumpOrigin_& lhs, const EarlyExercisePdeRiskBumpOrigin_& rhs) {
        return lhs.val_ < rhs.val_;
    }
public:
    explicit EarlyExercisePdeRiskBumpOrigin_(const String_& src);
    const char* String() const;
    Value_ Switch() const {return val_;}
    EarlyExercisePdeRiskBumpOrigin_() : val_(Value_::_NOT_SET) {};
};

Vector_<EarlyExercisePdeRiskBumpOrigin_> EarlyExercisePdeRiskBumpOriginListAll();

bool operator==(const EarlyExercisePdeRiskBumpOrigin_& lhs, const EarlyExercisePdeRiskBumpOrigin_& rhs);
inline bool operator!=(const EarlyExercisePdeRiskBumpOrigin_& lhs, const EarlyExercisePdeRiskBumpOrigin_& rhs) {return !(lhs == rhs);}
inline bool operator==(const EarlyExercisePdeRiskBumpOrigin_& lhs, EarlyExercisePdeRiskBumpOrigin_::Value_ rhs) {return lhs.Switch() == rhs;}
inline bool operator!=(const EarlyExercisePdeRiskBumpOrigin_& lhs, EarlyExercisePdeRiskBumpOrigin_::Value_ rhs) {return lhs.Switch() != rhs;}

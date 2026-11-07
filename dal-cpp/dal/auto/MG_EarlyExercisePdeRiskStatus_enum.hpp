#pragma once

class  EarlyExercisePdeRiskStatus_
{
public:
    enum class Value_ : char
    {
     _NOT_SET=-1,
     COMPLETE,
     PARTIAL,
     BASE_FAILED,
     WORK_LIMIT,
     _N_VALUES
    } val_;
      
    EarlyExercisePdeRiskStatus_(Value_ val) : val_(val) {
        REQUIRE(val < Value_::_N_VALUES, "val is not valid");
    }
private:
    friend bool operator==(const EarlyExercisePdeRiskStatus_& lhs, const EarlyExercisePdeRiskStatus_& rhs);
    friend struct ReadStringEarlyExercisePdeRiskStatus_;
    friend Vector_<EarlyExercisePdeRiskStatus_> EarlyExercisePdeRiskStatusListAll();
    friend bool operator<(const EarlyExercisePdeRiskStatus_& lhs, const EarlyExercisePdeRiskStatus_& rhs) {
        return lhs.val_ < rhs.val_;
    }
public:
    explicit EarlyExercisePdeRiskStatus_(const String_& src);
    const char* String() const;
    Value_ Switch() const {return val_;}
    EarlyExercisePdeRiskStatus_() : val_(Value_::_NOT_SET) {};
};

Vector_<EarlyExercisePdeRiskStatus_> EarlyExercisePdeRiskStatusListAll();

bool operator==(const EarlyExercisePdeRiskStatus_& lhs, const EarlyExercisePdeRiskStatus_& rhs);
inline bool operator!=(const EarlyExercisePdeRiskStatus_& lhs, const EarlyExercisePdeRiskStatus_& rhs) {return !(lhs == rhs);}
inline bool operator==(const EarlyExercisePdeRiskStatus_& lhs, EarlyExercisePdeRiskStatus_::Value_ rhs) {return lhs.Switch() == rhs;}
inline bool operator!=(const EarlyExercisePdeRiskStatus_& lhs, EarlyExercisePdeRiskStatus_::Value_ rhs) {return lhs.Switch() != rhs;}

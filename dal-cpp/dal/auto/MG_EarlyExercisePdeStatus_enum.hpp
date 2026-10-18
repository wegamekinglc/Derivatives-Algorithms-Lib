#pragma once

class  EarlyExercisePdeStatus_
{
public:
    enum class Value_ : char
    {
     _NOT_SET=-1,
     SUCCESS,
     MAX_ITERATIONS,
     LCP_RESIDUAL_LIMIT,
     LINEAR_SOLVE_FAILURE,
     NON_MONOTONE_OPERATOR,
     NONFINITE,
     WORK_LIMIT,
     _N_VALUES
    } val_;
      
    EarlyExercisePdeStatus_(Value_ val) : val_(val) {
        REQUIRE(val < Value_::_N_VALUES, "val is not valid");
    }
private:
    friend bool operator==(const EarlyExercisePdeStatus_& lhs, const EarlyExercisePdeStatus_& rhs);
    friend struct ReadStringEarlyExercisePdeStatus_;
    friend Vector_<EarlyExercisePdeStatus_> EarlyExercisePdeStatusListAll();
    friend bool operator<(const EarlyExercisePdeStatus_& lhs, const EarlyExercisePdeStatus_& rhs) {
        return lhs.val_ < rhs.val_;
    }
public:
    explicit EarlyExercisePdeStatus_(const String_& src);
    const char* String() const;
    Value_ Switch() const {return val_;}
    EarlyExercisePdeStatus_() : val_(Value_::_NOT_SET) {};
};

Vector_<EarlyExercisePdeStatus_> EarlyExercisePdeStatusListAll();

bool operator==(const EarlyExercisePdeStatus_& lhs, const EarlyExercisePdeStatus_& rhs);
inline bool operator!=(const EarlyExercisePdeStatus_& lhs, const EarlyExercisePdeStatus_& rhs) {return !(lhs == rhs);}
inline bool operator==(const EarlyExercisePdeStatus_& lhs, EarlyExercisePdeStatus_::Value_ rhs) {return lhs.Switch() == rhs;}
inline bool operator!=(const EarlyExercisePdeStatus_& lhs, EarlyExercisePdeStatus_::Value_ rhs) {return lhs.Switch() != rhs;}

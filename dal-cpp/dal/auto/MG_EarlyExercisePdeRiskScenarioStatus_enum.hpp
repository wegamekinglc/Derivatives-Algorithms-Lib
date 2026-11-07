#pragma once

class  EarlyExercisePdeRiskScenarioStatus_
{
public:
    enum class Value_ : char
    {
     _NOT_SET=-1,
     PLANNED,
     SUCCESS,
     NUMERICAL_FAILURE,
     NOT_RUN_WORK_LIMIT,
     NOT_RUN_BASE_FAILED,
     _N_VALUES
    } val_;
      
    EarlyExercisePdeRiskScenarioStatus_(Value_ val) : val_(val) {
        REQUIRE(val < Value_::_N_VALUES, "val is not valid");
    }
private:
    friend bool operator==(const EarlyExercisePdeRiskScenarioStatus_& lhs, const EarlyExercisePdeRiskScenarioStatus_& rhs);
    friend struct ReadStringEarlyExercisePdeRiskScenarioStatus_;
    friend Vector_<EarlyExercisePdeRiskScenarioStatus_> EarlyExercisePdeRiskScenarioStatusListAll();
    friend bool operator<(const EarlyExercisePdeRiskScenarioStatus_& lhs, const EarlyExercisePdeRiskScenarioStatus_& rhs) {
        return lhs.val_ < rhs.val_;
    }
public:
    explicit EarlyExercisePdeRiskScenarioStatus_(const String_& src);
    const char* String() const;
    Value_ Switch() const {return val_;}
    EarlyExercisePdeRiskScenarioStatus_() : val_(Value_::_NOT_SET) {};
};

Vector_<EarlyExercisePdeRiskScenarioStatus_> EarlyExercisePdeRiskScenarioStatusListAll();

bool operator==(const EarlyExercisePdeRiskScenarioStatus_& lhs, const EarlyExercisePdeRiskScenarioStatus_& rhs);
inline bool operator!=(const EarlyExercisePdeRiskScenarioStatus_& lhs, const EarlyExercisePdeRiskScenarioStatus_& rhs) {return !(lhs == rhs);}
inline bool operator==(const EarlyExercisePdeRiskScenarioStatus_& lhs, EarlyExercisePdeRiskScenarioStatus_::Value_ rhs) {return lhs.Switch() == rhs;}
inline bool operator!=(const EarlyExercisePdeRiskScenarioStatus_& lhs, EarlyExercisePdeRiskScenarioStatus_::Value_ rhs) {return lhs.Switch() != rhs;}

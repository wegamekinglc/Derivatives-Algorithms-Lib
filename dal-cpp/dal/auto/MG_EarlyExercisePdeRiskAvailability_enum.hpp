#pragma once

class  EarlyExercisePdeRiskAvailability_
{
public:
    enum class Value_ : char
    {
     _NOT_SET=-1,
     AVAILABLE,
     AVAILABLE_EXERCISE_REGION,
     UNAVAILABLE_BASE_FAILED,
     UNAVAILABLE_WORK_LIMIT,
     UNAVAILABLE_SCENARIO_FAILED,
     UNAVAILABLE_DEGENERATE_DIFFUSION,
     UNAVAILABLE_DOMAIN_BOUNDARY,
     UNAVAILABLE_NEAR_EXERCISE_BOUNDARY,
     UNAVAILABLE_EXERCISE_SWITCH,
     UNAVAILABLE_NONFINITE,
     UNAVAILABLE_STEP_UNSTABLE,
     _N_VALUES
    } val_;
      
    EarlyExercisePdeRiskAvailability_(Value_ val) : val_(val) {
        REQUIRE(val < Value_::_N_VALUES, "val is not valid");
    }
private:
    friend bool operator==(const EarlyExercisePdeRiskAvailability_& lhs, const EarlyExercisePdeRiskAvailability_& rhs);
    friend struct ReadStringEarlyExercisePdeRiskAvailability_;
    friend Vector_<EarlyExercisePdeRiskAvailability_> EarlyExercisePdeRiskAvailabilityListAll();
    friend bool operator<(const EarlyExercisePdeRiskAvailability_& lhs, const EarlyExercisePdeRiskAvailability_& rhs) {
        return lhs.val_ < rhs.val_;
    }
public:
    explicit EarlyExercisePdeRiskAvailability_(const String_& src);
    const char* String() const;
    Value_ Switch() const {return val_;}
    EarlyExercisePdeRiskAvailability_() : val_(Value_::_NOT_SET) {};
};

Vector_<EarlyExercisePdeRiskAvailability_> EarlyExercisePdeRiskAvailabilityListAll();

bool operator==(const EarlyExercisePdeRiskAvailability_& lhs, const EarlyExercisePdeRiskAvailability_& rhs);
inline bool operator!=(const EarlyExercisePdeRiskAvailability_& lhs, const EarlyExercisePdeRiskAvailability_& rhs) {return !(lhs == rhs);}
inline bool operator==(const EarlyExercisePdeRiskAvailability_& lhs, EarlyExercisePdeRiskAvailability_::Value_ rhs) {return lhs.Switch() == rhs;}
inline bool operator!=(const EarlyExercisePdeRiskAvailability_& lhs, EarlyExercisePdeRiskAvailability_::Value_ rhs) {return lhs.Switch() != rhs;}

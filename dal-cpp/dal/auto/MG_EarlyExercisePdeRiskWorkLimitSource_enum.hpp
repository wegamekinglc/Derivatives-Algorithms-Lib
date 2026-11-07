#pragma once

class  EarlyExercisePdeRiskWorkLimitSource_
{
public:
    enum class Value_ : char
    {
     _NOT_SET=-1,
     NONE,
     SCENARIO,
     GLOBAL,
     _N_VALUES
    } val_;
      
    EarlyExercisePdeRiskWorkLimitSource_(Value_ val) : val_(val) {
        REQUIRE(val < Value_::_N_VALUES, "val is not valid");
    }
private:
    friend bool operator==(const EarlyExercisePdeRiskWorkLimitSource_& lhs, const EarlyExercisePdeRiskWorkLimitSource_& rhs);
    friend struct ReadStringEarlyExercisePdeRiskWorkLimitSource_;
    friend Vector_<EarlyExercisePdeRiskWorkLimitSource_> EarlyExercisePdeRiskWorkLimitSourceListAll();
    friend bool operator<(const EarlyExercisePdeRiskWorkLimitSource_& lhs, const EarlyExercisePdeRiskWorkLimitSource_& rhs) {
        return lhs.val_ < rhs.val_;
    }
public:
    explicit EarlyExercisePdeRiskWorkLimitSource_(const String_& src);
    const char* String() const;
    Value_ Switch() const {return val_;}
    EarlyExercisePdeRiskWorkLimitSource_() : val_(Value_::_NOT_SET) {};
};

Vector_<EarlyExercisePdeRiskWorkLimitSource_> EarlyExercisePdeRiskWorkLimitSourceListAll();

bool operator==(const EarlyExercisePdeRiskWorkLimitSource_& lhs, const EarlyExercisePdeRiskWorkLimitSource_& rhs);
inline bool operator!=(const EarlyExercisePdeRiskWorkLimitSource_& lhs, const EarlyExercisePdeRiskWorkLimitSource_& rhs) {return !(lhs == rhs);}
inline bool operator==(const EarlyExercisePdeRiskWorkLimitSource_& lhs, EarlyExercisePdeRiskWorkLimitSource_::Value_ rhs) {return lhs.Switch() == rhs;}
inline bool operator!=(const EarlyExercisePdeRiskWorkLimitSource_& lhs, EarlyExercisePdeRiskWorkLimitSource_::Value_ rhs) {return lhs.Switch() != rhs;}

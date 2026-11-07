#pragma once

class  EarlyExercisePdeRiskMembership_
{
public:
    enum class Value_ : char
    {
     _NOT_SET=-1,
     NOT_EXERCISABLE,
     CONTINUATION,
     EXERCISE,
     BOUNDARY,
     _N_VALUES
    } val_;
      
    EarlyExercisePdeRiskMembership_(Value_ val) : val_(val) {
        REQUIRE(val < Value_::_N_VALUES, "val is not valid");
    }
private:
    friend bool operator==(const EarlyExercisePdeRiskMembership_& lhs, const EarlyExercisePdeRiskMembership_& rhs);
    friend struct ReadStringEarlyExercisePdeRiskMembership_;
    friend Vector_<EarlyExercisePdeRiskMembership_> EarlyExercisePdeRiskMembershipListAll();
    friend bool operator<(const EarlyExercisePdeRiskMembership_& lhs, const EarlyExercisePdeRiskMembership_& rhs) {
        return lhs.val_ < rhs.val_;
    }
public:
    explicit EarlyExercisePdeRiskMembership_(const String_& src);
    const char* String() const;
    Value_ Switch() const {return val_;}
    EarlyExercisePdeRiskMembership_() : val_(Value_::_NOT_SET) {};
};

Vector_<EarlyExercisePdeRiskMembership_> EarlyExercisePdeRiskMembershipListAll();

bool operator==(const EarlyExercisePdeRiskMembership_& lhs, const EarlyExercisePdeRiskMembership_& rhs);
inline bool operator!=(const EarlyExercisePdeRiskMembership_& lhs, const EarlyExercisePdeRiskMembership_& rhs) {return !(lhs == rhs);}
inline bool operator==(const EarlyExercisePdeRiskMembership_& lhs, EarlyExercisePdeRiskMembership_::Value_ rhs) {return lhs.Switch() == rhs;}
inline bool operator!=(const EarlyExercisePdeRiskMembership_& lhs, EarlyExercisePdeRiskMembership_::Value_ rhs) {return lhs.Switch() != rhs;}

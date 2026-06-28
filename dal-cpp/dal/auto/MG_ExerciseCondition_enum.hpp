#pragma once

class  ExerciseCondition_
{
public:
    enum class Value_ : char
    {
     _NOT_SET=-1,
     UNCONDITIONAL,
     ON_EXERCISE,
     ON_BARRIER_HIT,
     ON_CONTINUATION,
     _N_VALUES
    } val_;
      
    ExerciseCondition_(Value_ val) : val_(val) {
        REQUIRE(val < Value_::_N_VALUES, "val is not valid");
    }
private:
    friend bool operator==(const ExerciseCondition_& lhs, const ExerciseCondition_& rhs);
    friend struct ReadStringExerciseCondition_;
    friend Vector_<ExerciseCondition_> ExerciseConditionListAll();
    friend bool operator<(const ExerciseCondition_& lhs, const ExerciseCondition_& rhs) {
        return lhs.val_ < rhs.val_;
    }
public:
    explicit ExerciseCondition_(const String_& src);
    const char* String() const;
    Value_ Switch() const {return val_;}
    ExerciseCondition_() : val_(Value_::_NOT_SET) {};
};

Vector_<ExerciseCondition_> ExerciseConditionListAll();

bool operator==(const ExerciseCondition_& lhs, const ExerciseCondition_& rhs);
inline bool operator!=(const ExerciseCondition_& lhs, const ExerciseCondition_& rhs) {return !(lhs == rhs);}
inline bool operator==(const ExerciseCondition_& lhs, ExerciseCondition_::Value_ rhs) {return lhs.Switch() == rhs;}
inline bool operator!=(const ExerciseCondition_& lhs, ExerciseCondition_::Value_ rhs) {return lhs.Switch() != rhs;}

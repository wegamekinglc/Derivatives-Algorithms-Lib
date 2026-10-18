#pragma once

class  EarlyExerciseStyle_
{
public:
    enum class Value_ : char
    {
     _NOT_SET=-1,
     BERMUDAN,
     AMERICAN,
     _N_VALUES
    } val_;
      
    EarlyExerciseStyle_(Value_ val) : val_(val) {
        REQUIRE(val < Value_::_N_VALUES, "val is not valid");
    }
private:
    friend bool operator==(const EarlyExerciseStyle_& lhs, const EarlyExerciseStyle_& rhs);
    friend struct ReadStringEarlyExerciseStyle_;
    friend Vector_<EarlyExerciseStyle_> EarlyExerciseStyleListAll();
    friend bool operator<(const EarlyExerciseStyle_& lhs, const EarlyExerciseStyle_& rhs) {
        return lhs.val_ < rhs.val_;
    }
public:
    explicit EarlyExerciseStyle_(const String_& src);
    const char* String() const;
    Value_ Switch() const {return val_;}
    EarlyExerciseStyle_() : val_(Value_::_NOT_SET) {};
};

Vector_<EarlyExerciseStyle_> EarlyExerciseStyleListAll();

bool operator==(const EarlyExerciseStyle_& lhs, const EarlyExerciseStyle_& rhs);
inline bool operator!=(const EarlyExerciseStyle_& lhs, const EarlyExerciseStyle_& rhs) {return !(lhs == rhs);}
inline bool operator==(const EarlyExerciseStyle_& lhs, EarlyExerciseStyle_::Value_ rhs) {return lhs.Switch() == rhs;}
inline bool operator!=(const EarlyExerciseStyle_& lhs, EarlyExerciseStyle_::Value_ rhs) {return lhs.Switch() != rhs;}

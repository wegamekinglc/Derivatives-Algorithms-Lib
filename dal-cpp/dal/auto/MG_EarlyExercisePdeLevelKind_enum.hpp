#pragma once

class  EarlyExercisePdeLevelKind_
{
public:
    enum class Value_ : char
    {
     _NOT_SET=-1,
     BERMUDAN_PROJECTION,
     AMERICAN_LCP,
     TERMINAL,
     _N_VALUES
    } val_;
      
    EarlyExercisePdeLevelKind_(Value_ val) : val_(val) {
        REQUIRE(val < Value_::_N_VALUES, "val is not valid");
    }
private:
    friend bool operator==(const EarlyExercisePdeLevelKind_& lhs, const EarlyExercisePdeLevelKind_& rhs);
    friend struct ReadStringEarlyExercisePdeLevelKind_;
    friend Vector_<EarlyExercisePdeLevelKind_> EarlyExercisePdeLevelKindListAll();
    friend bool operator<(const EarlyExercisePdeLevelKind_& lhs, const EarlyExercisePdeLevelKind_& rhs) {
        return lhs.val_ < rhs.val_;
    }
public:
    explicit EarlyExercisePdeLevelKind_(const String_& src);
    const char* String() const;
    Value_ Switch() const {return val_;}
    EarlyExercisePdeLevelKind_() : val_(Value_::_NOT_SET) {};
};

Vector_<EarlyExercisePdeLevelKind_> EarlyExercisePdeLevelKindListAll();

bool operator==(const EarlyExercisePdeLevelKind_& lhs, const EarlyExercisePdeLevelKind_& rhs);
inline bool operator!=(const EarlyExercisePdeLevelKind_& lhs, const EarlyExercisePdeLevelKind_& rhs) {return !(lhs == rhs);}
inline bool operator==(const EarlyExercisePdeLevelKind_& lhs, EarlyExercisePdeLevelKind_::Value_ rhs) {return lhs.Switch() == rhs;}
inline bool operator!=(const EarlyExercisePdeLevelKind_& lhs, EarlyExercisePdeLevelKind_::Value_ rhs) {return lhs.Switch() != rhs;}

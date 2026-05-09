#pragma once

class  CurveSolveMode_
{
public:
    enum class Value_ : char
    {
     _NOT_SET=-1,
     EXACT,
     APPROXIMATE,
     _N_VALUES
    } val_;
      
    CurveSolveMode_(Value_ val) : val_(val) {
        REQUIRE(val < Value_::_N_VALUES, "val is not valid");
    }
private:
    friend bool operator==(const CurveSolveMode_& lhs, const CurveSolveMode_& rhs);
    friend struct ReadStringCurveSolveMode_;
    friend Vector_<CurveSolveMode_> CurveSolveModeListAll();
    friend bool operator<(const CurveSolveMode_& lhs, const CurveSolveMode_& rhs) {
        return lhs.val_ < rhs.val_;
    }
public:
    explicit CurveSolveMode_(const String_& src);
    const char* String() const;
    Value_ Switch() const {return val_;}
    CurveSolveMode_() : val_(Value_::_NOT_SET) {};
};

Vector_<CurveSolveMode_> CurveSolveModeListAll();

bool operator==(const CurveSolveMode_& lhs, const CurveSolveMode_& rhs);
inline bool operator!=(const CurveSolveMode_& lhs, const CurveSolveMode_& rhs) {return !(lhs == rhs);}
inline bool operator==(const CurveSolveMode_& lhs, CurveSolveMode_::Value_ rhs) {return lhs.Switch() == rhs;}
inline bool operator!=(const CurveSolveMode_& lhs, CurveSolveMode_::Value_ rhs) {return lhs.Switch() != rhs;}

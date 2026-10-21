#pragma once

class  CalibrationDerivative_
{
public:
    enum class Value_ : char
    {
     _NOT_SET=-1,
     Auto,
     Analytic,
     AAD,
     FiniteDifference,
     None,
     _N_VALUES
    } val_;
      
    CalibrationDerivative_(Value_ val) : val_(val) {
        REQUIRE(val < Value_::_N_VALUES, "val is not valid");
    }
private:
    friend bool operator==(const CalibrationDerivative_& lhs, const CalibrationDerivative_& rhs);
    friend struct ReadStringCalibrationDerivative_;
    friend Vector_<CalibrationDerivative_> CalibrationDerivativeListAll();
    friend bool operator<(const CalibrationDerivative_& lhs, const CalibrationDerivative_& rhs) {
        return lhs.val_ < rhs.val_;
    }
public:
    explicit CalibrationDerivative_(const String_& src);
    const char* String() const;
    Value_ Switch() const {return val_;}
    CalibrationDerivative_() : val_(Value_::_NOT_SET) {};
};

Vector_<CalibrationDerivative_> CalibrationDerivativeListAll();

bool operator==(const CalibrationDerivative_& lhs, const CalibrationDerivative_& rhs);
inline bool operator!=(const CalibrationDerivative_& lhs, const CalibrationDerivative_& rhs) {return !(lhs == rhs);}
inline bool operator==(const CalibrationDerivative_& lhs, CalibrationDerivative_::Value_ rhs) {return lhs.Switch() == rhs;}
inline bool operator!=(const CalibrationDerivative_& lhs, CalibrationDerivative_::Value_ rhs) {return lhs.Switch() != rhs;}

#pragma once

class  CalibrationEvaluation_
{
public:
    enum class Value_ : char
    {
     _NOT_SET=-1,
     AutoFast,
     RequireFast,
     GenericDeterministic,
     ScriptMC,
     _N_VALUES
    } val_;
      
    CalibrationEvaluation_(Value_ val) : val_(val) {
        REQUIRE(val < Value_::_N_VALUES, "val is not valid");
    }
private:
    friend bool operator==(const CalibrationEvaluation_& lhs, const CalibrationEvaluation_& rhs);
    friend struct ReadStringCalibrationEvaluation_;
    friend Vector_<CalibrationEvaluation_> CalibrationEvaluationListAll();
    friend bool operator<(const CalibrationEvaluation_& lhs, const CalibrationEvaluation_& rhs) {
        return lhs.val_ < rhs.val_;
    }
public:
    explicit CalibrationEvaluation_(const String_& src);
    const char* String() const;
    Value_ Switch() const {return val_;}
    CalibrationEvaluation_() : val_(Value_::_NOT_SET) {};
};

Vector_<CalibrationEvaluation_> CalibrationEvaluationListAll();

bool operator==(const CalibrationEvaluation_& lhs, const CalibrationEvaluation_& rhs);
inline bool operator!=(const CalibrationEvaluation_& lhs, const CalibrationEvaluation_& rhs) {return !(lhs == rhs);}
inline bool operator==(const CalibrationEvaluation_& lhs, CalibrationEvaluation_::Value_ rhs) {return lhs.Switch() == rhs;}
inline bool operator!=(const CalibrationEvaluation_& lhs, CalibrationEvaluation_::Value_ rhs) {return lhs.Switch() != rhs;}

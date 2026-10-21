#pragma once

class  CalibrationStage_
{
public:
    enum class Value_ : char
    {
     _NOT_SET=-1,
     Input,
     Prepare,
     Evaluate,
     Solve,
     Validate,
     Load,
     _N_VALUES
    } val_;
      
    CalibrationStage_(Value_ val) : val_(val) {
        REQUIRE(val < Value_::_N_VALUES, "val is not valid");
    }
private:
    friend bool operator==(const CalibrationStage_& lhs, const CalibrationStage_& rhs);
    friend struct ReadStringCalibrationStage_;
    friend Vector_<CalibrationStage_> CalibrationStageListAll();
    friend bool operator<(const CalibrationStage_& lhs, const CalibrationStage_& rhs) {
        return lhs.val_ < rhs.val_;
    }
public:
    explicit CalibrationStage_(const String_& src);
    const char* String() const;
    Value_ Switch() const {return val_;}
    CalibrationStage_() : val_(Value_::_NOT_SET) {};
};

Vector_<CalibrationStage_> CalibrationStageListAll();

bool operator==(const CalibrationStage_& lhs, const CalibrationStage_& rhs);
inline bool operator!=(const CalibrationStage_& lhs, const CalibrationStage_& rhs) {return !(lhs == rhs);}
inline bool operator==(const CalibrationStage_& lhs, CalibrationStage_::Value_ rhs) {return lhs.Switch() == rhs;}
inline bool operator!=(const CalibrationStage_& lhs, CalibrationStage_::Value_ rhs) {return lhs.Switch() != rhs;}

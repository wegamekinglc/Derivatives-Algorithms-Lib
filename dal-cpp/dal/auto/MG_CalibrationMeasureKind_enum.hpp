#pragma once

class  CalibrationMeasureKind_
{
public:
    enum class Value_ : char
    {
     _NOT_SET=-1,
     Price,
     ParAffineRate,
     ParAffineSpread,
     FxOutright,
     _N_VALUES
    } val_;
      
    CalibrationMeasureKind_(Value_ val) : val_(val) {
        REQUIRE(val < Value_::_N_VALUES, "val is not valid");
    }
private:
    friend bool operator==(const CalibrationMeasureKind_& lhs, const CalibrationMeasureKind_& rhs);
    friend struct ReadStringCalibrationMeasureKind_;
    friend Vector_<CalibrationMeasureKind_> CalibrationMeasureKindListAll();
    friend bool operator<(const CalibrationMeasureKind_& lhs, const CalibrationMeasureKind_& rhs) {
        return lhs.val_ < rhs.val_;
    }
public:
    explicit CalibrationMeasureKind_(const String_& src);
    const char* String() const;
    Value_ Switch() const {return val_;}
    CalibrationMeasureKind_() : val_(Value_::_NOT_SET) {};
};

Vector_<CalibrationMeasureKind_> CalibrationMeasureKindListAll();

bool operator==(const CalibrationMeasureKind_& lhs, const CalibrationMeasureKind_& rhs);
inline bool operator!=(const CalibrationMeasureKind_& lhs, const CalibrationMeasureKind_& rhs) {return !(lhs == rhs);}
inline bool operator==(const CalibrationMeasureKind_& lhs, CalibrationMeasureKind_::Value_ rhs) {return lhs.Switch() == rhs;}
inline bool operator!=(const CalibrationMeasureKind_& lhs, CalibrationMeasureKind_::Value_ rhs) {return lhs.Switch() != rhs;}

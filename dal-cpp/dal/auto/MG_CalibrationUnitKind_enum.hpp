#pragma once

class  CalibrationUnitKind_
{
public:
    enum class Value_ : char
    {
     _NOT_SET=-1,
     Rate,
     BasisSpread,
     Price,
     FxOutright,
     BondPrice,
     YearFraction,
     Dimensionless,
     _N_VALUES
    } val_;
      
    CalibrationUnitKind_(Value_ val) : val_(val) {
        REQUIRE(val < Value_::_N_VALUES, "val is not valid");
    }
private:
    friend bool operator==(const CalibrationUnitKind_& lhs, const CalibrationUnitKind_& rhs);
    friend struct ReadStringCalibrationUnitKind_;
    friend Vector_<CalibrationUnitKind_> CalibrationUnitKindListAll();
    friend bool operator<(const CalibrationUnitKind_& lhs, const CalibrationUnitKind_& rhs) {
        return lhs.val_ < rhs.val_;
    }
public:
    explicit CalibrationUnitKind_(const String_& src);
    const char* String() const;
    Value_ Switch() const {return val_;}
    CalibrationUnitKind_() : val_(Value_::_NOT_SET) {};
};

Vector_<CalibrationUnitKind_> CalibrationUnitKindListAll();

bool operator==(const CalibrationUnitKind_& lhs, const CalibrationUnitKind_& rhs);
inline bool operator!=(const CalibrationUnitKind_& lhs, const CalibrationUnitKind_& rhs) {return !(lhs == rhs);}
inline bool operator==(const CalibrationUnitKind_& lhs, CalibrationUnitKind_::Value_ rhs) {return lhs.Switch() == rhs;}
inline bool operator!=(const CalibrationUnitKind_& lhs, CalibrationUnitKind_::Value_ rhs) {return lhs.Switch() != rhs;}

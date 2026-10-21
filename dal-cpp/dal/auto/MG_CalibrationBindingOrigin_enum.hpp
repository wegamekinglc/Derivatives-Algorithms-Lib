#pragma once

class  CalibrationBindingOrigin_
{
public:
    enum class Value_ : char
    {
     _NOT_SET=-1,
     SourceDefault,
     Literal,
     QuoteCopy,
     _N_VALUES
    } val_;
      
    CalibrationBindingOrigin_(Value_ val) : val_(val) {
        REQUIRE(val < Value_::_N_VALUES, "val is not valid");
    }
private:
    friend bool operator==(const CalibrationBindingOrigin_& lhs, const CalibrationBindingOrigin_& rhs);
    friend struct ReadStringCalibrationBindingOrigin_;
    friend Vector_<CalibrationBindingOrigin_> CalibrationBindingOriginListAll();
    friend bool operator<(const CalibrationBindingOrigin_& lhs, const CalibrationBindingOrigin_& rhs) {
        return lhs.val_ < rhs.val_;
    }
public:
    explicit CalibrationBindingOrigin_(const String_& src);
    const char* String() const;
    Value_ Switch() const {return val_;}
    CalibrationBindingOrigin_() : val_(Value_::_NOT_SET) {};
};

Vector_<CalibrationBindingOrigin_> CalibrationBindingOriginListAll();

bool operator==(const CalibrationBindingOrigin_& lhs, const CalibrationBindingOrigin_& rhs);
inline bool operator!=(const CalibrationBindingOrigin_& lhs, const CalibrationBindingOrigin_& rhs) {return !(lhs == rhs);}
inline bool operator==(const CalibrationBindingOrigin_& lhs, CalibrationBindingOrigin_::Value_ rhs) {return lhs.Switch() == rhs;}
inline bool operator!=(const CalibrationBindingOrigin_& lhs, CalibrationBindingOrigin_::Value_ rhs) {return lhs.Switch() != rhs;}

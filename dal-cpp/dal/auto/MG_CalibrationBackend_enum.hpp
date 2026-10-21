#pragma once

class  CalibrationBackend_
{
public:
    enum class Value_ : char
    {
     _NOT_SET=-1,
     FastDeterministic,
     FastGaussian,
     GenericDeterministic,
     ScriptMC,
     _N_VALUES
    } val_;
      
    CalibrationBackend_(Value_ val) : val_(val) {
        REQUIRE(val < Value_::_N_VALUES, "val is not valid");
    }
private:
    friend bool operator==(const CalibrationBackend_& lhs, const CalibrationBackend_& rhs);
    friend struct ReadStringCalibrationBackend_;
    friend Vector_<CalibrationBackend_> CalibrationBackendListAll();
    friend bool operator<(const CalibrationBackend_& lhs, const CalibrationBackend_& rhs) {
        return lhs.val_ < rhs.val_;
    }
public:
    explicit CalibrationBackend_(const String_& src);
    const char* String() const;
    Value_ Switch() const {return val_;}
    CalibrationBackend_() : val_(Value_::_NOT_SET) {};
};

Vector_<CalibrationBackend_> CalibrationBackendListAll();

bool operator==(const CalibrationBackend_& lhs, const CalibrationBackend_& rhs);
inline bool operator!=(const CalibrationBackend_& lhs, const CalibrationBackend_& rhs) {return !(lhs == rhs);}
inline bool operator==(const CalibrationBackend_& lhs, CalibrationBackend_::Value_ rhs) {return lhs.Switch() == rhs;}
inline bool operator!=(const CalibrationBackend_& lhs, CalibrationBackend_::Value_ rhs) {return lhs.Switch() != rhs;}

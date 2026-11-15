#pragma once

class  CompoundedOisObservationMode_
{
public:
    enum class Value_ : char
    {
     _NOT_SET=-1,
     None,
     Lookback,
     ObservationShift,
     Lockout,
     _N_VALUES
    } val_;
      
    CompoundedOisObservationMode_(Value_ val) : val_(val) {
        REQUIRE(val < Value_::_N_VALUES, "val is not valid");
    }
private:
    friend bool operator==(const CompoundedOisObservationMode_& lhs, const CompoundedOisObservationMode_& rhs);
    friend struct ReadStringCompoundedOisObservationMode_;
    friend Vector_<CompoundedOisObservationMode_> CompoundedOisObservationModeListAll();
    friend bool operator<(const CompoundedOisObservationMode_& lhs, const CompoundedOisObservationMode_& rhs) {
        return lhs.val_ < rhs.val_;
    }
public:
    explicit CompoundedOisObservationMode_(const String_& src);
    const char* String() const;
    Value_ Switch() const {return val_;}
    CompoundedOisObservationMode_() : val_(Value_::_NOT_SET) {};
};

Vector_<CompoundedOisObservationMode_> CompoundedOisObservationModeListAll();

bool operator==(const CompoundedOisObservationMode_& lhs, const CompoundedOisObservationMode_& rhs);
inline bool operator!=(const CompoundedOisObservationMode_& lhs, const CompoundedOisObservationMode_& rhs) {return !(lhs == rhs);}
inline bool operator==(const CompoundedOisObservationMode_& lhs, CompoundedOisObservationMode_::Value_ rhs) {return lhs.Switch() == rhs;}
inline bool operator!=(const CompoundedOisObservationMode_& lhs, CompoundedOisObservationMode_::Value_ rhs) {return lhs.Switch() != rhs;}

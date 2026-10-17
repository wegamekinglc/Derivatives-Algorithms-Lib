#pragma once

class  AADRecordingState_
{
public:
    enum class Value_ : char
    {
     _NOT_SET=-1,
     REGISTERING,
     RECORDING,
     READY,
     REVERSING,
     FAILED,
     CLOSED,
     _N_VALUES
    } val_;
      
    AADRecordingState_(Value_ val) : val_(val) {
        REQUIRE(val < Value_::_N_VALUES, "val is not valid");
    }
private:
    friend bool operator==(const AADRecordingState_& lhs, const AADRecordingState_& rhs);
    friend struct ReadStringAADRecordingState_;
    friend Vector_<AADRecordingState_> AADRecordingStateListAll();
    friend bool operator<(const AADRecordingState_& lhs, const AADRecordingState_& rhs) {
        return lhs.val_ < rhs.val_;
    }
public:
    explicit AADRecordingState_(const String_& src);
    const char* String() const;
    Value_ Switch() const {return val_;}
    AADRecordingState_() : val_(Value_::_NOT_SET) {};
};

Vector_<AADRecordingState_> AADRecordingStateListAll();

bool operator==(const AADRecordingState_& lhs, const AADRecordingState_& rhs);
inline bool operator!=(const AADRecordingState_& lhs, const AADRecordingState_& rhs) {return !(lhs == rhs);}
inline bool operator==(const AADRecordingState_& lhs, AADRecordingState_::Value_ rhs) {return lhs.Switch() == rhs;}
inline bool operator!=(const AADRecordingState_& lhs, AADRecordingState_::Value_ rhs) {return lhs.Switch() != rhs;}

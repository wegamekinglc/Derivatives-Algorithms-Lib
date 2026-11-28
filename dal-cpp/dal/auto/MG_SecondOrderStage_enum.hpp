#pragma once

class  SecondOrderStage_
{
public:
    enum class Value_ : char
    {
     _NOT_SET=-1,
     VALIDATION,
     RECORDING,
     REVERSE,
     RESULT,
     CLEANUP,
     _N_VALUES
    } val_;
      
    SecondOrderStage_(Value_ val) : val_(val) {
        REQUIRE(val < Value_::_N_VALUES, "val is not valid");
    }
private:
    friend bool operator==(const SecondOrderStage_& lhs, const SecondOrderStage_& rhs);
    friend struct ReadStringSecondOrderStage_;
    friend Vector_<SecondOrderStage_> SecondOrderStageListAll();
    friend bool operator<(const SecondOrderStage_& lhs, const SecondOrderStage_& rhs) {
        return lhs.val_ < rhs.val_;
    }
public:
    explicit SecondOrderStage_(const String_& src);
    const char* String() const;
    Value_ Switch() const {return val_;}
    SecondOrderStage_() : val_(Value_::_NOT_SET) {};
};

Vector_<SecondOrderStage_> SecondOrderStageListAll();

bool operator==(const SecondOrderStage_& lhs, const SecondOrderStage_& rhs);
inline bool operator!=(const SecondOrderStage_& lhs, const SecondOrderStage_& rhs) {return !(lhs == rhs);}
inline bool operator==(const SecondOrderStage_& lhs, SecondOrderStage_::Value_ rhs) {return lhs.Switch() == rhs;}
inline bool operator!=(const SecondOrderStage_& lhs, SecondOrderStage_::Value_ rhs) {return lhs.Switch() != rhs;}

#pragma once

class  SecondOrderErrorReason_
{
public:
    enum class Value_ : char
    {
     _NOT_SET=-1,
     INVALID_INPUT,
     OUTSIDE_DOMAIN,
     UNSUPPORTED_OPERATION,
     BUDGET_EXCEEDED,
     CAPACITY_EXCEEDED,
     LIFECYCLE,
     NUMERICAL_FAILURE,
     _N_VALUES
    } val_;
      
    SecondOrderErrorReason_(Value_ val) : val_(val) {
        REQUIRE(val < Value_::_N_VALUES, "val is not valid");
    }
private:
    friend bool operator==(const SecondOrderErrorReason_& lhs, const SecondOrderErrorReason_& rhs);
    friend struct ReadStringSecondOrderErrorReason_;
    friend Vector_<SecondOrderErrorReason_> SecondOrderErrorReasonListAll();
    friend bool operator<(const SecondOrderErrorReason_& lhs, const SecondOrderErrorReason_& rhs) {
        return lhs.val_ < rhs.val_;
    }
public:
    explicit SecondOrderErrorReason_(const String_& src);
    const char* String() const;
    Value_ Switch() const {return val_;}
    SecondOrderErrorReason_() : val_(Value_::_NOT_SET) {};
};

Vector_<SecondOrderErrorReason_> SecondOrderErrorReasonListAll();

bool operator==(const SecondOrderErrorReason_& lhs, const SecondOrderErrorReason_& rhs);
inline bool operator!=(const SecondOrderErrorReason_& lhs, const SecondOrderErrorReason_& rhs) {return !(lhs == rhs);}
inline bool operator==(const SecondOrderErrorReason_& lhs, SecondOrderErrorReason_::Value_ rhs) {return lhs.Switch() == rhs;}
inline bool operator!=(const SecondOrderErrorReason_& lhs, SecondOrderErrorReason_::Value_ rhs) {return lhs.Switch() != rhs;}

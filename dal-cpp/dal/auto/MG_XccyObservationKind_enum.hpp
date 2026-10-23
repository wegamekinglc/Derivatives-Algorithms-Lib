#pragma once

class  XccyObservationKind_
{
public:
    enum class Value_ : char
    {
     _NOT_SET=-1,
     RateFixing,
     ResetFxFixing,
     PaymentFxSpot,
     _N_VALUES
    } val_;
      
    XccyObservationKind_(Value_ val) : val_(val) {
        REQUIRE(val < Value_::_N_VALUES, "val is not valid");
    }
private:
    friend bool operator==(const XccyObservationKind_& lhs, const XccyObservationKind_& rhs);
    friend struct ReadStringXccyObservationKind_;
    friend Vector_<XccyObservationKind_> XccyObservationKindListAll();
    friend bool operator<(const XccyObservationKind_& lhs, const XccyObservationKind_& rhs) {
        return lhs.val_ < rhs.val_;
    }
public:
    explicit XccyObservationKind_(const String_& src);
    const char* String() const;
    Value_ Switch() const {return val_;}
    XccyObservationKind_() : val_(Value_::_NOT_SET) {};
};

Vector_<XccyObservationKind_> XccyObservationKindListAll();

bool operator==(const XccyObservationKind_& lhs, const XccyObservationKind_& rhs);
inline bool operator!=(const XccyObservationKind_& lhs, const XccyObservationKind_& rhs) {return !(lhs == rhs);}
inline bool operator==(const XccyObservationKind_& lhs, XccyObservationKind_::Value_ rhs) {return lhs.Switch() == rhs;}
inline bool operator!=(const XccyObservationKind_& lhs, XccyObservationKind_::Value_ rhs) {return lhs.Switch() != rhs;}

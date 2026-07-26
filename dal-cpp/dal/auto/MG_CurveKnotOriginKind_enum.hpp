#pragma once

class  CurveKnotOriginKind_
{
public:
    enum class Value_ : char
    {
     _NOT_SET=-1,
     INPUT,
     INSTRUMENT_START,
     INSTRUMENT_END,
     SYNTHETIC_ANCHOR,
     _N_VALUES
    } val_;

    CurveKnotOriginKind_(Value_ val) : val_(val) {
        REQUIRE(val < Value_::_N_VALUES, "val is not valid");
    }
private:
    friend bool operator==(const CurveKnotOriginKind_& lhs, const CurveKnotOriginKind_& rhs);
    friend struct ReadStringCurveKnotOriginKind_;
    friend Vector_<CurveKnotOriginKind_> CurveKnotOriginKindListAll();
    friend bool operator<(const CurveKnotOriginKind_& lhs, const CurveKnotOriginKind_& rhs) {
        return lhs.val_ < rhs.val_;
    }
public:
    explicit CurveKnotOriginKind_(const String_& src);
    const char* String() const;
    Value_ Switch() const {return val_;}
    CurveKnotOriginKind_() : val_(Value_::_NOT_SET) {};
};

Vector_<CurveKnotOriginKind_> CurveKnotOriginKindListAll();

bool operator==(const CurveKnotOriginKind_& lhs, const CurveKnotOriginKind_& rhs);
inline bool operator!=(const CurveKnotOriginKind_& lhs, const CurveKnotOriginKind_& rhs) {return !(lhs == rhs);}
inline bool operator==(const CurveKnotOriginKind_& lhs, CurveKnotOriginKind_::Value_ rhs) {return lhs.Switch() == rhs;}
inline bool operator!=(const CurveKnotOriginKind_& lhs, CurveKnotOriginKind_::Value_ rhs) {return lhs.Switch() != rhs;}

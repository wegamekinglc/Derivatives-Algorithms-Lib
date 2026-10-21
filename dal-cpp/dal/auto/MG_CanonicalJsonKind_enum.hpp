#pragma once

class  CanonicalJsonKind_
{
public:
    enum class Value_ : char
    {
     _NOT_SET=-1,
     NIL,
     BOOLEAN,
     NUMBER,
     STRING,
     ARRAY,
     OBJECT,
     _N_VALUES
    } val_;
      
    CanonicalJsonKind_(Value_ val) : val_(val) {
        REQUIRE(val < Value_::_N_VALUES, "val is not valid");
    }
private:
    friend bool operator==(const CanonicalJsonKind_& lhs, const CanonicalJsonKind_& rhs);
    friend struct ReadStringCanonicalJsonKind_;
    friend Vector_<CanonicalJsonKind_> CanonicalJsonKindListAll();
    friend bool operator<(const CanonicalJsonKind_& lhs, const CanonicalJsonKind_& rhs) {
        return lhs.val_ < rhs.val_;
    }
public:
    explicit CanonicalJsonKind_(const String_& src);
    const char* String() const;
    Value_ Switch() const {return val_;}
    CanonicalJsonKind_() : val_(Value_::_NOT_SET) {};
};

Vector_<CanonicalJsonKind_> CanonicalJsonKindListAll();

bool operator==(const CanonicalJsonKind_& lhs, const CanonicalJsonKind_& rhs);
inline bool operator!=(const CanonicalJsonKind_& lhs, const CanonicalJsonKind_& rhs) {return !(lhs == rhs);}
inline bool operator==(const CanonicalJsonKind_& lhs, CanonicalJsonKind_::Value_ rhs) {return lhs.Switch() == rhs;}
inline bool operator!=(const CanonicalJsonKind_& lhs, CanonicalJsonKind_::Value_ rhs) {return lhs.Switch() != rhs;}

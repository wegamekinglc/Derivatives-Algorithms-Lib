#pragma once

class  StackInfoType_
{
public:
    enum class Value_ : char
    {
     _NOT_SET=-1,
     INT,
     DBL,
     CSTR,
     STR,
     DATE,
     DATETIME,
     VOID,
     _N_VALUES
    } val_;
      
    StackInfoType_(Value_ val) : val_(val) {
        REQUIRE(val < Value_::_N_VALUES, "val is not valid");
    }
private:
    friend bool operator==(const StackInfoType_& lhs, const StackInfoType_& rhs);
    friend struct ReadStringStackInfoType_;
    friend Vector_<StackInfoType_> StackInfoTypeListAll();
    friend bool operator<(const StackInfoType_& lhs, const StackInfoType_& rhs) {
        return lhs.val_ < rhs.val_;
    }
public:
    explicit StackInfoType_(const String_& src);
    const char* String() const;
    Value_ Switch() const {return val_;}
    StackInfoType_() : val_(Value_::_NOT_SET) {};
};

Vector_<StackInfoType_> StackInfoTypeListAll();

bool operator==(const StackInfoType_& lhs, const StackInfoType_& rhs);
inline bool operator!=(const StackInfoType_& lhs, const StackInfoType_& rhs) {return !(lhs == rhs);}
inline bool operator==(const StackInfoType_& lhs, StackInfoType_::Value_ rhs) {return lhs.Switch() == rhs;}
inline bool operator!=(const StackInfoType_& lhs, StackInfoType_::Value_ rhs) {return lhs.Switch() != rhs;}

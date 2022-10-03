#pragma once

class  BizDayConvention_
{
public:
    enum class Value_ : char
    {
     _NOT_SET=-1,
     Following,
     ModifiedFollowing,
     _N_VALUES
    } val_;
      
    BizDayConvention_(Value_ val) : val_(val) {
        REQUIRE(val < Value_::_N_VALUES, "val is not valid");
    }
private:
    friend bool operator==(const BizDayConvention_& lhs, const BizDayConvention_& rhs);
    friend struct ReadStringBizDayConvention_;
    friend Vector_<BizDayConvention_> BizDayConventionListAll();
    friend bool operator<(const BizDayConvention_& lhs, const BizDayConvention_& rhs) {
        return lhs.val_ < rhs.val_;
    }
public:
    explicit BizDayConvention_(const String_& src);
    const char* String() const;
    Value_ Switch() const {return val_;}
    BizDayConvention_() : val_(Value_::_NOT_SET) {};
};

Vector_<BizDayConvention_> BizDayConventionListAll();

bool operator==(const BizDayConvention_& lhs, const BizDayConvention_& rhs);
inline bool operator!=(const BizDayConvention_& lhs, const BizDayConvention_& rhs) {return !(lhs == rhs);}
inline bool operator==(const BizDayConvention_& lhs, BizDayConvention_::Value_ rhs) {return lhs.Switch() == rhs;}
inline bool operator!=(const BizDayConvention_& lhs, BizDayConvention_::Value_ rhs) {return lhs.Switch() != rhs;}

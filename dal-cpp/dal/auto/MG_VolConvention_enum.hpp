#pragma once

class  VolConvention_
{
public:
    enum class Value_ : char
    {
     _NOT_SET=-1,
     NORMAL,
     BLACK,
     SHIFTED_BLACK,
     _N_VALUES
    } val_;
      
    VolConvention_(Value_ val) : val_(val) {
        REQUIRE(val < Value_::_N_VALUES, "val is not valid");
    }
private:
    friend bool operator==(const VolConvention_& lhs, const VolConvention_& rhs);
    friend struct ReadStringVolConvention_;
    friend Vector_<VolConvention_> VolConventionListAll();
    friend bool operator<(const VolConvention_& lhs, const VolConvention_& rhs) {
        return lhs.val_ < rhs.val_;
    }
public:
    explicit VolConvention_(const String_& src);
    const char* String() const;
    Value_ Switch() const {return val_;}
    VolConvention_() : val_(Value_::_NOT_SET) {};
};

Vector_<VolConvention_> VolConventionListAll();

bool operator==(const VolConvention_& lhs, const VolConvention_& rhs);
inline bool operator!=(const VolConvention_& lhs, const VolConvention_& rhs) {return !(lhs == rhs);}
inline bool operator==(const VolConvention_& lhs, VolConvention_::Value_ rhs) {return lhs.Switch() == rhs;}
inline bool operator!=(const VolConvention_& lhs, VolConvention_::Value_ rhs) {return lhs.Switch() != rhs;}

#pragma once

class  XccyNotionalMode_
{
public:
    enum class Value_ : char
    {
     _NOT_SET=-1,
     FIXED,
     RESETTABLE,
     MARK_TO_MARKET,
     _N_VALUES
    } val_;
      
    XccyNotionalMode_(Value_ val) : val_(val) {
        REQUIRE(val < Value_::_N_VALUES, "val is not valid");
    }
private:
    friend bool operator==(const XccyNotionalMode_& lhs, const XccyNotionalMode_& rhs);
    friend struct ReadStringXccyNotionalMode_;
    friend Vector_<XccyNotionalMode_> XccyNotionalModeListAll();
    friend bool operator<(const XccyNotionalMode_& lhs, const XccyNotionalMode_& rhs) {
        return lhs.val_ < rhs.val_;
    }
public:
    explicit XccyNotionalMode_(const String_& src);
    const char* String() const;
    Value_ Switch() const {return val_;}
    XccyNotionalMode_() : val_(Value_::_NOT_SET) {};
};

Vector_<XccyNotionalMode_> XccyNotionalModeListAll();

bool operator==(const XccyNotionalMode_& lhs, const XccyNotionalMode_& rhs);
inline bool operator!=(const XccyNotionalMode_& lhs, const XccyNotionalMode_& rhs) {return !(lhs == rhs);}
inline bool operator==(const XccyNotionalMode_& lhs, XccyNotionalMode_::Value_ rhs) {return lhs.Switch() == rhs;}
inline bool operator!=(const XccyNotionalMode_& lhs, XccyNotionalMode_::Value_ rhs) {return lhs.Switch() != rhs;}

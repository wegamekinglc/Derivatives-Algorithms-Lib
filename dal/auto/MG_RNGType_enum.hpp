#pragma once

class __declspec(dllexport) RNGType_
{
    enum class Value_ : char
    {
     _NOT_SET=-1,
     IRN,
     MRG32,
     _N_VALUES
    } val_;
      
    RNGType_(Value_ val) : val_(val) {
        REQUIRE(val < Value_::_N_VALUES, "val is not valid");
    }
    friend __declspec(dllexport)bool operator==(const RNGType_& lhs, const RNGType_& rhs);
    friend struct ReadStringRNGType_;
    friend Vector_<RNGType_> RNGTypeListAll();
    friend bool operator<(const RNGType_& lhs, const RNGType_& rhs) {
        return lhs.val_ < rhs.val_;
    }
public:
    explicit RNGType_(const String_& src);
    const char* String() const;
    RNGType_() : val_(Value_::_NOT_SET) {};
};

Vector_<RNGType_> RNGTypeListAll();

__declspec(dllexport)bool operator==(const RNGType_& lhs, const RNGType_& rhs);
inline bool operator!=(const RNGType_& lhs, const RNGType_& rhs) {return !(lhs == rhs);}

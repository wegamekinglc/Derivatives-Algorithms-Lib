#pragma once

class  CollateralType_
{
public:
    enum class Value_ : char
    {
     _NOT_SET=-1,
     OIS,
     GC,
     NONE,
     _N_VALUES
    } val_;
      
    CollateralType_(Value_ val) : val_(val) {
        REQUIRE(val < Value_::_N_VALUES, "val is not valid");
    }
private:
    friend bool operator==(const CollateralType_& lhs, const CollateralType_& rhs);
    friend struct ReadStringCollateralType_;
    friend Vector_<CollateralType_> CollateralTypeListAll();
    friend bool operator<(const CollateralType_& lhs, const CollateralType_& rhs) {
        return lhs.val_ < rhs.val_;
    }
public:
    explicit CollateralType_(const String_& src);
    const char* String() const;
    Value_ Switch() const {return val_;}
    CollateralType_() : val_(Value_::_NOT_SET) {};
};

Vector_<CollateralType_> CollateralTypeListAll();

bool operator==(const CollateralType_& lhs, const CollateralType_& rhs);
inline bool operator!=(const CollateralType_& lhs, const CollateralType_& rhs) {return !(lhs == rhs);}
inline bool operator==(const CollateralType_& lhs, CollateralType_::Value_ rhs) {return lhs.Switch() == rhs;}
inline bool operator!=(const CollateralType_& lhs, CollateralType_::Value_ rhs) {return lhs.Switch() != rhs;}

#pragma once

class  DomainCondProp_
{
public:
    enum class Value_ : char
    {
     _NOT_SET=-1,
     AlwaysTrue,
     AlwaysFalse,
     TrueOrFalse,
     _N_VALUES
    } val_;
      
    DomainCondProp_(Value_ val) : val_(val) {
        REQUIRE(val < Value_::_N_VALUES, "val is not valid");
    }
private:
    friend bool operator==(const DomainCondProp_& lhs, const DomainCondProp_& rhs);
    friend struct ReadStringDomainCondProp_;
    friend Vector_<DomainCondProp_> DomainCondPropListAll();
    friend bool operator<(const DomainCondProp_& lhs, const DomainCondProp_& rhs) {
        return lhs.val_ < rhs.val_;
    }
public:
    explicit DomainCondProp_(const String_& src);
    const char* String() const;
    Value_ Switch() const {return val_;}
    DomainCondProp_() : val_(Value_::_NOT_SET) {};
};

Vector_<DomainCondProp_> DomainCondPropListAll();

bool operator==(const DomainCondProp_& lhs, const DomainCondProp_& rhs);
inline bool operator!=(const DomainCondProp_& lhs, const DomainCondProp_& rhs) {return !(lhs == rhs);}
inline bool operator==(const DomainCondProp_& lhs, DomainCondProp_::Value_ rhs) {return lhs.Switch() == rhs;}
inline bool operator!=(const DomainCondProp_& lhs, DomainCondProp_::Value_ rhs) {return lhs.Switch() != rhs;}

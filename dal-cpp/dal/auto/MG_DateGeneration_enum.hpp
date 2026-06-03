#pragma once

class  DateGeneration_
{
public:
    enum class Value_ : char
    {
     _NOT_SET=-1,
     Backward,
     Forward,
     _N_VALUES
    } val_;
      
    DateGeneration_(Value_ val) : val_(val) {
        REQUIRE(val < Value_::_N_VALUES, "val is not valid");
    }
private:
    friend bool operator==(const DateGeneration_& lhs, const DateGeneration_& rhs);
    friend struct ReadStringDateGeneration_;
    friend Vector_<DateGeneration_> DateGenerationListAll();
    friend bool operator<(const DateGeneration_& lhs, const DateGeneration_& rhs) {
        return lhs.val_ < rhs.val_;
    }
public:
    explicit DateGeneration_(const String_& src);
    const char* String() const;
    Value_ Switch() const {return val_;}
    DateGeneration_() : val_(Value_::_NOT_SET) {};
};

Vector_<DateGeneration_> DateGenerationListAll();

bool operator==(const DateGeneration_& lhs, const DateGeneration_& rhs);
inline bool operator!=(const DateGeneration_& lhs, const DateGeneration_& rhs) {return !(lhs == rhs);}
inline bool operator==(const DateGeneration_& lhs, DateGeneration_::Value_ rhs) {return lhs.Switch() == rhs;}
inline bool operator!=(const DateGeneration_& lhs, DateGeneration_::Value_ rhs) {return lhs.Switch() != rhs;}

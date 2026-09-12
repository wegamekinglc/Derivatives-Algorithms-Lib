#pragma once

class  TodayFixingPolicy_
{
public:
    enum class Value_ : char
    {
     _NOT_SET=-1,
     MODEL,
     REQUIREHISTORICAL,
     _N_VALUES
    } val_;

    TodayFixingPolicy_(Value_ val) : val_(val) {
        REQUIRE(val < Value_::_N_VALUES, "val is not valid");
    }
private:
    friend bool operator==(const TodayFixingPolicy_& lhs, const TodayFixingPolicy_& rhs);
    friend struct ReadStringTodayFixingPolicy_;
    friend Vector_<TodayFixingPolicy_> TodayFixingPolicyListAll();
    friend bool operator<(const TodayFixingPolicy_& lhs, const TodayFixingPolicy_& rhs) {
        return lhs.val_ < rhs.val_;
    }
public:
    explicit TodayFixingPolicy_(const String_& src);
    const char* String() const;
    Value_ Switch() const {return val_;}
    TodayFixingPolicy_() : val_(Value_::_NOT_SET) {};
};

Vector_<TodayFixingPolicy_> TodayFixingPolicyListAll();

bool operator==(const TodayFixingPolicy_& lhs, const TodayFixingPolicy_& rhs);
inline bool operator!=(const TodayFixingPolicy_& lhs, const TodayFixingPolicy_& rhs) {return !(lhs == rhs);}
inline bool operator==(const TodayFixingPolicy_& lhs, TodayFixingPolicy_::Value_ rhs) {return lhs.Switch() == rhs;}
inline bool operator!=(const TodayFixingPolicy_& lhs, TodayFixingPolicy_::Value_ rhs) {return lhs.Switch() != rhs;}

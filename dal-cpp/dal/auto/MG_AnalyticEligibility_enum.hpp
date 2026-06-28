#pragma once

class  AnalyticEligibility_
{
public:
    enum class Value_ : char
    {
     _NOT_SET=-1,
     UNKNOWN,
     ELIGIBLE,
     INELIGIBLE,
     _N_VALUES
    } val_;
      
    AnalyticEligibility_(Value_ val) : val_(val) {
        REQUIRE(val < Value_::_N_VALUES, "val is not valid");
    }
private:
    friend bool operator==(const AnalyticEligibility_& lhs, const AnalyticEligibility_& rhs);
    friend struct ReadStringAnalyticEligibility_;
    friend Vector_<AnalyticEligibility_> AnalyticEligibilityListAll();
    friend bool operator<(const AnalyticEligibility_& lhs, const AnalyticEligibility_& rhs) {
        return lhs.val_ < rhs.val_;
    }
public:
    explicit AnalyticEligibility_(const String_& src);
    const char* String() const;
    Value_ Switch() const {return val_;}
    AnalyticEligibility_() : val_(Value_::_NOT_SET) {};
};

Vector_<AnalyticEligibility_> AnalyticEligibilityListAll();

bool operator==(const AnalyticEligibility_& lhs, const AnalyticEligibility_& rhs);
inline bool operator!=(const AnalyticEligibility_& lhs, const AnalyticEligibility_& rhs) {return !(lhs == rhs);}
inline bool operator==(const AnalyticEligibility_& lhs, AnalyticEligibility_::Value_ rhs) {return lhs.Switch() == rhs;}
inline bool operator!=(const AnalyticEligibility_& lhs, AnalyticEligibility_::Value_ rhs) {return lhs.Switch() != rhs;}

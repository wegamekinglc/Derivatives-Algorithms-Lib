#pragma once

class  CalibrationQuoteConventionId_
{
public:
    enum class Value_ : char
    {
     _NOT_SET=-1,
     DecimalRate,
     DecimalSpread,
     PresentValueAmount,
     QuotePerBase,
     _N_VALUES
    } val_;
      
    CalibrationQuoteConventionId_(Value_ val) : val_(val) {
        REQUIRE(val < Value_::_N_VALUES, "val is not valid");
    }
private:
    friend bool operator==(const CalibrationQuoteConventionId_& lhs, const CalibrationQuoteConventionId_& rhs);
    friend struct ReadStringCalibrationQuoteConventionId_;
    friend Vector_<CalibrationQuoteConventionId_> CalibrationQuoteConventionIdListAll();
    friend bool operator<(const CalibrationQuoteConventionId_& lhs, const CalibrationQuoteConventionId_& rhs) {
        return lhs.val_ < rhs.val_;
    }
public:
    explicit CalibrationQuoteConventionId_(const String_& src);
    const char* String() const;
    Value_ Switch() const {return val_;}
    CalibrationQuoteConventionId_() : val_(Value_::_NOT_SET) {};
};

Vector_<CalibrationQuoteConventionId_> CalibrationQuoteConventionIdListAll();

bool operator==(const CalibrationQuoteConventionId_& lhs, const CalibrationQuoteConventionId_& rhs);
inline bool operator!=(const CalibrationQuoteConventionId_& lhs, const CalibrationQuoteConventionId_& rhs) {return !(lhs == rhs);}
inline bool operator==(const CalibrationQuoteConventionId_& lhs, CalibrationQuoteConventionId_::Value_ rhs) {return lhs.Switch() == rhs;}
inline bool operator!=(const CalibrationQuoteConventionId_& lhs, CalibrationQuoteConventionId_::Value_ rhs) {return lhs.Switch() != rhs;}

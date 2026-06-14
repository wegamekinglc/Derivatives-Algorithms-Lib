#pragma once

class  LogDfScheme_
{
public:
    enum class Value_ : char
    {
     _NOT_SET=-1,
     LOG_LINEAR,
     LOG_CUBIC_NATURAL,
     MIXED,
     _N_VALUES
    } val_;
      
    LogDfScheme_(Value_ val) : val_(val) {
        REQUIRE(val < Value_::_N_VALUES, "val is not valid");
    }
private:
    friend bool operator==(const LogDfScheme_& lhs, const LogDfScheme_& rhs);
    friend struct ReadStringLogDfScheme_;
    friend Vector_<LogDfScheme_> LogDfSchemeListAll();
    friend bool operator<(const LogDfScheme_& lhs, const LogDfScheme_& rhs) {
        return lhs.val_ < rhs.val_;
    }
public:
    explicit LogDfScheme_(const String_& src);
    const char* String() const;
    Value_ Switch() const {return val_;}
    LogDfScheme_() : val_(Value_::_NOT_SET) {};
};

Vector_<LogDfScheme_> LogDfSchemeListAll();

bool operator==(const LogDfScheme_& lhs, const LogDfScheme_& rhs);
inline bool operator!=(const LogDfScheme_& lhs, const LogDfScheme_& rhs) {return !(lhs == rhs);}
inline bool operator==(const LogDfScheme_& lhs, LogDfScheme_::Value_ rhs) {return lhs.Switch() == rhs;}
inline bool operator!=(const LogDfScheme_& lhs, LogDfScheme_::Value_ rhs) {return lhs.Switch() != rhs;}

#pragma once

class  AuditorMode_
{
public:
    enum class Value_ : char
    {
     _NOT_SET=-1,
     PASSIVE,
     READING,
     READING_EXCLUSIVE,
     SHOWING,
     _N_VALUES
    } val_;
      
    AuditorMode_(Value_ val) : val_(val) {
        REQUIRE(val < Value_::_N_VALUES, "val is not valid");
    }
private:
    friend bool operator==(const AuditorMode_& lhs, const AuditorMode_& rhs);
    friend struct ReadStringAuditorMode_;
    friend Vector_<AuditorMode_> AuditorModeListAll();
    friend bool operator<(const AuditorMode_& lhs, const AuditorMode_& rhs) {
        return lhs.val_ < rhs.val_;
    }
public:
    explicit AuditorMode_(const String_& src);
    const char* String() const;
    Value_ Switch() const {return val_;}
    AuditorMode_() : val_(Value_::_NOT_SET) {};
};

Vector_<AuditorMode_> AuditorModeListAll();

bool operator==(const AuditorMode_& lhs, const AuditorMode_& rhs);
inline bool operator!=(const AuditorMode_& lhs, const AuditorMode_& rhs) {return !(lhs == rhs);}
inline bool operator==(const AuditorMode_& lhs, AuditorMode_::Value_ rhs) {return lhs.Switch() == rhs;}
inline bool operator!=(const AuditorMode_& lhs, AuditorMode_::Value_ rhs) {return lhs.Switch() != rhs;}

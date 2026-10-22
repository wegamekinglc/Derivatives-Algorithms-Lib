#pragma once

class  ScriptParameterRole_
{
public:
    enum class Value_ : char
    {
     _NOT_SET=-1,
     Notional,
     Coupon,
     Strike,
     Accrual,
     RateStrike,
     _N_VALUES
    } val_;
      
    ScriptParameterRole_(Value_ val) : val_(val) {
        REQUIRE(val < Value_::_N_VALUES, "val is not valid");
    }
private:
    friend bool operator==(const ScriptParameterRole_& lhs, const ScriptParameterRole_& rhs);
    friend struct ReadStringScriptParameterRole_;
    friend Vector_<ScriptParameterRole_> ScriptParameterRoleListAll();
    friend bool operator<(const ScriptParameterRole_& lhs, const ScriptParameterRole_& rhs) {
        return lhs.val_ < rhs.val_;
    }
public:
    explicit ScriptParameterRole_(const String_& src);
    const char* String() const;
    Value_ Switch() const {return val_;}
    ScriptParameterRole_() : val_(Value_::_NOT_SET) {};
};

Vector_<ScriptParameterRole_> ScriptParameterRoleListAll();

bool operator==(const ScriptParameterRole_& lhs, const ScriptParameterRole_& rhs);
inline bool operator!=(const ScriptParameterRole_& lhs, const ScriptParameterRole_& rhs) {return !(lhs == rhs);}
inline bool operator==(const ScriptParameterRole_& lhs, ScriptParameterRole_::Value_ rhs) {return lhs.Switch() == rhs;}
inline bool operator!=(const ScriptParameterRole_& lhs, ScriptParameterRole_::Value_ rhs) {return lhs.Switch() != rhs;}

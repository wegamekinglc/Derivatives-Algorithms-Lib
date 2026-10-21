#pragma once

class  ScriptParameterType_
{
public:
    enum class Value_ : char
    {
     _NOT_SET=-1,
     Real,
     _N_VALUES
    } val_;
      
    ScriptParameterType_(Value_ val) : val_(val) {
        REQUIRE(val < Value_::_N_VALUES, "val is not valid");
    }
private:
    friend bool operator==(const ScriptParameterType_& lhs, const ScriptParameterType_& rhs);
    friend struct ReadStringScriptParameterType_;
    friend Vector_<ScriptParameterType_> ScriptParameterTypeListAll();
    friend bool operator<(const ScriptParameterType_& lhs, const ScriptParameterType_& rhs) {
        return lhs.val_ < rhs.val_;
    }
public:
    explicit ScriptParameterType_(const String_& src);
    const char* String() const;
    Value_ Switch() const {return val_;}
    ScriptParameterType_() : val_(Value_::_NOT_SET) {};
};

Vector_<ScriptParameterType_> ScriptParameterTypeListAll();

bool operator==(const ScriptParameterType_& lhs, const ScriptParameterType_& rhs);
inline bool operator!=(const ScriptParameterType_& lhs, const ScriptParameterType_& rhs) {return !(lhs == rhs);}
inline bool operator==(const ScriptParameterType_& lhs, ScriptParameterType_::Value_ rhs) {return lhs.Switch() == rhs;}
inline bool operator!=(const ScriptParameterType_& lhs, ScriptParameterType_::Value_ rhs) {return lhs.Switch() != rhs;}

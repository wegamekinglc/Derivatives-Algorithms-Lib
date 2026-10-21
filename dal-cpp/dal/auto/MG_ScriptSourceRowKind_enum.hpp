#pragma once

class  ScriptSourceRowKind_
{
public:
    enum class Value_ : char
    {
     _NOT_SET=-1,
     Definition,
     Event,
     _N_VALUES
    } val_;
      
    ScriptSourceRowKind_(Value_ val) : val_(val) {
        REQUIRE(val < Value_::_N_VALUES, "val is not valid");
    }
private:
    friend bool operator==(const ScriptSourceRowKind_& lhs, const ScriptSourceRowKind_& rhs);
    friend struct ReadStringScriptSourceRowKind_;
    friend Vector_<ScriptSourceRowKind_> ScriptSourceRowKindListAll();
    friend bool operator<(const ScriptSourceRowKind_& lhs, const ScriptSourceRowKind_& rhs) {
        return lhs.val_ < rhs.val_;
    }
public:
    explicit ScriptSourceRowKind_(const String_& src);
    const char* String() const;
    Value_ Switch() const {return val_;}
    ScriptSourceRowKind_() : val_(Value_::_NOT_SET) {};
};

Vector_<ScriptSourceRowKind_> ScriptSourceRowKindListAll();

bool operator==(const ScriptSourceRowKind_& lhs, const ScriptSourceRowKind_& rhs);
inline bool operator!=(const ScriptSourceRowKind_& lhs, const ScriptSourceRowKind_& rhs) {return !(lhs == rhs);}
inline bool operator==(const ScriptSourceRowKind_& lhs, ScriptSourceRowKind_::Value_ rhs) {return lhs.Switch() == rhs;}
inline bool operator!=(const ScriptSourceRowKind_& lhs, ScriptSourceRowKind_::Value_ rhs) {return lhs.Switch() != rhs;}

#pragma once

class  ScriptSettlement_
{
public:
    enum class Value_ : char
    {
     _NOT_SET=-1,
     AtEvent,
     _N_VALUES
    } val_;
      
    ScriptSettlement_(Value_ val) : val_(val) {
        REQUIRE(val < Value_::_N_VALUES, "val is not valid");
    }
private:
    friend bool operator==(const ScriptSettlement_& lhs, const ScriptSettlement_& rhs);
    friend struct ReadStringScriptSettlement_;
    friend Vector_<ScriptSettlement_> ScriptSettlementListAll();
    friend bool operator<(const ScriptSettlement_& lhs, const ScriptSettlement_& rhs) {
        return lhs.val_ < rhs.val_;
    }
public:
    explicit ScriptSettlement_(const String_& src);
    const char* String() const;
    Value_ Switch() const {return val_;}
    ScriptSettlement_() : val_(Value_::_NOT_SET) {};
};

Vector_<ScriptSettlement_> ScriptSettlementListAll();

bool operator==(const ScriptSettlement_& lhs, const ScriptSettlement_& rhs);
inline bool operator!=(const ScriptSettlement_& lhs, const ScriptSettlement_& rhs) {return !(lhs == rhs);}
inline bool operator==(const ScriptSettlement_& lhs, ScriptSettlement_::Value_ rhs) {return lhs.Switch() == rhs;}
inline bool operator!=(const ScriptSettlement_& lhs, ScriptSettlement_::Value_ rhs) {return lhs.Switch() != rhs;}

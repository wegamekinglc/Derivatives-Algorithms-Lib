#pragma once

class  NodeType_
{
public:
    enum class Value_ : char
    {
     _NOT_SET=-1,
     Add,
     AddConst,
     Sub,
     SubConst,
     ConstSub,
     Mult,
     MultConst,
     Div,
     DivConst,
     ConstDiv,
     Pow,
     PowConst,
     ConstPow,
     Max2,
     Max2Const,
     Min2,
     Min2Const,
     Spot,
     Var,
     Const,
     Assign,
     AssignConst,
     Pays,
     PaysConst,
     If,
     IfElse,
     Equal,
     Sup,
     SupEqual,
     And,
     Or,
     Smooth,
     Sqrt,
     Log,
     Not,
     Uminus,
     True,
     False,
     ConstVar,
     _N_VALUES
    } val_;
      
    NodeType_(Value_ val) : val_(val) {
        REQUIRE(val < Value_::_N_VALUES, "val is not valid");
    }
private:
    friend bool operator==(const NodeType_& lhs, const NodeType_& rhs);
    friend struct ReadStringNodeType_;
    friend Vector_<NodeType_> NodeTypeListAll();
    friend bool operator<(const NodeType_& lhs, const NodeType_& rhs) {
        return lhs.val_ < rhs.val_;
    }
public:
    explicit NodeType_(const String_& src);
    const char* String() const;
    Value_ Switch() const {return val_;}
    NodeType_() : val_(Value_::_NOT_SET) {};
};

Vector_<NodeType_> NodeTypeListAll();

bool operator==(const NodeType_& lhs, const NodeType_& rhs);
inline bool operator!=(const NodeType_& lhs, const NodeType_& rhs) {return !(lhs == rhs);}
inline bool operator==(const NodeType_& lhs, NodeType_::Value_ rhs) {return lhs.Switch() == rhs;}
inline bool operator!=(const NodeType_& lhs, NodeType_::Value_ rhs) {return lhs.Switch() != rhs;}

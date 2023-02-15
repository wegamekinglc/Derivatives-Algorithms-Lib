
bool operator==(const NodeType_& lhs, const NodeType_& rhs) {return lhs.val_ == rhs.val_;}
namespace {
    Vector_<NodeType_>& TheNodeTypeList() {
        RETURN_STATIC(Vector_<NodeType_>);
    }
}    // leave local

Vector_<NodeType_> NodeTypeListAll() {
   if (TheNodeTypeList().empty()) {
        TheNodeTypeList().emplace_back(NodeType_::Value_::Add);
        TheNodeTypeList().emplace_back(NodeType_::Value_::AddConst);
        TheNodeTypeList().emplace_back(NodeType_::Value_::Sub);
        TheNodeTypeList().emplace_back(NodeType_::Value_::SubConst);
        TheNodeTypeList().emplace_back(NodeType_::Value_::ConstSub);
        TheNodeTypeList().emplace_back(NodeType_::Value_::Mult);
        TheNodeTypeList().emplace_back(NodeType_::Value_::MultConst);
        TheNodeTypeList().emplace_back(NodeType_::Value_::Div);
        TheNodeTypeList().emplace_back(NodeType_::Value_::DivConst);
        TheNodeTypeList().emplace_back(NodeType_::Value_::ConstDiv);
        TheNodeTypeList().emplace_back(NodeType_::Value_::Pow);
        TheNodeTypeList().emplace_back(NodeType_::Value_::PowConst);
        TheNodeTypeList().emplace_back(NodeType_::Value_::ConstPow);
        TheNodeTypeList().emplace_back(NodeType_::Value_::Max2);
        TheNodeTypeList().emplace_back(NodeType_::Value_::Max2Const);
        TheNodeTypeList().emplace_back(NodeType_::Value_::Min2);
        TheNodeTypeList().emplace_back(NodeType_::Value_::Min2Const);
        TheNodeTypeList().emplace_back(NodeType_::Value_::Spot);
        TheNodeTypeList().emplace_back(NodeType_::Value_::Var);
        TheNodeTypeList().emplace_back(NodeType_::Value_::Const);
        TheNodeTypeList().emplace_back(NodeType_::Value_::Assign);
        TheNodeTypeList().emplace_back(NodeType_::Value_::AssignConst);
        TheNodeTypeList().emplace_back(NodeType_::Value_::Pays);
        TheNodeTypeList().emplace_back(NodeType_::Value_::PaysConst);
        TheNodeTypeList().emplace_back(NodeType_::Value_::If);
        TheNodeTypeList().emplace_back(NodeType_::Value_::IfElse);
        TheNodeTypeList().emplace_back(NodeType_::Value_::Equal);
        TheNodeTypeList().emplace_back(NodeType_::Value_::Sup);
        TheNodeTypeList().emplace_back(NodeType_::Value_::SupEqual);
        TheNodeTypeList().emplace_back(NodeType_::Value_::And);
        TheNodeTypeList().emplace_back(NodeType_::Value_::Or);
        TheNodeTypeList().emplace_back(NodeType_::Value_::Smooth);
        TheNodeTypeList().emplace_back(NodeType_::Value_::Sqrt);
        TheNodeTypeList().emplace_back(NodeType_::Value_::Log);
        TheNodeTypeList().emplace_back(NodeType_::Value_::Not);
        TheNodeTypeList().emplace_back(NodeType_::Value_::Uminus);
        TheNodeTypeList().emplace_back(NodeType_::Value_::True);
        TheNodeTypeList().emplace_back(NodeType_::Value_::False);
   }
   return TheNodeTypeList();
}


const char* NodeType_::String() const {
    switch (val_)
    {
    default:
    case Value_::_NOT_SET:
        return 0;
        case Value_::Add:
        return "Add";
    case Value_::AddConst:
        return "AddConst";
    case Value_::Sub:
        return "Sub";
    case Value_::SubConst:
        return "SubConst";
    case Value_::ConstSub:
        return "ConstSub";
    case Value_::Mult:
        return "Mult";
    case Value_::MultConst:
        return "MultConst";
    case Value_::Div:
        return "Div";
    case Value_::DivConst:
        return "DivConst";
    case Value_::ConstDiv:
        return "ConstDiv";
    case Value_::Pow:
        return "Pow";
    case Value_::PowConst:
        return "PowConst";
    case Value_::ConstPow:
        return "ConstPow";
    case Value_::Max2:
        return "Max2";
    case Value_::Max2Const:
        return "Max2Const";
    case Value_::Min2:
        return "Min2";
    case Value_::Min2Const:
        return "Min2Const";
    case Value_::Spot:
        return "Spot";
    case Value_::Var:
        return "Var";
    case Value_::Const:
        return "Const";
    case Value_::Assign:
        return "Assign";
    case Value_::AssignConst:
        return "AssignConst";
    case Value_::Pays:
        return "Pays";
    case Value_::PaysConst:
        return "PaysConst";
    case Value_::If:
        return "If";
    case Value_::IfElse:
        return "IfElse";
    case Value_::Equal:
        return "Equal";
    case Value_::Sup:
        return "Sup";
    case Value_::SupEqual:
        return "SupEqual";
    case Value_::And:
        return "And";
    case Value_::Or:
        return "Or";
    case Value_::Smooth:
        return "Smooth";
    case Value_::Sqrt:
        return "Sqrt";
    case Value_::Log:
        return "Log";
    case Value_::Not:
        return "Not";
    case Value_::Uminus:
        return "Uminus";
    case Value_::True:
        return "True";
    case Value_::False:
        return "False";
        
    }}

struct ReadStringNodeType_
{
    ReadStringNodeType_() {}

    bool operator()(const String_& src, NodeType_::Value_* val) const    // returns true iff recognized input
    {
        bool ret_val = true;
        if (0);	// otiose code to allow regular else-if structure
        else if (src.empty())
        { ret_val = false; }
        
	else if (String::Equivalent(src, "ADD"))
        *val = NodeType_::Value_::Add;

	else if (String::Equivalent(src, "ADDCONST"))
        *val = NodeType_::Value_::AddConst;

	else if (String::Equivalent(src, "SUB"))
        *val = NodeType_::Value_::Sub;

	else if (String::Equivalent(src, "SUBCONST"))
        *val = NodeType_::Value_::SubConst;

	else if (String::Equivalent(src, "CONSTSUB"))
        *val = NodeType_::Value_::ConstSub;

	else if (String::Equivalent(src, "MULT"))
        *val = NodeType_::Value_::Mult;

	else if (String::Equivalent(src, "MULTCONST"))
        *val = NodeType_::Value_::MultConst;

	else if (String::Equivalent(src, "DIV"))
        *val = NodeType_::Value_::Div;

	else if (String::Equivalent(src, "DIVCONST"))
        *val = NodeType_::Value_::DivConst;

	else if (String::Equivalent(src, "CONSTDIV"))
        *val = NodeType_::Value_::ConstDiv;

	else if (String::Equivalent(src, "POW"))
        *val = NodeType_::Value_::Pow;

	else if (String::Equivalent(src, "POWCONST"))
        *val = NodeType_::Value_::PowConst;

	else if (String::Equivalent(src, "CONSTPOW"))
        *val = NodeType_::Value_::ConstPow;

	else if (String::Equivalent(src, "MAX2"))
        *val = NodeType_::Value_::Max2;

	else if (String::Equivalent(src, "MAX2CONST"))
        *val = NodeType_::Value_::Max2Const;

	else if (String::Equivalent(src, "MIN2"))
        *val = NodeType_::Value_::Min2;

	else if (String::Equivalent(src, "MIN2CONST"))
        *val = NodeType_::Value_::Min2Const;

	else if (String::Equivalent(src, "SPOT"))
        *val = NodeType_::Value_::Spot;

	else if (String::Equivalent(src, "VAR"))
        *val = NodeType_::Value_::Var;

	else if (String::Equivalent(src, "CONST"))
        *val = NodeType_::Value_::Const;

	else if (String::Equivalent(src, "ASSIGN"))
        *val = NodeType_::Value_::Assign;

	else if (String::Equivalent(src, "ASSIGNCONST"))
        *val = NodeType_::Value_::AssignConst;

	else if (String::Equivalent(src, "PAYS"))
        *val = NodeType_::Value_::Pays;

	else if (String::Equivalent(src, "PAYSCONST"))
        *val = NodeType_::Value_::PaysConst;

	else if (String::Equivalent(src, "IF"))
        *val = NodeType_::Value_::If;

	else if (String::Equivalent(src, "IFELSE"))
        *val = NodeType_::Value_::IfElse;

	else if (String::Equivalent(src, "EQUAL"))
        *val = NodeType_::Value_::Equal;

	else if (String::Equivalent(src, "SUP"))
        *val = NodeType_::Value_::Sup;

	else if (String::Equivalent(src, "SUPEQUAL"))
        *val = NodeType_::Value_::SupEqual;

	else if (String::Equivalent(src, "AND"))
        *val = NodeType_::Value_::And;

	else if (String::Equivalent(src, "OR"))
        *val = NodeType_::Value_::Or;

	else if (String::Equivalent(src, "SMOOTH"))
        *val = NodeType_::Value_::Smooth;

	else if (String::Equivalent(src, "SQRT"))
        *val = NodeType_::Value_::Sqrt;

	else if (String::Equivalent(src, "LOG"))
        *val = NodeType_::Value_::Log;

	else if (String::Equivalent(src, "NOT"))
        *val = NodeType_::Value_::Not;

	else if (String::Equivalent(src, "UMINUS"))
        *val = NodeType_::Value_::Uminus;

	else if (String::Equivalent(src, "TRUE"))
        *val = NodeType_::Value_::True;

	else if (String::Equivalent(src, "FALSE"))
        *val = NodeType_::Value_::False;
        else
            ret_val = false;
        return ret_val;
    }
};

NodeType_::NodeType_(const String_& src) {
    static const ReadStringNodeType_ READ_FIXED;    // allows precomputation for speed, in constructor
    if (READ_FIXED(src, &val_))
        return;
    THROW("'" + src + "' is not a recognizable NodeType");
}




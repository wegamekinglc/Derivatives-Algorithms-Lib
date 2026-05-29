
package types;

public class NodeType
{
    public enum Value
    {
		ADD,
		ADDCONST,
		SUB,
		SUBCONST,
		CONSTSUB,
		MULT,
		MULTCONST,
		DIV,
		DIVCONST,
		CONSTDIV,
		POW,
		POWCONST,
		CONSTPOW,
		MAX2,
		MAX2CONST,
		MIN2,
		MIN2CONST,
		SPOT,
		VAR,
		CONST,
		ASSIGN,
		ASSIGNCONST,
		PAYS,
		PAYSCONST,
		IF,
		IFELSE,
		EQUAL,
		SUP,
		SUPEQUAL,
		AND,
		OR,
		SMOOTH,
		SQRT,
		LOG,
		NOT,
		UMINUS,
		TRUE,
		FALSE,
		CONSTVAR,
        N_VALUES
    }
}

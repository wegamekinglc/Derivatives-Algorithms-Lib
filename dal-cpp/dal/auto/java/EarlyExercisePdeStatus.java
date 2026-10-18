
package types;

public class EarlyExercisePdeStatus
{
    public enum Value
    {
		SUCCESS,
		MAXITERATIONS,
		LCPRESIDUALLIMIT,
		LINEARSOLVEFAILURE,
		NONMONOTONEOPERATOR,
		NONFINITE,
		WORKLIMIT,
        N_VALUES
    }
}

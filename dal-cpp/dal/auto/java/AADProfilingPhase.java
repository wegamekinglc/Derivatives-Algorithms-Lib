
package types;

public class AADProfilingPhase
{
    public enum Value
    {
		PREPARE,
		CALIBRATE,
		WORKERINIT,
		PATHFORWARD,
		PAYOFF,
		REVERSE,
		REVERSESUFFIX,
		REVERSEPREFIX,
		WAIT,
		REDUCE,
		SAMPLETAPE,
		QUOTEMAP,
		LSMTRAIN,
		LSMREGRESS,
		LSMREPLAY,
        N_VALUES
    }
}

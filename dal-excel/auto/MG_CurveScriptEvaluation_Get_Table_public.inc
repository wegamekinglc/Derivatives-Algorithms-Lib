

extern "C" __declspec(dllexport) OPER_* xl_CurveScriptEvaluation_Get_Table
    (const OPER_* xl_outcome, const OPER_* xl_table)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("CurveScriptEvaluation_Get_Table");        
        argName = "outcome (input #1)";
        const Handle_<StorableCalibrationOutcome_> outcome = Excel::ToHandle<StorableCalibrationOutcome_>(_env, xl_outcome);
        argName = "table (input #2)";
        const String_ table = Excel::ToString(xl_table);
        argName = 0;
		Matrix_<Cell_> value;
        CurveScriptEvaluation_Get_Table(outcome, table, &value);
        Excel::Retval_ retval;
        retval.Load(value);
        return retval.ToXloper();
    }
    catch (std::exception& e)
    {
        return Excel::Error(e.what(), argName);
    }
    catch (...)
    {
        return Excel::Error("Unknown error", argName);
    }
}

struct XlRegister_CurveScriptEvaluation_Get_Table_
{
    XlRegister_CurveScriptEvaluation_Get_Table_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Evaluation outcome");
        argHelp.push_back("measures parameters cashflows observations or summary");
        Excel::Register("Base", "xl_CurveScriptEvaluation_Get_Table", "CURVESCRIPTEVALUATION.GET.TABLE", "Same core compounded OIS interface", "QQQ", "outcome,table", argHelp, false);
    }
};
static XlRegister_CurveScriptEvaluation_Get_Table_ The_CurveScriptEvaluation_Get_Table_XlRegisterer;

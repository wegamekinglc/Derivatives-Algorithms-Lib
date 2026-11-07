

extern "C" __declspec(dllexport) OPER_* xl_CurveScriptEvaluation_Get_Value
    (const OPER_* xl_outcome, const OPER_* xl_measureId)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("CurveScriptEvaluation_Get_Value");        
        argName = "outcome (input #1)";
        const Handle_<StorableCalibrationOutcome_> outcome = Excel::ToHandle<StorableCalibrationOutcome_>(_env, xl_outcome);
        argName = "measureId (input #2)";
        const String_ measureId = Excel::ToString(xl_measureId);
        argName = 0;
		double value;
        CurveScriptEvaluation_Get_Value(outcome, measureId, &value);
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

struct XlRegister_CurveScriptEvaluation_Get_Value_
{
    XlRegister_CurveScriptEvaluation_Get_Value_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Evaluation outcome");
        argHelp.push_back("Requested measure ID");
        Excel::Register("Base", "xl_CurveScriptEvaluation_Get_Value", "CURVESCRIPTEVALUATION.GET.VALUE", "Same core compounded OIS interface", "QQQ", "outcome,measureId", argHelp, false);
    }
};
static XlRegister_CurveScriptEvaluation_Get_Value_ The_CurveScriptEvaluation_Get_Value_XlRegisterer;

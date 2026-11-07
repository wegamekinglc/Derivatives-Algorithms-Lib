

extern "C" __declspec(dllexport) OPER_* xl_CurveScriptEvaluation_Get_NativePartials
    (const OPER_* xl_outcome)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("CurveScriptEvaluation_Get_NativePartials");        
        argName = "outcome (input #1)";
        const Handle_<StorableCalibrationOutcome_> outcome = Excel::ToHandle<StorableCalibrationOutcome_>(_env, xl_outcome);
        argName = 0;
		Matrix_<double> value;
        CurveScriptEvaluation_Get_NativePartials(outcome, &value);
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

struct XlRegister_CurveScriptEvaluation_Get_NativePartials_
{
    XlRegister_CurveScriptEvaluation_Get_NativePartials_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Evaluation outcome");
        Excel::Register("Base", "xl_CurveScriptEvaluation_Get_NativePartials", "CURVESCRIPTEVALUATION.GET.NATIVEPARTIALS", "Same core compounded OIS interface", "QQ", "outcome", argHelp, false);
    }
};
static XlRegister_CurveScriptEvaluation_Get_NativePartials_ The_CurveScriptEvaluation_Get_NativePartials_XlRegisterer;

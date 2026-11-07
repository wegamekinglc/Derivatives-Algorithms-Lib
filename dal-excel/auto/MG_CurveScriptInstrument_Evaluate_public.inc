

extern "C" __declspec(dllexport) OPER_* xl_CurveScriptInstrument_Evaluate
    (const OPER_* xl_instrument, const OPER_* xl_request)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("CurveScriptInstrument_Evaluate");        
        argName = "instrument (input #1)";
        const Handle_<CalibrationInstrument_> instrument = Excel::ToHandle<CalibrationInstrument_>(_env, xl_instrument);
        argName = "request (input #2)";
        const Handle_<StorableCurveScriptEvaluationRequest_> request = Excel::ToHandle<StorableCurveScriptEvaluationRequest_>(_env, xl_request);
        argName = 0;
		Handle_<StorableCalibrationOutcome_> outcome;
        CurveScriptInstrument_Evaluate(instrument, request, &outcome);
        Excel::Retval_ retval;
        retval.Load(_env, outcome);
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

struct XlRegister_CurveScriptInstrument_Evaluate_
{
    XlRegister_CurveScriptInstrument_Evaluate_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Immutable source instrument");
        argHelp.push_back("Explicit native request");
        Excel::Register("Base", "xl_CurveScriptInstrument_Evaluate", "CURVESCRIPTINSTRUMENT.EVALUATE", "Same core compounded OIS interface", "QQQ", "instrument,request", argHelp, false);
    }
};
static XlRegister_CurveScriptInstrument_Evaluate_ The_CurveScriptInstrument_Evaluate_XlRegisterer;

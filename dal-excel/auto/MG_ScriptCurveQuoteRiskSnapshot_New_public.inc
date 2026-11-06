

extern "C" __declspec(dllexport) OPER_* xl_ScriptCurveQuoteRiskSnapshot_New
    (const OPER_* xl_calibration, const OPER_* xl_accuracy)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("ScriptCurveQuoteRiskSnapshot_New");        
        argName = "calibration (input #1)";
        const Handle_<StorableCalibrationOutcome_> calibration = Excel::ToHandle<StorableCalibrationOutcome_>(_env, xl_calibration);
        argName = "accuracy (input #2)";
        const Handle_<StorableScriptCurveQuoteRiskAccuracy_> accuracy = Excel::ToHandle<StorableScriptCurveQuoteRiskAccuracy_>(_env, xl_accuracy);
        argName = 0;
		Handle_<StorableScriptCurveQuoteRiskSnapshot_> value;
        ScriptCurveQuoteRiskSnapshot_New(calibration, accuracy, &value);
        Excel::Retval_ retval;
        retval.Load(_env, value);
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

struct XlRegister_ScriptCurveQuoteRiskSnapshot_New_
{
    XlRegister_ScriptCurveQuoteRiskSnapshot_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Complete curve fit outcome");
        argHelp.push_back("Explicit numerical admission");
        Excel::Register("Base", "xl_ScriptCurveQuoteRiskSnapshot_New", "SCRIPTCURVEQUOTERISKSNAPSHOT.NEW", "Capture an owning script curve quote risk snapshot", "QQQ", "calibration,accuracy", argHelp, false);
    }
};
static XlRegister_ScriptCurveQuoteRiskSnapshot_New_ The_ScriptCurveQuoteRiskSnapshot_New_XlRegisterer;



extern "C" __declspec(dllexport) OPER_* xl_ScriptCurveQuoteRiskSnapshot_Get_Diagnostics
    (const OPER_* xl_snapshot)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("ScriptCurveQuoteRiskSnapshot_Get_Diagnostics");        
        argName = "snapshot (input #1)";
        const Handle_<StorableScriptCurveQuoteRiskSnapshot_> snapshot = Excel::ToHandle<StorableScriptCurveQuoteRiskSnapshot_>(_env, xl_snapshot);
        argName = 0;
		Matrix_<Cell_> value;
        ScriptCurveQuoteRiskSnapshot_Get_Diagnostics(snapshot, &value);
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

struct XlRegister_ScriptCurveQuoteRiskSnapshot_Get_Diagnostics_
{
    XlRegister_ScriptCurveQuoteRiskSnapshot_Get_Diagnostics_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Owning risk handle");
        Excel::Register("Base", "xl_ScriptCurveQuoteRiskSnapshot_Get_Diagnostics", "SCRIPTCURVEQUOTERISKSNAPSHOT.GET.DIAGNOSTICS", "Read owning script curve quote risk Diagnostics", "QQ", "snapshot", argHelp, false);
    }
};
static XlRegister_ScriptCurveQuoteRiskSnapshot_Get_Diagnostics_ The_ScriptCurveQuoteRiskSnapshot_Get_Diagnostics_XlRegisterer;

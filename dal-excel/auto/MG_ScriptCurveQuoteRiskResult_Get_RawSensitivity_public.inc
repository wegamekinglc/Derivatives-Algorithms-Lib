

extern "C" __declspec(dllexport) OPER_* xl_ScriptCurveQuoteRiskResult_Get_RawSensitivity
    (const OPER_* xl_result)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("ScriptCurveQuoteRiskResult_Get_RawSensitivity");        
        argName = "result (input #1)";
        const Handle_<StorableScriptCurveQuoteRiskResult_> result = Excel::ToHandle<StorableScriptCurveQuoteRiskResult_>(_env, xl_result);
        argName = 0;
		Vector_<double> value;
        ScriptCurveQuoteRiskResult_Get_RawSensitivity(result, &value);
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

struct XlRegister_ScriptCurveQuoteRiskResult_Get_RawSensitivity_
{
    XlRegister_ScriptCurveQuoteRiskResult_Get_RawSensitivity_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Owning risk handle");
        Excel::Register("Base", "xl_ScriptCurveQuoteRiskResult_Get_RawSensitivity", "SCRIPTCURVEQUOTERISKRESULT.GET.RAWSENSITIVITY", "Read owning script curve quote risk RawSensitivity", "QQ", "result", argHelp, false);
    }
};
static XlRegister_ScriptCurveQuoteRiskResult_Get_RawSensitivity_ The_ScriptCurveQuoteRiskResult_Get_RawSensitivity_XlRegisterer;

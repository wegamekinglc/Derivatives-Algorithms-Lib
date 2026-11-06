

extern "C" __declspec(dllexport) OPER_* xl_ScriptCurveQuoteRiskResult_Get_QuoteAxis
    (const OPER_* xl_result)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("ScriptCurveQuoteRiskResult_Get_QuoteAxis");        
        argName = "result (input #1)";
        const Handle_<StorableScriptCurveQuoteRiskResult_> result = Excel::ToHandle<StorableScriptCurveQuoteRiskResult_>(_env, xl_result);
        argName = 0;
		Matrix_<Cell_> value;
        ScriptCurveQuoteRiskResult_Get_QuoteAxis(result, &value);
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

struct XlRegister_ScriptCurveQuoteRiskResult_Get_QuoteAxis_
{
    XlRegister_ScriptCurveQuoteRiskResult_Get_QuoteAxis_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Owning risk handle");
        Excel::Register("Base", "xl_ScriptCurveQuoteRiskResult_Get_QuoteAxis", "SCRIPTCURVEQUOTERISKRESULT.GET.QUOTEAXIS", "Read owning script curve quote risk QuoteAxis", "QQ", "result", argHelp, false);
    }
};
static XlRegister_ScriptCurveQuoteRiskResult_Get_QuoteAxis_ The_ScriptCurveQuoteRiskResult_Get_QuoteAxis_XlRegisterer;

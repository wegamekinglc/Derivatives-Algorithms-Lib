

extern "C" __declspec(dllexport) OPER_* xl_DupireScriptRiskResult_Get_QuoteRisk
    (const OPER_* xl_result)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("DupireScriptRiskResult_Get_QuoteRisk");        
        argName = "result (input #1)";
        const Handle_<StorableDupireScriptRiskResult_> result = Excel::ToHandle<StorableDupireScriptRiskResult_>(_env, xl_result);
        argName = 0;
		Handle_<StorableCalibrationRiskResult_> quoteRisk;
        DupireScriptRiskResult_Get_QuoteRisk(result, &quoteRisk);
        Excel::Retval_ retval;
        retval.Load(_env, quoteRisk);
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

struct XlRegister_DupireScriptRiskResult_Get_QuoteRisk_
{
    XlRegister_DupireScriptRiskResult_Get_QuoteRisk_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Automatic result");
        Excel::Register("Base", "xl_DupireScriptRiskResult_Get_QuoteRisk", "DUPIRESCRIPTRISKRESULT.GET.QUOTERISK", "Copy the retained requested common quote risk", "QQ", "result", argHelp, false);
    }
};
static XlRegister_DupireScriptRiskResult_Get_QuoteRisk_ The_DupireScriptRiskResult_Get_QuoteRisk_XlRegisterer;

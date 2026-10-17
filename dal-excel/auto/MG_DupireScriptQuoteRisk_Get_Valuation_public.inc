

extern "C" __declspec(dllexport) OPER_* xl_DupireScriptQuoteRisk_Get_Valuation
    (const OPER_* xl_result)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("DupireScriptQuoteRisk_Get_Valuation");        
        argName = "result (input #1)";
        const Handle_<StorableDupireScriptQuoteRisk_> result = Excel::ToHandle<StorableDupireScriptQuoteRisk_>(_env, xl_result);
        argName = 0;
		Handle_<StorableRiskResult_> valuation;
        DupireScriptQuoteRisk_Get_Valuation(result, &valuation);
        Excel::Retval_ retval;
        retval.Load(_env, valuation);
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

struct XlRegister_DupireScriptQuoteRisk_Get_Valuation_
{
    XlRegister_DupireScriptQuoteRisk_Get_Valuation_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Completed valuation and quote risk");
        Excel::Register("Base", "xl_DupireScriptQuoteRisk_Get_Valuation", "DUPIRESCRIPTQUOTERISK.GET.VALUATION", "Copy the retained scalar valuation result", "QQ", "result", argHelp, false);
    }
};
static XlRegister_DupireScriptQuoteRisk_Get_Valuation_ The_DupireScriptQuoteRisk_Get_Valuation_XlRegisterer;



extern "C" __declspec(dllexport) OPER_* xl_DupireScriptQuoteRisk_Get_QuoteRisk
    (const OPER_* xl_result)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("DupireScriptQuoteRisk_Get_QuoteRisk");        
        argName = "result (input #1)";
        const Handle_<StorableDupireScriptQuoteRisk_> result = Excel::ToHandle<StorableDupireScriptQuoteRisk_>(_env, xl_result);
        argName = 0;
		Handle_<StorableDupireQuoteRisk_> quoteRisk;
        DupireScriptQuoteRisk_Get_QuoteRisk(result, &quoteRisk);
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

struct XlRegister_DupireScriptQuoteRisk_Get_QuoteRisk_
{
    XlRegister_DupireScriptQuoteRisk_Get_QuoteRisk_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Completed valuation and quote risk");
        Excel::Register("Base", "xl_DupireScriptQuoteRisk_Get_QuoteRisk", "DUPIRESCRIPTQUOTERISK.GET.QUOTERISK", "Copy retained spread quote risk", "QQ", "result", argHelp, false);
    }
};
static XlRegister_DupireScriptQuoteRisk_Get_QuoteRisk_ The_DupireScriptQuoteRisk_Get_QuoteRisk_XlRegisterer;

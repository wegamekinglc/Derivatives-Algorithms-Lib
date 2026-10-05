

extern "C" __declspec(dllexport) OPER_* xl_DupireScriptQuoteRisk_Get_Provenance
    (const OPER_* xl_result)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("DupireScriptQuoteRisk_Get_Provenance");        
        argName = "result (input #1)";
        const Handle_<StorableDupireScriptQuoteRisk_> result = Excel::ToHandle<StorableDupireScriptQuoteRisk_>(_env, xl_result);
        argName = 0;
		Matrix_<Cell_> provenance;
        DupireScriptQuoteRisk_Get_Provenance(result, &provenance);
        Excel::Retval_ retval;
        retval.Load(provenance);
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

struct XlRegister_DupireScriptQuoteRisk_Get_Provenance_
{
    XlRegister_DupireScriptQuoteRisk_Get_Provenance_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Completed valuation and quote risk");
        Excel::Register("Base", "xl_DupireScriptQuoteRisk_Get_Provenance", "DUPIRESCRIPTQUOTERISK.GET.PROVENANCE", "Extract composite quote-risk method and component", "QQ", "result", argHelp, false);
    }
};
static XlRegister_DupireScriptQuoteRisk_Get_Provenance_ The_DupireScriptQuoteRisk_Get_Provenance_XlRegisterer;

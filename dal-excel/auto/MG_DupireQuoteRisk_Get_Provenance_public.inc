

extern "C" __declspec(dllexport) OPER_* xl_DupireQuoteRisk_Get_Provenance
    (const OPER_* xl_result)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("DupireQuoteRisk_Get_Provenance");        
        argName = "result (input #1)";
        const Handle_<StorableDupireQuoteRisk_> result = Excel::ToHandle<StorableDupireQuoteRisk_>(_env, xl_result);
        argName = 0;
		Matrix_<Cell_> provenance;
        DupireQuoteRisk_Get_Provenance(result, &provenance);
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

struct XlRegister_DupireQuoteRisk_Get_Provenance_
{
    XlRegister_DupireQuoteRisk_Get_Provenance_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Completed passive quote risk");
        Excel::Register("Base", "xl_DupireQuoteRisk_Get_Provenance", "DUPIREQUOTERISK.GET.PROVENANCE", "Extract quote-risk method and units", "QQ", "result", argHelp, false);
    }
};
static XlRegister_DupireQuoteRisk_Get_Provenance_ The_DupireQuoteRisk_Get_Provenance_XlRegisterer;



extern "C" __declspec(dllexport) OPER_* xl_DupireScriptRiskResult_Get_Provenance
    (const OPER_* xl_result)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("DupireScriptRiskResult_Get_Provenance");        
        argName = "result (input #1)";
        const Handle_<StorableDupireScriptRiskResult_> result = Excel::ToHandle<StorableDupireScriptRiskResult_>(_env, xl_result);
        argName = 0;
		Matrix_<Cell_> provenance;
        DupireScriptRiskResult_Get_Provenance(result, &provenance);
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

struct XlRegister_DupireScriptRiskResult_Get_Provenance_
{
    XlRegister_DupireScriptRiskResult_Get_Provenance_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Automatic result");
        Excel::Register("Base", "xl_DupireScriptRiskResult_Get_Provenance", "DUPIRESCRIPTRISKRESULT.GET.PROVENANCE", "Copy automatic result method and retained numeric payload", "QQ", "result", argHelp, false);
    }
};
static XlRegister_DupireScriptRiskResult_Get_Provenance_ The_DupireScriptRiskResult_Get_Provenance_XlRegisterer;

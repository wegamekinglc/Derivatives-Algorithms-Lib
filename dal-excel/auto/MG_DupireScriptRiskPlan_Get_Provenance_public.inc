

extern "C" __declspec(dllexport) OPER_* xl_DupireScriptRiskPlan_Get_Provenance
    (const OPER_* xl_plan)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("DupireScriptRiskPlan_Get_Provenance");        
        argName = "plan (input #1)";
        const Handle_<StorableDupireScriptRiskPlan_> plan = Excel::ToHandle<StorableDupireScriptRiskPlan_>(_env, xl_plan);
        argName = 0;
		Matrix_<Cell_> provenance;
        DupireScriptRiskPlan_Get_Provenance(plan, &provenance);
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

struct XlRegister_DupireScriptRiskPlan_Get_Provenance_
{
    XlRegister_DupireScriptRiskPlan_Get_Provenance_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Sealed plan");
        Excel::Register("Base", "xl_DupireScriptRiskPlan_Get_Provenance", "DUPIRESCRIPTRISKPLAN.GET.PROVENANCE", "Copy passive automatic plan metadata", "QQ", "plan", argHelp, false);
    }
};
static XlRegister_DupireScriptRiskPlan_Get_Provenance_ The_DupireScriptRiskPlan_Get_Provenance_XlRegisterer;

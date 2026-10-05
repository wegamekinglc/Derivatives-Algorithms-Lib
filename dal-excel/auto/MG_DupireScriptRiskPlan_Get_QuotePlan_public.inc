

extern "C" __declspec(dllexport) OPER_* xl_DupireScriptRiskPlan_Get_QuotePlan
    (const OPER_* xl_plan)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("DupireScriptRiskPlan_Get_QuotePlan");        
        argName = "plan (input #1)";
        const Handle_<StorableDupireScriptRiskPlan_> plan = Excel::ToHandle<StorableDupireScriptRiskPlan_>(_env, xl_plan);
        argName = 0;
		Handle_<StorableCalibrationRiskPlan_> quotePlan;
        DupireScriptRiskPlan_Get_QuotePlan(plan, &quotePlan);
        Excel::Retval_ retval;
        retval.Load(_env, quotePlan);
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

struct XlRegister_DupireScriptRiskPlan_Get_QuotePlan_
{
    XlRegister_DupireScriptRiskPlan_Get_QuotePlan_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Sealed plan");
        Excel::Register("Base", "xl_DupireScriptRiskPlan_Get_QuotePlan", "DUPIRESCRIPTRISKPLAN.GET.QUOTEPLAN", "Copy the automatic plan's common quote plan", "QQ", "plan", argHelp, false);
    }
};
static XlRegister_DupireScriptRiskPlan_Get_QuotePlan_ The_DupireScriptRiskPlan_Get_QuotePlan_XlRegisterer;

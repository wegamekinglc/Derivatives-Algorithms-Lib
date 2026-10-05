

extern "C" __declspec(dllexport) OPER_* xl_DupireScriptRiskPlan_Get_Configuration
    (const OPER_* xl_plan, const OPER_* format)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("DupireScriptRiskPlan_Get_Configuration");        
        argName = "plan (input #1)";
        const Handle_<StorableDupireScriptRiskPlan_> plan = Excel::ToHandle<StorableDupireScriptRiskPlan_>(_env, xl_plan);
        argName = 0;
		Handle_<StorableDupireScriptRiskSettings_> settings;
		Matrix_<Cell_> bindings;
        DupireScriptRiskPlan_Get_Configuration(plan, &settings, &bindings);
        Excel::Retval_ retval;
        retval.Load(_env, settings);
        retval.Load(bindings);
        return retval.ToXloper(Excel::ToString(format, true));
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

struct XlRegister_DupireScriptRiskPlan_Get_Configuration_
{
    XlRegister_DupireScriptRiskPlan_Get_Configuration_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Sealed plan");
        Excel::Register("Base", "xl_DupireScriptRiskPlan_Get_Configuration", "DUPIRESCRIPTRISKPLAN.GET.CONFIGURATION", "Copy captured execution settings and direct bindings", "QQQ", "plan,format", argHelp, false);
    }
};
static XlRegister_DupireScriptRiskPlan_Get_Configuration_ The_DupireScriptRiskPlan_Get_Configuration_XlRegisterer;

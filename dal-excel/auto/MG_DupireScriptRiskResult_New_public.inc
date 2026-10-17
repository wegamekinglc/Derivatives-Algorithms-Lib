

extern "C" __declspec(dllexport) OPER_* xl_DupireScriptRiskResult_New
    (const OPER_* xl_name, const OPER_* xl_plan)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("DupireScriptRiskResult_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "name"; Excel::ValidateRiskRequestText(xl_name, "DupireScriptRiskResult_New; name");
        argName = "plan (input #2)";
        const Handle_<StorableDupireScriptRiskPlan_> plan = Excel::ToHandle<StorableDupireScriptRiskPlan_>(_env, xl_plan);
        argName = 0;
		Handle_<StorableDupireScriptRiskResult_> result;
        DupireScriptRiskResult_New(name, plan, &result);
        Excel::Retval_ retval;
        retval.Load(_env, result);
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

struct XlRegister_DupireScriptRiskResult_New_
{
    XlRegister_DupireScriptRiskResult_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Object name");
        argHelp.push_back("Sealed automatic plan");
        Excel::Register("Base", "xl_DupireScriptRiskResult_New", "DUPIRESCRIPTRISKRESULT.NEW", "Execute native AAD valuation and common quote pullback", "QQQ", "name,plan", argHelp, false);
    }
};
static XlRegister_DupireScriptRiskResult_New_ The_DupireScriptRiskResult_New_XlRegisterer;

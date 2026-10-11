

extern "C" __declspec(dllexport) OPER_* xl_DupireScriptCurvaturePlan_Get_BasePlan
    (const OPER_* xl_plan)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("DupireScriptCurvaturePlan_Get_BasePlan");        
        argName = "plan (input #1)";
        const Handle_<StorableDupireScriptCurvaturePlan_> plan = Excel::ToHandle<StorableDupireScriptCurvaturePlan_>(_env, xl_plan);
        argName = 0;
		Handle_<StorableDupireScriptRiskPlan_> basePlan;
        DupireScriptCurvaturePlan_Get_BasePlan(plan, &basePlan);
        Excel::Retval_ retval;
        retval.Load(_env, basePlan);
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

struct XlRegister_DupireScriptCurvaturePlan_Get_BasePlan_
{
    XlRegister_DupireScriptCurvaturePlan_Get_BasePlan_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Admitted curvature plan");
        Excel::Register("Base", "xl_DupireScriptCurvaturePlan_Get_BasePlan", "DUPIRESCRIPTCURVATUREPLAN.GET.BASEPLAN", "Copy the frozen first-order plan retained by Dupire curvature", "QQ", "plan", argHelp, false);
    }
};
static XlRegister_DupireScriptCurvaturePlan_Get_BasePlan_ The_DupireScriptCurvaturePlan_Get_BasePlan_XlRegisterer;



extern "C" __declspec(dllexport) OPER_* xl_DupireScriptCurvaturePlan_Get_Steps
    (const OPER_* xl_plan)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("DupireScriptCurvaturePlan_Get_Steps");        
        argName = "plan (input #1)";
        const Handle_<StorableDupireScriptCurvaturePlan_> plan = Excel::ToHandle<StorableDupireScriptCurvaturePlan_>(_env, xl_plan);
        argName = 0;
		Matrix_<Cell_> steps;
        DupireScriptCurvaturePlan_Get_Steps(plan, &steps);
        Excel::Retval_ retval;
        retval.Load(steps);
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

struct XlRegister_DupireScriptCurvaturePlan_Get_Steps_
{
    XlRegister_DupireScriptCurvaturePlan_Get_Steps_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Admitted curvature plan");
        Excel::Register("Base", "xl_DupireScriptCurvaturePlan_Get_Steps", "DUPIRESCRIPTCURVATUREPLAN.GET.STEPS", "Copy admitted Dupire finite-step sizes", "QQ", "plan", argHelp, false);
    }
};
static XlRegister_DupireScriptCurvaturePlan_Get_Steps_ The_DupireScriptCurvaturePlan_Get_Steps_XlRegisterer;

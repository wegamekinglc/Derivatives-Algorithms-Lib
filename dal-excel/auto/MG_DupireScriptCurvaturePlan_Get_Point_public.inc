

extern "C" __declspec(dllexport) OPER_* xl_DupireScriptCurvaturePlan_Get_Point
    (const OPER_* xl_plan)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("DupireScriptCurvaturePlan_Get_Point");        
        argName = "plan (input #1)";
        const Handle_<StorableDupireScriptCurvaturePlan_> plan = Excel::ToHandle<StorableDupireScriptCurvaturePlan_>(_env, xl_plan);
        argName = 0;
		Matrix_<Cell_> point;
        DupireScriptCurvaturePlan_Get_Point(plan, &point);
        Excel::Retval_ retval;
        retval.Load(point);
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

struct XlRegister_DupireScriptCurvaturePlan_Get_Point_
{
    XlRegister_DupireScriptCurvaturePlan_Get_Point_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Admitted curvature plan");
        Excel::Register("Base", "xl_DupireScriptCurvaturePlan_Get_Point", "DUPIRESCRIPTCURVATUREPLAN.GET.POINT", "Copy the frozen full raw Dupire quote point", "QQ", "plan", argHelp, false);
    }
};
static XlRegister_DupireScriptCurvaturePlan_Get_Point_ The_DupireScriptCurvaturePlan_Get_Point_XlRegisterer;

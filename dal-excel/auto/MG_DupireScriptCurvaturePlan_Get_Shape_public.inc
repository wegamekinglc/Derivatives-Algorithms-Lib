

extern "C" __declspec(dllexport) OPER_* xl_DupireScriptCurvaturePlan_Get_Shape
    (const OPER_* xl_plan)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("DupireScriptCurvaturePlan_Get_Shape");        
        argName = "plan (input #1)";
        const Handle_<StorableDupireScriptCurvaturePlan_> plan = Excel::ToHandle<StorableDupireScriptCurvaturePlan_>(_env, xl_plan);
        argName = 0;
		Matrix_<Cell_> shape;
        DupireScriptCurvaturePlan_Get_Shape(plan, &shape);
        Excel::Retval_ retval;
        retval.Load(shape);
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

struct XlRegister_DupireScriptCurvaturePlan_Get_Shape_
{
    XlRegister_DupireScriptCurvaturePlan_Get_Shape_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Admitted curvature plan");
        Excel::Register("Base", "xl_DupireScriptCurvaturePlan_Get_Shape", "DUPIRESCRIPTCURVATUREPLAN.GET.SHAPE", "Copy logical Dupire curvature direction and quote counts", "QQ", "plan", argHelp, false);
    }
};
static XlRegister_DupireScriptCurvaturePlan_Get_Shape_ The_DupireScriptCurvaturePlan_Get_Shape_XlRegisterer;

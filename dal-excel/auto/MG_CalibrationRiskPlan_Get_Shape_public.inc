

extern "C" __declspec(dllexport) OPER_* xl_CalibrationRiskPlan_Get_Shape
    (const OPER_* xl_plan)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("CalibrationRiskPlan_Get_Shape");        
        argName = "plan (input #1)";
        const Handle_<StorableCalibrationRiskPlan_> plan = Excel::ToHandle<StorableCalibrationRiskPlan_>(_env, xl_plan);
        argName = 0;
		Matrix_<Cell_> shape;
        CalibrationRiskPlan_Get_Shape(plan, &shape);
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

struct XlRegister_CalibrationRiskPlan_Get_Shape_
{
    XlRegister_CalibrationRiskPlan_Get_Shape_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Quote plan");
        Excel::Register("Base", "xl_CalibrationRiskPlan_Get_Shape", "CALIBRATIONRISKPLAN.GET.SHAPE", "Copy exact quote projection shape and retained payload", "QQ", "plan", argHelp, false);
    }
};
static XlRegister_CalibrationRiskPlan_Get_Shape_ The_CalibrationRiskPlan_Get_Shape_XlRegisterer;

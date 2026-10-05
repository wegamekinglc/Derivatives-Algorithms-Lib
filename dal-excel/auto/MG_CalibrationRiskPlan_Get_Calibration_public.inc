

extern "C" __declspec(dllexport) OPER_* xl_CalibrationRiskPlan_Get_Calibration
    (const OPER_* xl_plan)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("CalibrationRiskPlan_Get_Calibration");        
        argName = "plan (input #1)";
        const Handle_<StorableCalibrationRiskPlan_> plan = Excel::ToHandle<StorableCalibrationRiskPlan_>(_env, xl_plan);
        argName = 0;
		Handle_<StorableCalibrationPullback_> calibration;
        CalibrationRiskPlan_Get_Calibration(plan, &calibration);
        Excel::Retval_ retval;
        retval.Load(_env, calibration);
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

struct XlRegister_CalibrationRiskPlan_Get_Calibration_
{
    XlRegister_CalibrationRiskPlan_Get_Calibration_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Quote plan");
        Excel::Register("Base", "xl_CalibrationRiskPlan_Get_Calibration", "CALIBRATIONRISKPLAN.GET.CALIBRATION", "Copy a plan's common boundary", "QQ", "plan", argHelp, false);
    }
};
static XlRegister_CalibrationRiskPlan_Get_Calibration_ The_CalibrationRiskPlan_Get_Calibration_XlRegisterer;

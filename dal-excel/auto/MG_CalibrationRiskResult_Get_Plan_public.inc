

extern "C" __declspec(dllexport) OPER_* xl_CalibrationRiskResult_Get_Plan
    (const OPER_* xl_result)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("CalibrationRiskResult_Get_Plan");        
        argName = "result (input #1)";
        const Handle_<StorableCalibrationRiskResult_> result = Excel::ToHandle<StorableCalibrationRiskResult_>(_env, xl_result);
        argName = 0;
		Handle_<StorableCalibrationRiskPlan_> plan;
        CalibrationRiskResult_Get_Plan(result, &plan);
        Excel::Retval_ retval;
        retval.Load(_env, plan);
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

struct XlRegister_CalibrationRiskResult_Get_Plan_
{
    XlRegister_CalibrationRiskResult_Get_Plan_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Requested quote result");
        Excel::Register("Base", "xl_CalibrationRiskResult_Get_Plan", "CALIBRATIONRISKRESULT.GET.PLAN", "Copy the result's quote plan", "QQ", "result", argHelp, false);
    }
};
static XlRegister_CalibrationRiskResult_Get_Plan_ The_CalibrationRiskResult_Get_Plan_XlRegisterer;

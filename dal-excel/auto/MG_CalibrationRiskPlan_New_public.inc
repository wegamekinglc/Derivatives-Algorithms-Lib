

extern "C" __declspec(dllexport) OPER_* xl_CalibrationRiskPlan_New
    (const OPER_* xl_name, const OPER_* xl_calibration, const OPER_* xl_request)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("CalibrationRiskPlan_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "name"; Excel::ValidateRiskRequestText(xl_name, "CalibrationRiskPlan_New; name");
        argName = "calibration (input #2)";
        const Handle_<StorableCalibrationPullback_> calibration = Excel::ToHandle<StorableCalibrationPullback_>(_env, xl_calibration);
        xl_request = Excel::ScriptScalarInput(xl_request);
        argName = "request (input #3)";
        const Handle_<StorableCalibrationRiskRequest_> request = Excel::ToHandle<StorableCalibrationRiskRequest_>(_env, xl_request, true);
        argName = 0;
		Handle_<StorableCalibrationRiskPlan_> plan;
        CalibrationRiskPlan_New(name, calibration, request, &plan);
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

struct XlRegister_CalibrationRiskPlan_New_
{
    XlRegister_CalibrationRiskPlan_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Object name");
        argHelp.push_back("Common calibration boundary");
        argHelp.push_back("Blank selects all quotes");
        Excel::Register("Base", "xl_CalibrationRiskPlan_New", "CALIBRATIONRISKPLAN.NEW", "Plan canonical quote selection and numeric payload", "QQQQ", "name,calibration,[request]", argHelp, false);
    }
};
static XlRegister_CalibrationRiskPlan_New_ The_CalibrationRiskPlan_New_XlRegisterer;

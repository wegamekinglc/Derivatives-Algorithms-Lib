

extern "C" __declspec(dllexport) OPER_* xl_CalibrationRiskResult_New
    (const OPER_* xl_name, const OPER_* xl_plan, const OPER_* xl_parameters, const OPER_* xl_direct)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("CalibrationRiskResult_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "name"; Excel::ValidateRiskRequestText(xl_name, "CalibrationRiskResult_New; name");
        argName = "plan (input #2)";
        const Handle_<StorableCalibrationRiskPlan_> plan = Excel::ToHandle<StorableCalibrationRiskPlan_>(_env, xl_plan);
        argName = "parameters (input #3)";
        const Handle_<StorableCalibrationParameterAdjoints_> parameters = Excel::ToHandle<StorableCalibrationParameterAdjoints_>(_env, xl_parameters);
        xl_direct = Excel::ScriptScalarInput(xl_direct);
        argName = "direct (input #4)";
        const Handle_<StorableCalibrationDirectQuoteAdjoints_> direct = Excel::ToHandle<StorableCalibrationDirectQuoteAdjoints_>(_env, xl_direct, true);
        argName = 0;
		Handle_<StorableCalibrationRiskResult_> result;
        CalibrationRiskResult_New(name, plan, parameters, direct, &result);
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

struct XlRegister_CalibrationRiskResult_New_
{
    XlRegister_CalibrationRiskResult_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Object name");
        argHelp.push_back("Quote plan");
        argHelp.push_back("Canonical parameter seeds");
        argHelp.push_back("Blank omits direct quote seeds");
        Excel::Register("Base", "xl_CalibrationRiskResult_New", "CALIBRATIONRISKRESULT.NEW", "Execute the native calibration pullback for a quote plan", "QQQQQ", "name,plan,parameters,[direct]", argHelp, false);
    }
};
static XlRegister_CalibrationRiskResult_New_ The_CalibrationRiskResult_New_XlRegisterer;

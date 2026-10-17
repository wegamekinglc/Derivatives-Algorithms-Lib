

extern "C" __declspec(dllexport) OPER_* xl_CalibrationRiskResult_Get_Jacobian
    (const OPER_* xl_result, const OPER_* xl_projection)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("CalibrationRiskResult_Get_Jacobian");        
        argName = "result (input #1)";
        const Handle_<StorableCalibrationRiskResult_> result = Excel::ToHandle<StorableCalibrationRiskResult_>(_env, xl_result);
        argName = "projection"; Excel::ValidateRiskRequestText(xl_projection, "CalibrationRiskResult_Get_Jacobian; projection"); xl_projection = Excel::ScriptScalarInput(xl_projection);
        argName = "projection (input #2)";
        const String_ projection = Excel::ToString(xl_projection, true);
        argName = 0;
		Matrix_<Cell_> jacobian;
        CalibrationRiskResult_Get_Jacobian(result, projection, &jacobian);
        Excel::Retval_ retval;
        retval.Load(jacobian);
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

struct XlRegister_CalibrationRiskResult_Get_Jacobian_
{
    XlRegister_CalibrationRiskResult_Get_Jacobian_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Requested quote result");
        argHelp.push_back("Total (default), Calibration, Direct or Reported");
        Excel::Register("Base", "xl_CalibrationRiskResult_Get_Jacobian", "CALIBRATIONRISKRESULT.GET.JACOBIAN", "Copy a selected raw or reported quote row", "QQQ", "result,[projection]", argHelp, false);
    }
};
static XlRegister_CalibrationRiskResult_Get_Jacobian_ The_CalibrationRiskResult_Get_Jacobian_XlRegisterer;

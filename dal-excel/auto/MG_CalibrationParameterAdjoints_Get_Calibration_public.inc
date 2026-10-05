

extern "C" __declspec(dllexport) OPER_* xl_CalibrationParameterAdjoints_Get_Calibration
    (const OPER_* xl_parameters)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("CalibrationParameterAdjoints_Get_Calibration");        
        argName = "parameters (input #1)";
        const Handle_<StorableCalibrationParameterAdjoints_> parameters = Excel::ToHandle<StorableCalibrationParameterAdjoints_>(_env, xl_parameters);
        argName = 0;
		Handle_<StorableCalibrationPullback_> calibration;
        CalibrationParameterAdjoints_Get_Calibration(parameters, &calibration);
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

struct XlRegister_CalibrationParameterAdjoints_Get_Calibration_
{
    XlRegister_CalibrationParameterAdjoints_Get_Calibration_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Owned parameter seeds");
        Excel::Register("Base", "xl_CalibrationParameterAdjoints_Get_Calibration", "CALIBRATIONPARAMETERADJOINTS.GET.CALIBRATION", "Project the owning boundary of parameter seeds", "QQ", "parameters", argHelp, false);
    }
};
static XlRegister_CalibrationParameterAdjoints_Get_Calibration_ The_CalibrationParameterAdjoints_Get_Calibration_XlRegisterer;

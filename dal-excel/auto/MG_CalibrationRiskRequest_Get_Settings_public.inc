

extern "C" __declspec(dllexport) OPER_* xl_CalibrationRiskRequest_Get_Settings
    (const OPER_* xl_request)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("CalibrationRiskRequest_Get_Settings");        
        argName = "request (input #1)";
        const Handle_<StorableCalibrationRiskRequest_> request = Excel::ToHandle<StorableCalibrationRiskRequest_>(_env, xl_request);
        argName = 0;
		Matrix_<Cell_> settings;
        CalibrationRiskRequest_Get_Settings(request, &settings);
        Excel::Retval_ retval;
        retval.Load(settings);
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

struct XlRegister_CalibrationRiskRequest_Get_Settings_
{
    XlRegister_CalibrationRiskRequest_Get_Settings_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Quote request");
        Excel::Register("Base", "xl_CalibrationRiskRequest_Get_Settings", "CALIBRATIONRISKREQUEST.GET.SETTINGS", "Copy configured quote request rows", "QQ", "request", argHelp, false);
    }
};
static XlRegister_CalibrationRiskRequest_Get_Settings_ The_CalibrationRiskRequest_Get_Settings_XlRegisterer;



extern "C" __declspec(dllexport) OPER_* xl_DupireCalibration_Get_Surface
    (const OPER_* xl_calibration)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("DupireCalibration_Get_Surface");        
        argName = "calibration (input #1)";
        const Handle_<StorableDupireCalibration_> calibration = Excel::ToHandle<StorableDupireCalibration_>(_env, xl_calibration);
        argName = 0;
		Handle_<LocalVolSurfaceData_> surface;
        DupireCalibration_Get_Surface(calibration, &surface);
        Excel::Retval_ retval;
        retval.Load(_env, surface);
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

struct XlRegister_DupireCalibration_Get_Surface_
{
    XlRegister_DupireCalibration_Get_Surface_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Frozen calibration");
        Excel::Register("Base", "xl_DupireCalibration_Get_Surface", "DUPIRECALIBRATION.GET.SURFACE", "Copy the calibrated local-vol surface", "QQ", "calibration", argHelp, false);
    }
};
static XlRegister_DupireCalibration_Get_Surface_ The_DupireCalibration_Get_Surface_XlRegisterer;

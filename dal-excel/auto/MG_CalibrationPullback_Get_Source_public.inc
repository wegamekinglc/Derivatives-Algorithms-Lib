

extern "C" __declspec(dllexport) OPER_* xl_CalibrationPullback_Get_Source
    (const OPER_* xl_calibration)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("CalibrationPullback_Get_Source");        
        argName = "calibration (input #1)";
        const Handle_<StorableCalibrationPullback_> calibration = Excel::ToHandle<StorableCalibrationPullback_>(_env, xl_calibration);
        argName = 0;
		Handle_<Storable_> source;
        CalibrationPullback_Get_Source(calibration, &source);
        Excel::Retval_ retval;
        retval.Load(_env, source);
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

struct XlRegister_CalibrationPullback_Get_Source_
{
    XlRegister_CalibrationPullback_Get_Source_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Common boundary");
        Excel::Register("Base", "xl_CalibrationPullback_Get_Source", "CALIBRATIONPULLBACK.GET.SOURCE", "Project the owning typed source without recalibration", "QQ", "calibration", argHelp, false);
    }
};
static XlRegister_CalibrationPullback_Get_Source_ The_CalibrationPullback_Get_Source_XlRegisterer;

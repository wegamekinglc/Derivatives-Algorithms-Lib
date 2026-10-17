

extern "C" __declspec(dllexport) OPER_* xl_CalibrationPullback_Get_Provenance
    (const OPER_* xl_calibration)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("CalibrationPullback_Get_Provenance");        
        argName = "calibration (input #1)";
        const Handle_<StorableCalibrationPullback_> calibration = Excel::ToHandle<StorableCalibrationPullback_>(_env, xl_calibration);
        argName = 0;
		Matrix_<Cell_> provenance;
        CalibrationPullback_Get_Provenance(calibration, &provenance);
        Excel::Retval_ retval;
        retval.Load(provenance);
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

struct XlRegister_CalibrationPullback_Get_Provenance_
{
    XlRegister_CalibrationPullback_Get_Provenance_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Common boundary");
        Excel::Register("Base", "xl_CalibrationPullback_Get_Provenance", "CALIBRATIONPULLBACK.GET.PROVENANCE", "Project common method, units, dimensions and source identity metadata", "QQ", "calibration", argHelp, false);
    }
};
static XlRegister_CalibrationPullback_Get_Provenance_ The_CalibrationPullback_Get_Provenance_XlRegisterer;

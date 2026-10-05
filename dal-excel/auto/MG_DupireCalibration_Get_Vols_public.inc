

extern "C" __declspec(dllexport) OPER_* xl_DupireCalibration_Get_Vols
    (const OPER_* xl_calibration)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("DupireCalibration_Get_Vols");        
        argName = "calibration (input #1)";
        const Handle_<StorableDupireCalibration_> calibration = Excel::ToHandle<StorableDupireCalibration_>(_env, xl_calibration);
        argName = 0;
		Matrix_<double> vols;
        DupireCalibration_Get_Vols(calibration, &vols);
        Excel::Retval_ retval;
        retval.Load(vols);
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

struct XlRegister_DupireCalibration_Get_Vols_
{
    XlRegister_DupireCalibration_Get_Vols_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Frozen calibration");
        Excel::Register("Base", "xl_DupireCalibration_Get_Vols", "DUPIRECALIBRATION.GET.VOLS", "Copy calibrated local-vol values", "QQ", "calibration", argHelp, false);
    }
};
static XlRegister_DupireCalibration_Get_Vols_ The_DupireCalibration_Get_Vols_XlRegisterer;

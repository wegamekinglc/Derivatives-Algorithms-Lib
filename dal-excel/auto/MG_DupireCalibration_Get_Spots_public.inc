

extern "C" __declspec(dllexport) OPER_* xl_DupireCalibration_Get_Spots
    (const OPER_* xl_calibration)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("DupireCalibration_Get_Spots");        
        argName = "calibration (input #1)";
        const Handle_<StorableDupireCalibration_> calibration = Excel::ToHandle<StorableDupireCalibration_>(_env, xl_calibration);
        argName = 0;
		Vector_<double> spots;
        DupireCalibration_Get_Spots(calibration, &spots);
        Excel::Retval_ retval;
        retval.Load(spots);
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

struct XlRegister_DupireCalibration_Get_Spots_
{
    XlRegister_DupireCalibration_Get_Spots_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Frozen calibration");
        Excel::Register("Base", "xl_DupireCalibration_Get_Spots", "DUPIRECALIBRATION.GET.SPOTS", "Copy completed calibration spot coordinates", "QQ", "calibration", argHelp, false);
    }
};
static XlRegister_DupireCalibration_Get_Spots_ The_DupireCalibration_Get_Spots_XlRegisterer;



extern "C" __declspec(dllexport) OPER_* xl_DupireCalibration_Get_Times
    (const OPER_* xl_calibration)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("DupireCalibration_Get_Times");        
        argName = "calibration (input #1)";
        const Handle_<StorableDupireCalibration_> calibration = Excel::ToHandle<StorableDupireCalibration_>(_env, xl_calibration);
        argName = 0;
		Vector_<double> times;
        DupireCalibration_Get_Times(calibration, &times);
        Excel::Retval_ retval;
        retval.Load(times);
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

struct XlRegister_DupireCalibration_Get_Times_
{
    XlRegister_DupireCalibration_Get_Times_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Frozen calibration");
        Excel::Register("Base", "xl_DupireCalibration_Get_Times", "DUPIRECALIBRATION.GET.TIMES", "Copy completed calibration model times", "QQ", "calibration", argHelp, false);
    }
};
static XlRegister_DupireCalibration_Get_Times_ The_DupireCalibration_Get_Times_XlRegisterer;

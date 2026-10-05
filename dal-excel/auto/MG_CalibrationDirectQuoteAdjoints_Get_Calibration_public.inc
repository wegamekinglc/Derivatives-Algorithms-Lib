

extern "C" __declspec(dllexport) OPER_* xl_CalibrationDirectQuoteAdjoints_Get_Calibration
    (const OPER_* xl_direct)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("CalibrationDirectQuoteAdjoints_Get_Calibration");        
        argName = "direct (input #1)";
        const Handle_<StorableCalibrationDirectQuoteAdjoints_> direct = Excel::ToHandle<StorableCalibrationDirectQuoteAdjoints_>(_env, xl_direct);
        argName = 0;
		Handle_<StorableCalibrationPullback_> calibration;
        CalibrationDirectQuoteAdjoints_Get_Calibration(direct, &calibration);
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

struct XlRegister_CalibrationDirectQuoteAdjoints_Get_Calibration_
{
    XlRegister_CalibrationDirectQuoteAdjoints_Get_Calibration_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Owned direct quote seeds");
        Excel::Register("Base", "xl_CalibrationDirectQuoteAdjoints_Get_Calibration", "CALIBRATIONDIRECTQUOTEADJOINTS.GET.CALIBRATION", "Project the owning boundary of direct quote seeds", "QQ", "direct", argHelp, false);
    }
};
static XlRegister_CalibrationDirectQuoteAdjoints_Get_Calibration_ The_CalibrationDirectQuoteAdjoints_Get_Calibration_XlRegisterer;

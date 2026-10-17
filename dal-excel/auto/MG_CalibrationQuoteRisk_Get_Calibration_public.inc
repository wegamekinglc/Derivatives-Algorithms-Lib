

extern "C" __declspec(dllexport) OPER_* xl_CalibrationQuoteRisk_Get_Calibration
    (const OPER_* xl_result)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("CalibrationQuoteRisk_Get_Calibration");        
        argName = "result (input #1)";
        const Handle_<StorableCalibrationQuoteRisk_> result = Excel::ToHandle<StorableCalibrationQuoteRisk_>(_env, xl_result);
        argName = 0;
		Handle_<StorableCalibrationPullback_> calibration;
        CalibrationQuoteRisk_Get_Calibration(result, &calibration);
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

struct XlRegister_CalibrationQuoteRisk_Get_Calibration_
{
    XlRegister_CalibrationQuoteRisk_Get_Calibration_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Completed common quote risk");
        Excel::Register("Base", "xl_CalibrationQuoteRisk_Get_Calibration", "CALIBRATIONQUOTERISK.GET.CALIBRATION", "Project the owning boundary of a common quote result", "QQ", "result", argHelp, false);
    }
};
static XlRegister_CalibrationQuoteRisk_Get_Calibration_ The_CalibrationQuoteRisk_Get_Calibration_XlRegisterer;

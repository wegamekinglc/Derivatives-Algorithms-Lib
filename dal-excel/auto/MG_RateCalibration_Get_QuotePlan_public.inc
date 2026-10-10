

extern "C" __declspec(dllexport) OPER_* xl_RateCalibration_Get_QuotePlan
    (const OPER_* xl_calibration)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("RateCalibration_Get_QuotePlan");        
        argName = "calibration (input #1)";
        const Handle_<StorableRateCalibrationSnapshot_> calibration = Excel::ToHandle<StorableRateCalibrationSnapshot_>(_env, xl_calibration);
        argName = 0;
		Handle_<StorableCalibrationRiskPlan_> plan;
        RateCalibration_Get_QuotePlan(calibration, &plan);
        Excel::Retval_ retval;
        retval.Load(_env, plan);
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

struct XlRegister_RateCalibration_Get_QuotePlan_
{
    XlRegister_RateCalibration_Get_QuotePlan_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Owning sealed calibration snapshot");
        Excel::Register("Base", "xl_RateCalibration_Get_QuotePlan", "RATECALIBRATION.GET.QUOTEPLAN", "Copy the complete native rate quote metadata plan", "QQ", "calibration", argHelp, false);
    }
};
static XlRegister_RateCalibration_Get_QuotePlan_ The_RateCalibration_Get_QuotePlan_XlRegisterer;



extern "C" __declspec(dllexport) OPER_* xl_CalibrationOutcome_Get_Mean
    (const OPER_* xl_outcome)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("CalibrationOutcome_Get_Mean");        
        argName = "outcome (input #1)";
        const Handle_<StorableCalibrationOutcome_> outcome = Excel::ToHandle<StorableCalibrationOutcome_>(_env, xl_outcome);
        argName = 0;
		double mean;
        CalibrationOutcome_Get_Mean(outcome, &mean);
        Excel::Retval_ retval;
        retval.Load(mean);
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

struct XlRegister_CalibrationOutcome_Get_Mean_
{
    XlRegister_CalibrationOutcome_Get_Mean_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Typed outcome");
        Excel::Register("Base", "xl_CalibrationOutcome_Get_Mean", "CALIBRATIONOUTCOME.GET.MEAN", "Owning typed script calibration interface", "QQ", "outcome", argHelp, false);
    }
};
static XlRegister_CalibrationOutcome_Get_Mean_ The_CalibrationOutcome_Get_Mean_XlRegisterer;

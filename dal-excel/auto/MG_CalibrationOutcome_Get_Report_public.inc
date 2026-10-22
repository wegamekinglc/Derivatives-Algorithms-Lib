

extern "C" __declspec(dllexport) OPER_* xl_CalibrationOutcome_Get_Report
    (const OPER_* xl_outcome)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("CalibrationOutcome_Get_Report");        
        argName = "outcome (input #1)";
        const Handle_<StorableCalibrationOutcome_> outcome = Excel::ToHandle<StorableCalibrationOutcome_>(_env, xl_outcome);
        argName = 0;
		Handle_<CalibrationReport_> report;
        CalibrationOutcome_Get_Report(outcome, &report);
        Excel::Retval_ retval;
        retval.Load(_env, report);
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

struct XlRegister_CalibrationOutcome_Get_Report_
{
    XlRegister_CalibrationOutcome_Get_Report_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Typed outcome");
        Excel::Register("Base", "xl_CalibrationOutcome_Get_Report", "CALIBRATIONOUTCOME.GET.REPORT", "Owning typed script calibration interface", "QQ", "outcome", argHelp, false);
    }
};
static XlRegister_CalibrationOutcome_Get_Report_ The_CalibrationOutcome_Get_Report_XlRegisterer;

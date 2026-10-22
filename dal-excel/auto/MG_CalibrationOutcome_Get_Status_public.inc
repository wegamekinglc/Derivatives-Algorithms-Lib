

extern "C" __declspec(dllexport) OPER_* xl_CalibrationOutcome_Get_Status
    (const OPER_* xl_outcome)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("CalibrationOutcome_Get_Status");        
        argName = "outcome (input #1)";
        const Handle_<StorableCalibrationOutcome_> outcome = Excel::ToHandle<StorableCalibrationOutcome_>(_env, xl_outcome);
        argName = 0;
		String_ status;
        CalibrationOutcome_Get_Status(outcome, &status);
        Excel::Retval_ retval;
        retval.Load(status);
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

struct XlRegister_CalibrationOutcome_Get_Status_
{
    XlRegister_CalibrationOutcome_Get_Status_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Typed outcome");
        Excel::Register("Base", "xl_CalibrationOutcome_Get_Status", "CALIBRATIONOUTCOME.GET.STATUS", "Owning typed script calibration interface", "QQ", "outcome", argHelp, false);
    }
};
static XlRegister_CalibrationOutcome_Get_Status_ The_CalibrationOutcome_Get_Status_XlRegisterer;

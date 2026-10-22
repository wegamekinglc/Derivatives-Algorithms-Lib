

extern "C" __declspec(dllexport) OPER_* xl_CalibrationReport_Get_Quotes
    (const OPER_* xl_report)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("CalibrationReport_Get_Quotes");        
        argName = "report (input #1)";
        const Handle_<CalibrationReport_> report = Excel::ToHandle<CalibrationReport_>(_env, xl_report);
        argName = 0;
		Matrix_<Cell_> value;
        CalibrationReport_Get_Quotes(report, &value);
        Excel::Retval_ retval;
        retval.Load(value);
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

struct XlRegister_CalibrationReport_Get_Quotes_
{
    XlRegister_CalibrationReport_Get_Quotes_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Typed report");
        Excel::Register("Base", "xl_CalibrationReport_Get_Quotes", "CALIBRATIONREPORT.GET.QUOTES", "Owning typed script calibration interface", "QQ", "report", argHelp, false);
    }
};
static XlRegister_CalibrationReport_Get_Quotes_ The_CalibrationReport_Get_Quotes_XlRegisterer;

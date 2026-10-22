

extern "C" __declspec(dllexport) OPER_* xl_CalibrationReport_Get_Plans
    (const OPER_* xl_report)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("CalibrationReport_Get_Plans");        
        argName = "report (input #1)";
        const Handle_<CalibrationReport_> report = Excel::ToHandle<CalibrationReport_>(_env, xl_report);
        argName = 0;
		Matrix_<Cell_> value;
        CalibrationReport_Get_Plans(report, &value);
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

struct XlRegister_CalibrationReport_Get_Plans_
{
    XlRegister_CalibrationReport_Get_Plans_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Typed report");
        Excel::Register("Base", "xl_CalibrationReport_Get_Plans", "CALIBRATIONREPORT.GET.PLANS", "Owning typed script calibration interface", "QQ", "report", argHelp, false);
    }
};
static XlRegister_CalibrationReport_Get_Plans_ The_CalibrationReport_Get_Plans_XlRegisterer;

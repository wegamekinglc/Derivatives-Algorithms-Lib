

extern "C" __declspec(dllexport) OPER_* xl_XccyMonteCarloValidation_Get_Report
    (const OPER_* xl_value)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("XccyMonteCarloValidation_Get_Report");        
        argName = "value (input #1)";
        const Handle_<StorableXccyMonteCarloValidation_> value = Excel::ToHandle<StorableXccyMonteCarloValidation_>(_env, xl_value);
        argName = 0;
		Handle_<CalibrationReport_> report;
        XccyMonteCarloValidation_Get_Report(value, &report);
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

struct XlRegister_XccyMonteCarloValidation_Get_Report_
{
    XlRegister_XccyMonteCarloValidation_Get_Report_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Typed value");
        Excel::Register("Base", "xl_XccyMonteCarloValidation_Get_Report", "XCCYMONTECARLOVALIDATION.GET.REPORT", "XccyMonteCarloValidation Get Report", "QQ", "value", argHelp, false);
    }
};
static XlRegister_XccyMonteCarloValidation_Get_Report_ The_XccyMonteCarloValidation_Get_Report_XlRegisterer;

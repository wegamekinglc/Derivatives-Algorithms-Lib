

extern "C" __declspec(dllexport) OPER_* xl_XccyMonteCarloOutcome_Get_Report
    (const OPER_* xl_outcome)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("XccyMonteCarloOutcome_Get_Report");        
        argName = "outcome (input #1)";
        const Handle_<StorableXccyMonteCarloOutcome_> outcome = Excel::ToHandle<StorableXccyMonteCarloOutcome_>(_env, xl_outcome);
        argName = 0;
		Handle_<CalibrationReport_> report;
        XccyMonteCarloOutcome_Get_Report(outcome, &report);
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

struct XlRegister_XccyMonteCarloOutcome_Get_Report_
{
    XlRegister_XccyMonteCarloOutcome_Get_Report_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Typed outcome");
        Excel::Register("Base", "xl_XccyMonteCarloOutcome_Get_Report", "XCCYMONTECARLOOUTCOME.GET.REPORT", "XccyMonteCarloOutcome Get Report", "QQ", "outcome", argHelp, false);
    }
};
static XlRegister_XccyMonteCarloOutcome_Get_Report_ The_XccyMonteCarloOutcome_Get_Report_XlRegisterer;

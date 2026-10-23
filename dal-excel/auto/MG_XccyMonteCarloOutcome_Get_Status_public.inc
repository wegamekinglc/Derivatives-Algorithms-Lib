

extern "C" __declspec(dllexport) OPER_* xl_XccyMonteCarloOutcome_Get_Status
    (const OPER_* xl_outcome)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("XccyMonteCarloOutcome_Get_Status");        
        argName = "outcome (input #1)";
        const Handle_<StorableXccyMonteCarloOutcome_> outcome = Excel::ToHandle<StorableXccyMonteCarloOutcome_>(_env, xl_outcome);
        argName = 0;
		String_ status;
        XccyMonteCarloOutcome_Get_Status(outcome, &status);
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

struct XlRegister_XccyMonteCarloOutcome_Get_Status_
{
    XlRegister_XccyMonteCarloOutcome_Get_Status_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Typed outcome");
        Excel::Register("Base", "xl_XccyMonteCarloOutcome_Get_Status", "XCCYMONTECARLOOUTCOME.GET.STATUS", "XccyMonteCarloOutcome Get Status", "QQ", "outcome", argHelp, false);
    }
};
static XlRegister_XccyMonteCarloOutcome_Get_Status_ The_XccyMonteCarloOutcome_Get_Status_XlRegisterer;

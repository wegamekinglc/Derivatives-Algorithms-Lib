

extern "C" __declspec(dllexport) OPER_* xl_XccyMonteCarloBasketOutcome_Get_Status
    (const OPER_* xl_outcome)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("XccyMonteCarloBasketOutcome_Get_Status");        
        argName = "outcome (input #1)";
        const Handle_<StorableXccyMonteCarloBasketOutcome_> outcome = Excel::ToHandle<StorableXccyMonteCarloBasketOutcome_>(_env, xl_outcome);
        argName = 0;
		String_ status;
        XccyMonteCarloBasketOutcome_Get_Status(outcome, &status);
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

struct XlRegister_XccyMonteCarloBasketOutcome_Get_Status_
{
    XlRegister_XccyMonteCarloBasketOutcome_Get_Status_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Owning outcome");
        Excel::Register("Base", "xl_XccyMonteCarloBasketOutcome_Get_Status", "XCCYMONTECARLOBASKETOUTCOME.GET.STATUS", "Inspect basket outcome status", "QQ", "outcome", argHelp, false);
    }
};
static XlRegister_XccyMonteCarloBasketOutcome_Get_Status_ The_XccyMonteCarloBasketOutcome_Get_Status_XlRegisterer;

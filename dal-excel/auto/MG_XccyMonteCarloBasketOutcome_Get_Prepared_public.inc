

extern "C" __declspec(dllexport) OPER_* xl_XccyMonteCarloBasketOutcome_Get_Prepared
    (const OPER_* xl_outcome)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("XccyMonteCarloBasketOutcome_Get_Prepared");        
        argName = "outcome (input #1)";
        const Handle_<StorableXccyMonteCarloBasketOutcome_> outcome = Excel::ToHandle<StorableXccyMonteCarloBasketOutcome_>(_env, xl_outcome);
        argName = 0;
		Handle_<StorablePreparedXccyMonteCarloBasket_> prepared;
        XccyMonteCarloBasketOutcome_Get_Prepared(outcome, &prepared);
        Excel::Retval_ retval;
        retval.Load(_env, prepared);
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

struct XlRegister_XccyMonteCarloBasketOutcome_Get_Prepared_
{
    XlRegister_XccyMonteCarloBasketOutcome_Get_Prepared_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Prepared outcome");
        Excel::Register("Base", "xl_XccyMonteCarloBasketOutcome_Get_Prepared", "XCCYMONTECARLOBASKETOUTCOME.GET.PREPARED", "Extract a successfully prepared basket", "QQ", "outcome", argHelp, false);
    }
};
static XlRegister_XccyMonteCarloBasketOutcome_Get_Prepared_ The_XccyMonteCarloBasketOutcome_Get_Prepared_XlRegisterer;



extern "C" __declspec(dllexport) OPER_* xl_XccyMonteCarloBasketOutcome_Get_Partial
    (const OPER_* xl_outcome)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("XccyMonteCarloBasketOutcome_Get_Partial");        
        argName = "outcome (input #1)";
        const Handle_<StorableXccyMonteCarloBasketOutcome_> outcome = Excel::ToHandle<StorableXccyMonteCarloBasketOutcome_>(_env, xl_outcome);
        argName = 0;
		Handle_<StorableXccyMonteCarloBasketPartial_> partial;
        XccyMonteCarloBasketOutcome_Get_Partial(outcome, &partial);
        Excel::Retval_ retval;
        retval.Load(_env, partial);
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

struct XlRegister_XccyMonteCarloBasketOutcome_Get_Partial_
{
    XlRegister_XccyMonteCarloBasketOutcome_Get_Partial_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Failed outcome");
        Excel::Register("Base", "xl_XccyMonteCarloBasketOutcome_Get_Partial", "XCCYMONTECARLOBASKETOUTCOME.GET.PARTIAL", "Extract owned progress from a failed basket", "QQ", "outcome", argHelp, false);
    }
};
static XlRegister_XccyMonteCarloBasketOutcome_Get_Partial_ The_XccyMonteCarloBasketOutcome_Get_Partial_XlRegisterer;

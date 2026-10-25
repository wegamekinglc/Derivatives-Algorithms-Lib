

extern "C" __declspec(dllexport) OPER_* xl_XccyMonteCarloBasketOutcome_Get_Result
    (const OPER_* xl_outcome)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("XccyMonteCarloBasketOutcome_Get_Result");        
        argName = "outcome (input #1)";
        const Handle_<StorableXccyMonteCarloBasketOutcome_> outcome = Excel::ToHandle<StorableXccyMonteCarloBasketOutcome_>(_env, xl_outcome);
        argName = 0;
		Handle_<StorableXccyMonteCarloBasketResult_> result;
        XccyMonteCarloBasketOutcome_Get_Result(outcome, &result);
        Excel::Retval_ retval;
        retval.Load(_env, result);
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

struct XlRegister_XccyMonteCarloBasketOutcome_Get_Result_
{
    XlRegister_XccyMonteCarloBasketOutcome_Get_Result_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Estimate outcome");
        Excel::Register("Base", "xl_XccyMonteCarloBasketOutcome_Get_Result", "XCCYMONTECARLOBASKETOUTCOME.GET.RESULT", "Extract a complete basket estimate", "QQ", "outcome", argHelp, false);
    }
};
static XlRegister_XccyMonteCarloBasketOutcome_Get_Result_ The_XccyMonteCarloBasketOutcome_Get_Result_XlRegisterer;

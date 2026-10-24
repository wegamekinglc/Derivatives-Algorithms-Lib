

extern "C" __declspec(dllexport) OPER_* xl_XccyMonteCarloBasketOutcome_Get_Json
    (const OPER_* xl_outcome)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("XccyMonteCarloBasketOutcome_Get_Json");        
        argName = "outcome (input #1)";
        const Handle_<StorableXccyMonteCarloBasketOutcome_> outcome = Excel::ToHandle<StorableXccyMonteCarloBasketOutcome_>(_env, xl_outcome);
        argName = 0;
		Vector_<String_> json;
        XccyMonteCarloBasketOutcome_Get_Json(outcome, &json);
        Excel::Retval_ retval;
        retval.Load(json);
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

struct XlRegister_XccyMonteCarloBasketOutcome_Get_Json_
{
    XlRegister_XccyMonteCarloBasketOutcome_Get_Json_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Owning outcome");
        Excel::Register("Base", "xl_XccyMonteCarloBasketOutcome_Get_Json", "XCCYMONTECARLOBASKETOUTCOME.GET.JSON", "Export basket outcome details", "QQ", "outcome", argHelp, false);
    }
};
static XlRegister_XccyMonteCarloBasketOutcome_Get_Json_ The_XccyMonteCarloBasketOutcome_Get_Json_XlRegisterer;

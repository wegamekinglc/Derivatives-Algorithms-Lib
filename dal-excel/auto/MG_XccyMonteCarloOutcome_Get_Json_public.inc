

extern "C" __declspec(dllexport) OPER_* xl_XccyMonteCarloOutcome_Get_Json
    (const OPER_* xl_outcome)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("XccyMonteCarloOutcome_Get_Json");        
        argName = "outcome (input #1)";
        const Handle_<StorableXccyMonteCarloOutcome_> outcome = Excel::ToHandle<StorableXccyMonteCarloOutcome_>(_env, xl_outcome);
        argName = 0;
		Vector_<String_> json;
        XccyMonteCarloOutcome_Get_Json(outcome, &json);
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

struct XlRegister_XccyMonteCarloOutcome_Get_Json_
{
    XlRegister_XccyMonteCarloOutcome_Get_Json_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Typed outcome");
        Excel::Register("Base", "xl_XccyMonteCarloOutcome_Get_Json", "XCCYMONTECARLOOUTCOME.GET.JSON", "XccyMonteCarloOutcome Get Json", "QQ", "outcome", argHelp, false);
    }
};
static XlRegister_XccyMonteCarloOutcome_Get_Json_ The_XccyMonteCarloOutcome_Get_Json_XlRegisterer;

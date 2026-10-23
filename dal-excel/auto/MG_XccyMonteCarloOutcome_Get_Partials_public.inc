

extern "C" __declspec(dllexport) OPER_* xl_XccyMonteCarloOutcome_Get_Partials
    (const OPER_* xl_outcome)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("XccyMonteCarloOutcome_Get_Partials");        
        argName = "outcome (input #1)";
        const Handle_<StorableXccyMonteCarloOutcome_> outcome = Excel::ToHandle<StorableXccyMonteCarloOutcome_>(_env, xl_outcome);
        argName = 0;
		Matrix_<Cell_> partials;
        XccyMonteCarloOutcome_Get_Partials(outcome, &partials);
        Excel::Retval_ retval;
        retval.Load(partials);
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

struct XlRegister_XccyMonteCarloOutcome_Get_Partials_
{
    XlRegister_XccyMonteCarloOutcome_Get_Partials_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Typed outcome");
        Excel::Register("Base", "xl_XccyMonteCarloOutcome_Get_Partials", "XCCYMONTECARLOOUTCOME.GET.PARTIALS", "XccyMonteCarloOutcome Get Partials", "QQ", "outcome", argHelp, false);
    }
};
static XlRegister_XccyMonteCarloOutcome_Get_Partials_ The_XccyMonteCarloOutcome_Get_Partials_XlRegisterer;

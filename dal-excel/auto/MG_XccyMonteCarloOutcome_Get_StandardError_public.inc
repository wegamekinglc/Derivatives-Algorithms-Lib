

extern "C" __declspec(dllexport) OPER_* xl_XccyMonteCarloOutcome_Get_StandardError
    (const OPER_* xl_outcome)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("XccyMonteCarloOutcome_Get_StandardError");        
        argName = "outcome (input #1)";
        const Handle_<StorableXccyMonteCarloOutcome_> outcome = Excel::ToHandle<StorableXccyMonteCarloOutcome_>(_env, xl_outcome);
        argName = 0;
		double error;
        XccyMonteCarloOutcome_Get_StandardError(outcome, &error);
        Excel::Retval_ retval;
        retval.Load(error);
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

struct XlRegister_XccyMonteCarloOutcome_Get_StandardError_
{
    XlRegister_XccyMonteCarloOutcome_Get_StandardError_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Typed outcome");
        Excel::Register("Base", "xl_XccyMonteCarloOutcome_Get_StandardError", "XCCYMONTECARLOOUTCOME.GET.STANDARDERROR", "XccyMonteCarloOutcome Get StandardError", "QQ", "outcome", argHelp, false);
    }
};
static XlRegister_XccyMonteCarloOutcome_Get_StandardError_ The_XccyMonteCarloOutcome_Get_StandardError_XlRegisterer;

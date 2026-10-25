

extern "C" __declspec(dllexport) OPER_* xl_XccyMonteCarloOutcome_Validate
    (const OPER_* xl_outcome, const OPER_* xl_reference, const OPER_* xl_precision)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("XccyMonteCarloOutcome_Validate");        
        argName = "outcome (input #1)";
        const Handle_<StorableXccyMonteCarloOutcome_> outcome = Excel::ToHandle<StorableXccyMonteCarloOutcome_>(_env, xl_outcome);
        argName = "reference (input #2)";
        const Handle_<StorableXccyMonteCarloReference_> reference = Excel::ToHandle<StorableXccyMonteCarloReference_>(_env, xl_reference);
        argName = "precision (input #3)";
        const Handle_<StorableXccyMonteCarloPrecision_> precision = Excel::ToHandle<StorableXccyMonteCarloPrecision_>(_env, xl_precision);
        argName = 0;
		Handle_<StorableXccyMonteCarloValidation_> value;
        XccyMonteCarloOutcome_Validate(outcome, reference, precision, &value);
        Excel::Retval_ retval;
        retval.Load(_env, value);
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

struct XlRegister_XccyMonteCarloOutcome_Validate_
{
    XlRegister_XccyMonteCarloOutcome_Validate_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Typed outcome");
        argHelp.push_back("Typed reference");
        argHelp.push_back("Typed precision");
        Excel::Register("Base", "xl_XccyMonteCarloOutcome_Validate", "XCCYMONTECARLOOUTCOME.VALIDATE", "XccyMonteCarloOutcome Validate", "QQQQ", "outcome,reference,precision", argHelp, false);
    }
};
static XlRegister_XccyMonteCarloOutcome_Validate_ The_XccyMonteCarloOutcome_Validate_XlRegisterer;

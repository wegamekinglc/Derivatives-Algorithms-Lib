

extern "C" __declspec(dllexport) OPER_* xl_RateLegConventionNew
    (const OPER_* xl_freq, const OPER_* xl_basis)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("RateLegConventionNew");        
        argName = "freq (input #1)";
        const String_ freq = Excel::ToString(xl_freq);
        argName = "basis (input #2)";
        const String_ basis = Excel::ToString(xl_basis);
        argName = 0;
		Handle_<StorableRateLegConvention_> convention;
        RateLegConventionNew(freq, basis, &convention);
        Excel::Retval_ retval;
        retval.Load(_env, convention);
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

struct XlRegister_RateLegConventionNew_
{
    XlRegister_RateLegConventionNew_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Payment frequency as an ISO period (e.g. \"3M\", \"6M\")");
        argHelp.push_back("Day basis name (e.g. \"ACT_365F\", \"ACT_360\")");
        Excel::Register("Base", "xl_RateLegConventionNew", "DA.RATELEGCONVENTIONNEW", "Create a rate leg convention handle", "QQQ", "freq,basis", argHelp, false);
    }
};
static XlRegister_RateLegConventionNew_ The_RateLegConventionNew_XlRegisterer;

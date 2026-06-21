

extern "C" __declspec(dllexport) OPER_* xl_RateLegConvention_New
    (const OPER_* xl_freq, const OPER_* xl_basis)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("RateLegConvention_New");        
        argName = "freq (input #1)";
        const String_ freq = Excel::ToString(xl_freq);
        argName = "basis (input #2)";
        const String_ basis = Excel::ToString(xl_basis);
        argName = 0;
		Handle_<StorableRateLegConvention_> convention;
        RateLegConvention_New(freq, basis, &convention);
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

struct XlRegister_RateLegConvention_New_
{
    XlRegister_RateLegConvention_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Payment frequency as an ISO period (e.g. \"3M\", \"6M\")");
        argHelp.push_back("Day basis name (e.g. \"ACT_365F\", \"ACT_360\")");
        Excel::Register("Base", "xl_RateLegConvention_New", "RATELEGCONVENTION.NEW", "Create a rate leg convention handle", "QQQ", "freq,basis", argHelp, false);
    }
};
static XlRegister_RateLegConvention_New_ The_RateLegConvention_New_XlRegisterer;

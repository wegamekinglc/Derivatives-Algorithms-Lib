

extern "C" __declspec(dllexport) OPER_* xl_RateLegConvention_New
    (const OPER_* xl_freq, const OPER_* xl_basis, const OPER_* xl_calendar, const OPER_* xl_accrualConvention, const OPER_* xl_paymentConvention)	
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
        argName = "calendar (input #3)";
        const String_ calendar = Excel::ToString(xl_calendar, true);
        argName = "accrualConvention (input #4)";
        const String_ accrualConvention = Excel::ToString(xl_accrualConvention, true);
        argName = "paymentConvention (input #5)";
        const String_ paymentConvention = Excel::ToString(xl_paymentConvention, true);
        argName = 0;
		Handle_<StorableRateLegConvention_> convention;
        RateLegConvention_New(freq, basis, calendar, accrualConvention, paymentConvention, &convention);
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
        argHelp.push_back("Holiday center for accrual and payment dates (empty uses weekends only)");
        argHelp.push_back("Business-day convention for accrual dates (default Following)");
        argHelp.push_back("Business-day convention for payment dates (default Following)");
        Excel::Register("Base", "xl_RateLegConvention_New", "RATELEGCONVENTION.NEW", "Create a rate leg convention handle", "QQQQQQ", "freq,basis,[calendar],[accrualConvention],[paymentConvention]", argHelp, false);
    }
};
static XlRegister_RateLegConvention_New_ The_RateLegConvention_New_XlRegisterer;

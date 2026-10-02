

extern "C" __declspec(dllexport) OPER_* xl_Swaption_New
    (const OPER_* xl_expiry, const OPER_* xl_fixed, const OPER_* xl_floating, const OPER_* xl_strike, const OPER_* xl_type)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("Swaption_New");        
        argName = "expiry (input #1)";
        const Date_ expiry = Excel::ToDate(xl_expiry);
        argName = "fixed (input #2)";
        const Vector_<Handle_<Storable_>> fixed = Excel::ToHandleBaseVector(_env, xl_fixed);
        argName = "floating (input #3)";
        const Vector_<Handle_<Storable_>> floating = Excel::ToHandleBaseVector(_env, xl_floating);
        argName = "strike (input #4)";
        const double strike = Excel::ToDouble(xl_strike);
        argName = "type (input #5)";
        const String_ type = Excel::ToString(xl_type);
        argName = 0;
		Handle_<StorableGSREuropeanOption_> option;
        Swaption_New(expiry, fixed, floating, strike, type, &option);
        Excel::Retval_ retval;
        retval.Load(_env, option);
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

struct XlRegister_Swaption_New_
{
    XlRegister_Swaption_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Exercise date");
        argHelp.push_back("GSRFixedCoupon handles");
        argHelp.push_back("GSRFloatingCoupon handles");
        argHelp.push_back("Fixed rate strike");
        argHelp.push_back("CALL for payer or PUT for receiver");
        Excel::Register("Base", "xl_Swaption_New", "SWAPTION.NEW", "Create a physically settled European swaption", "QQQQQQ", "expiry,fixed,floating,strike,type", argHelp, false);
    }
};
static XlRegister_Swaption_New_ The_Swaption_New_XlRegisterer;

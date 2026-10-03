

extern "C" __declspec(dllexport) OPER_* xl_BondOption_New
    (const OPER_* xl_expiry, const OPER_* xl_maturity, const OPER_* xl_strike, const OPER_* xl_type)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("BondOption_New");        
        argName = "expiry (input #1)";
        const Date_ expiry = Excel::ToDate(xl_expiry);
        argName = "maturity (input #2)";
        const Date_ maturity = Excel::ToDate(xl_maturity);
        argName = "strike (input #3)";
        const double strike = Excel::ToDouble(xl_strike);
        argName = "type (input #4)";
        const String_ type = Excel::ToString(xl_type);
        argName = 0;
		Handle_<StorableEuropeanRateOption_> option;
        BondOption_New(expiry, maturity, strike, type, &option);
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

struct XlRegister_BondOption_New_
{
    XlRegister_BondOption_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Exercise date");
        argHelp.push_back("Bond maturity");
        argHelp.push_back("Bond-price strike per unit face value");
        argHelp.push_back("CALL or PUT");
        Excel::Register("Base", "xl_BondOption_New", "BONDOPTION.NEW", "Create a European zero-coupon bond option", "QQQQQ", "expiry,maturity,strike,type", argHelp, false);
    }
};
static XlRegister_BondOption_New_ The_BondOption_New_XlRegisterer;

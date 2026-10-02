

extern "C" __declspec(dllexport) OPER_* xl_VolQuote_New
    (const OPER_* xl_name, const OPER_* xl_option, const OPER_* xl_volatility, const OPER_* xl_priceScale, const OPER_* xl_convention, const OPER_* xl_shift)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("VolQuote_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "option (input #2)";
        const Handle_<StorableGSREuropeanOption_> option = Excel::ToHandle<StorableGSREuropeanOption_>(_env, xl_option);
        argName = "volatility (input #3)";
        const double volatility = Excel::ToDouble(xl_volatility);
        argName = "priceScale (input #4)";
        const double priceScale = Excel::ToDouble(xl_priceScale);
        argName = "convention (input #5)";
        const String_ convention = Excel::ToString(xl_convention, true);
        argName = "shift (input #6)";
        const double shift = Excel::ToDouble(xl_shift, true).value_or(0.0);
        argName = 0;
		Handle_<StorableVolQuote_> quote;
        VolQuote_New(name, option, volatility, priceScale, convention, shift, &quote);
        Excel::Retval_ retval;
        retval.Load(_env, quote);
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

struct XlRegister_VolQuote_New_
{
    XlRegister_VolQuote_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Unique quote name");
        argHelp.push_back("Caplet or swaption");
        argHelp.push_back("Annualized decimal volatility");
        argHelp.push_back("Positive price residual scale");
        argHelp.push_back("NORMAL (default), BLACK or SHIFTED_BLACK");
        argHelp.push_back("Shift for SHIFTED_BLACK");
        Excel::Register("Base", "xl_VolQuote_New", "VOLQUOTE.NEW", "Create a market volatility quote for a caplet or physical swaption", "QQQQQQQ", "name,option,volatility,priceScale,[convention],[shift]", argHelp, false);
    }
};
static XlRegister_VolQuote_New_ The_VolQuote_New_XlRegisterer;



extern "C" __declspec(dllexport) OPER_* xl_CalibrationQuote_New
    (const OPER_* xl_name, const OPER_* xl_option, const OPER_* xl_price, const OPER_* xl_priceScale)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("CalibrationQuote_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "option (input #2)";
        const Handle_<StorableEuropeanRateOption_> option = Excel::ToHandle<StorableEuropeanRateOption_>(_env, xl_option);
        argName = "price (input #3)";
        const double price = Excel::ToDouble(xl_price);
        argName = "priceScale (input #4)";
        const double priceScale = Excel::ToDouble(xl_priceScale);
        argName = 0;
		Handle_<StorableCalibrationQuote_> quote;
        CalibrationQuote_New(name, option, price, priceScale, &quote);
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

struct XlRegister_CalibrationQuote_New_
{
    XlRegister_CalibrationQuote_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Unique quote name");
        argHelp.push_back("Quoted option");
        argHelp.push_back("Market price per unit notional");
        argHelp.push_back("Positive price error scale");
        Excel::Register("Base", "xl_CalibrationQuote_New", "CALIBRATIONQUOTE.NEW", "Create a price quote for Gaussian volatility calibration", "QQQQQ", "name,option,price,priceScale", argHelp, false);
    }
};
static XlRegister_CalibrationQuote_New_ The_CalibrationQuote_New_XlRegisterer;



extern "C" __declspec(dllexport) OPER_* xl_Deposit_New
    (const OPER_* xl_tradeDate, const OPER_* xl_start, const OPER_* xl_maturity, const OPER_* xl_marketRate, const OPER_* xl_convention)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("Deposit_New");        
        argName = "tradeDate (input #1)";
        const Date_ tradeDate = Excel::ToDate(xl_tradeDate);
        argName = "start (input #2)";
        const Date_ start = Excel::ToDate(xl_start);
        argName = "maturity (input #3)";
        const Date_ maturity = Excel::ToDate(xl_maturity);
        argName = "marketRate (input #4)";
        const double marketRate = Excel::ToDouble(xl_marketRate);
        argName = "convention (input #5)";
        const Handle_<StorableRateIndexConvention_> convention = Excel::ToHandle<StorableRateIndexConvention_>(_env, xl_convention);
        argName = 0;
		Handle_<StorableYCInstrument_> instrument;
        Deposit_New(tradeDate, start, maturity, marketRate, convention, &instrument);
        Excel::Retval_ retval;
        retval.Load(_env, instrument);
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

struct XlRegister_Deposit_New_
{
    XlRegister_Deposit_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("The trade/settlement date");
        argHelp.push_back("The start date of the deposit");
        argHelp.push_back("The maturity date of the deposit");
        argHelp.push_back("The market quoted rate");
        argHelp.push_back("The rate index convention");
        Excel::Register("Base", "xl_Deposit_New", "DA.DEPOSITNEW", "Create a deposit instrument for curve calibration", "QQQQQQ", "tradeDate,start,maturity,marketRate,convention", argHelp, false);
    }
};
static XlRegister_Deposit_New_ The_Deposit_New_XlRegisterer;

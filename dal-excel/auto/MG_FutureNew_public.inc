

extern "C" __declspec(dllexport) OPER_* xl_FutureNew
    (const OPER_* xl_tradeDate, const OPER_* xl_start, const OPER_* xl_maturity, const OPER_* xl_marketRate, const OPER_* xl_convention, const OPER_* xl_convexityAdjustment)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("FutureNew");        
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
        argName = "convexityAdjustment (input #6)";
        const double convexityAdjustment = Excel::ToDouble(xl_convexityAdjustment, true).value_or(0.0);
        argName = 0;
		Handle_<StorableYCInstrument_> instrument;
        FutureNew(tradeDate, start, maturity, marketRate, convention, convexityAdjustment, &instrument);
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

struct XlRegister_FutureNew_
{
    XlRegister_FutureNew_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("The trade/settlement date");
        argHelp.push_back("The start date of the future");
        argHelp.push_back("The maturity date of the future");
        argHelp.push_back("The market quoted rate");
        argHelp.push_back("The rate index convention");
        argHelp.push_back("Convexity adjustment");
        Excel::Register("Base", "xl_FutureNew", "DA.FUTURENEW", "Create a future instrument for curve calibration", "QQQQQQQ", "tradeDate,start,maturity,marketRate,convention,[convexityAdjustment]", argHelp, false);
    }
};
static XlRegister_FutureNew_ The_FutureNew_XlRegisterer;



extern "C" __declspec(dllexport) OPER_* xl_RateTradeHeader_New
    (const OPER_* xl_instrumentId, const OPER_* xl_tradeDate, const OPER_* xl_start, const OPER_* xl_maturity, const OPER_* xl_currency)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("RateTradeHeader_New");        
        argName = "instrumentId (input #1)";
        const String_ instrumentId = Excel::ToString(xl_instrumentId);
        argName = "tradeDate (input #2)";
        const Date_ tradeDate = Excel::ToDate(xl_tradeDate);
        argName = "start (input #3)";
        const Date_ start = Excel::ToDate(xl_start);
        argName = "maturity (input #4)";
        const Date_ maturity = Excel::ToDate(xl_maturity);
        argName = "currency (input #5)";
        const String_ currency = Excel::ToString(xl_currency);
        argName = 0;
		Handle_<StorableRateTradeDefinition_> header;
        RateTradeHeader_New(instrumentId, tradeDate, start, maturity, currency, &header);
        Excel::Retval_ retval;
        retval.Load(_env, header);
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

struct XlRegister_RateTradeHeader_New_
{
    XlRegister_RateTradeHeader_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Trade identifier");
        argHelp.push_back("The trade date");
        argHelp.push_back("The start date");
        argHelp.push_back("The maturity date");
        argHelp.push_back("Trade currency code (for XCCY pass the domestic currency, the actual PV denomination)");
        Excel::Register("Base", "xl_RateTradeHeader_New", "RATETRADEHEADER.NEW", "Create the shared header (identifier, schedule, currency) of a rate trade", "QQQQQQ", "instrumentId,tradeDate,start,maturity,currency", argHelp, false);
    }
};
static XlRegister_RateTradeHeader_New_ The_RateTradeHeader_New_XlRegisterer;

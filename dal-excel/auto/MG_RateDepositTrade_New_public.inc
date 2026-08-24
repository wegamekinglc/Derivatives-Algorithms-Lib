

extern "C" __declspec(dllexport) OPER_* xl_RateDepositTrade_New
    (const OPER_* xl_instrumentId, const OPER_* xl_tradeDate, const OPER_* xl_start, const OPER_* xl_maturity, const OPER_* xl_currency, const OPER_* xl_notional, const OPER_* xl_contractRate, const OPER_* xl_lend, const OPER_* xl_index, const OPER_* xl_discountComponentKey)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("RateDepositTrade_New");        
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
        argName = "notional (input #6)";
        const double notional = Excel::ToDouble(xl_notional);
        argName = "contractRate (input #7)";
        const double contractRate = Excel::ToDouble(xl_contractRate);
        argName = "lend (input #8)";
        const bool lend = Excel::ToBool(xl_lend);
        argName = "index (input #9)";
        const Handle_<StorableRateIndexConvention_> index = Excel::ToHandle<StorableRateIndexConvention_>(_env, xl_index);
        argName = "discountComponentKey (input #10)";
        const String_ discountComponentKey = Excel::ToString(xl_discountComponentKey);
        argName = 0;
		Handle_<StorableRateTradeDefinition_> trade;
        RateDepositTrade_New(instrumentId, tradeDate, start, maturity, currency, notional, contractRate, lend, index, discountComponentKey, &trade);
        Excel::Retval_ retval;
        retval.Load(_env, trade);
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

struct XlRegister_RateDepositTrade_New_
{
    XlRegister_RateDepositTrade_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Trade identifier");
        argHelp.push_back("The trade date");
        argHelp.push_back("The start date");
        argHelp.push_back("The maturity date");
        argHelp.push_back("Trade currency code (the actual PV denomination)");
        argHelp.push_back("Positive notional");
        argHelp.push_back("Contract rate");
        argHelp.push_back("True to lend (receive the maturity payment)");
        argHelp.push_back("The rate index convention");
        argHelp.push_back("Market component key of the discount curve");
        Excel::Register("Base", "xl_RateDepositTrade_New", "RATEDEPOSITTRADE.NEW", "Create a deposit rate trade for node-sensitivity pricing", "QQQQQQQQQQQ", "instrumentId,tradeDate,start,maturity,currency,notional,contractRate,lend,index,discountComponentKey", argHelp, false);
    }
};
static XlRegister_RateDepositTrade_New_ The_RateDepositTrade_New_XlRegisterer;

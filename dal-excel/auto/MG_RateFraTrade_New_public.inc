

extern "C" __declspec(dllexport) OPER_* xl_RateFraTrade_New
    (const OPER_* xl_instrumentId, const OPER_* xl_tradeDate, const OPER_* xl_start, const OPER_* xl_maturity, const OPER_* xl_currency, const OPER_* xl_notional, const OPER_* xl_contractRate, const OPER_* xl_receiveFloating, const OPER_* xl_settleAtStart, const OPER_* xl_index, const OPER_* xl_fixingIndexName, const OPER_* xl_fixingHour, const OPER_* xl_fixingMinute, const OPER_* xl_forecastComponentKey, const OPER_* xl_discountComponentKey)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("RateFraTrade_New");        
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
        argName = "receiveFloating (input #8)";
        const bool receiveFloating = Excel::ToBool(xl_receiveFloating);
        argName = "settleAtStart (input #9)";
        const bool settleAtStart = Excel::ToBool(xl_settleAtStart);
        argName = "index (input #10)";
        const Handle_<StorableRateIndexConvention_> index = Excel::ToHandle<StorableRateIndexConvention_>(_env, xl_index);
        argName = "fixingIndexName (input #11)";
        const String_ fixingIndexName = Excel::ToString(xl_fixingIndexName);
        argName = "fixingHour (input #12)";
        const int fixingHour = Excel::ToInt(xl_fixingHour);
        argName = "fixingMinute (input #13)";
        const int fixingMinute = Excel::ToInt(xl_fixingMinute);
        argName = "forecastComponentKey (input #14)";
        const String_ forecastComponentKey = Excel::ToString(xl_forecastComponentKey);
        argName = "discountComponentKey (input #15)";
        const String_ discountComponentKey = Excel::ToString(xl_discountComponentKey);
        argName = 0;
		Handle_<StorableRateTradeDefinition_> trade;
        RateFraTrade_New(instrumentId, tradeDate, start, maturity, currency, notional, contractRate, receiveFloating, settleAtStart, index, fixingIndexName, fixingHour, fixingMinute, forecastComponentKey, discountComponentKey, &trade);
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

struct XlRegister_RateFraTrade_New_
{
    XlRegister_RateFraTrade_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Trade identifier");
        argHelp.push_back("The trade date");
        argHelp.push_back("The start date");
        argHelp.push_back("The maturity date");
        argHelp.push_back("Trade currency code (the actual PV denomination)");
        argHelp.push_back("Positive notional");
        argHelp.push_back("Contract rate");
        argHelp.push_back("True to receive the floating leg");
        argHelp.push_back("True to settle at the period start");
        argHelp.push_back("The rate index convention");
        argHelp.push_back("Fixing index name (e.g. \"USD-LIBOR-3M\")");
        argHelp.push_back("Fixing publication hour (0-23)");
        argHelp.push_back("Fixing publication minute (0-59)");
        argHelp.push_back("Market component key of the forecast curve");
        argHelp.push_back("Market component key of the discount curve");
        Excel::Register("Base", "xl_RateFraTrade_New", "RATEFRATRADE.NEW", "Create a FRA rate trade for node-sensitivity pricing", "QQQQQQQQQQQQQQQQ", "instrumentId,tradeDate,start,maturity,currency,notional,contractRate,receiveFloating,settleAtStart,index,fixingIndexName,fixingHour,fixingMinute,forecastComponentKey,discountComponentKey", argHelp, false);
    }
};
static XlRegister_RateFraTrade_New_ The_RateFraTrade_New_XlRegisterer;

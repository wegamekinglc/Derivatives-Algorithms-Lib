

extern "C" __declspec(dllexport) OPER_* xl_RateFutureTrade_New
    (const OPER_* xl_instrumentId, const OPER_* xl_tradeDate, const OPER_* xl_start, const OPER_* xl_maturity, const OPER_* xl_currency, const OPER_* xl_contractCount, const OPER_* xl_longPosition, const OPER_* xl_referencePrice, const OPER_* xl_contractValuePerPricePoint, const OPER_* xl_convexityAdjustment, const OPER_* xl_index, const OPER_* xl_fixingIndexName, const OPER_* xl_fixingHour, const OPER_* xl_fixingMinute, const OPER_* xl_forecastComponentKey)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("RateFutureTrade_New");        
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
        argName = "contractCount (input #6)";
        const double contractCount = Excel::ToDouble(xl_contractCount);
        argName = "longPosition (input #7)";
        const bool longPosition = Excel::ToBool(xl_longPosition);
        argName = "referencePrice (input #8)";
        const double referencePrice = Excel::ToDouble(xl_referencePrice);
        argName = "contractValuePerPricePoint (input #9)";
        const double contractValuePerPricePoint = Excel::ToDouble(xl_contractValuePerPricePoint);
        argName = "convexityAdjustment (input #10)";
        const double convexityAdjustment = Excel::ToDouble(xl_convexityAdjustment);
        argName = "index (input #11)";
        const Handle_<StorableRateIndexConvention_> index = Excel::ToHandle<StorableRateIndexConvention_>(_env, xl_index);
        argName = "fixingIndexName (input #12)";
        const String_ fixingIndexName = Excel::ToString(xl_fixingIndexName);
        argName = "fixingHour (input #13)";
        const int fixingHour = Excel::ToInt(xl_fixingHour);
        argName = "fixingMinute (input #14)";
        const int fixingMinute = Excel::ToInt(xl_fixingMinute);
        argName = "forecastComponentKey (input #15)";
        const String_ forecastComponentKey = Excel::ToString(xl_forecastComponentKey);
        argName = 0;
		Handle_<StorableRateTradeDefinition_> trade;
        RateFutureTrade_New(instrumentId, tradeDate, start, maturity, currency, contractCount, longPosition, referencePrice, contractValuePerPricePoint, convexityAdjustment, index, fixingIndexName, fixingHour, fixingMinute, forecastComponentKey, &trade);
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

struct XlRegister_RateFutureTrade_New_
{
    XlRegister_RateFutureTrade_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Trade identifier");
        argHelp.push_back("The trade date");
        argHelp.push_back("The start date");
        argHelp.push_back("The maturity date");
        argHelp.push_back("Trade currency code (the actual PV denomination)");
        argHelp.push_back("Positive contract count");
        argHelp.push_back("True when long the future");
        argHelp.push_back("Reference (trade) price");
        argHelp.push_back("Positive value per price point");
        argHelp.push_back("Convexity adjustment");
        argHelp.push_back("The rate index convention");
        argHelp.push_back("Fixing index name");
        argHelp.push_back("Fixing publication hour (0-23)");
        argHelp.push_back("Fixing publication minute (0-59)");
        argHelp.push_back("Market component key of the forecast curve");
        Excel::Register("Base", "xl_RateFutureTrade_New", "RATEFUTURETRADE.NEW", "Create a futures rate trade for node-sensitivity pricing", "QQQQQQQQQQQQQQQQ", "instrumentId,tradeDate,start,maturity,currency,contractCount,longPosition,referencePrice,contractValuePerPricePoint,convexityAdjustment,index,fixingIndexName,fixingHour,fixingMinute,forecastComponentKey", argHelp, false);
    }
};
static XlRegister_RateFutureTrade_New_ The_RateFutureTrade_New_XlRegisterer;

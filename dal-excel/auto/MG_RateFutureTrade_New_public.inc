

extern "C" __declspec(dllexport) OPER_* xl_RateFutureTrade_New
    (const OPER_* xl_header, const OPER_* xl_contractCount, const OPER_* xl_longPosition, const OPER_* xl_referencePrice, const OPER_* xl_contractValuePerPricePoint, const OPER_* xl_convexityAdjustment, const OPER_* xl_index, const OPER_* xl_fixingIdentity, const OPER_* xl_forecastComponentKey)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("RateFutureTrade_New");        
        argName = "header (input #1)";
        const Handle_<StorableRateTradeDefinition_> header = Excel::ToHandle<StorableRateTradeDefinition_>(_env, xl_header);
        argName = "contractCount (input #2)";
        const double contractCount = Excel::ToDouble(xl_contractCount);
        argName = "longPosition (input #3)";
        const bool longPosition = Excel::ToBool(xl_longPosition);
        argName = "referencePrice (input #4)";
        const double referencePrice = Excel::ToDouble(xl_referencePrice);
        argName = "contractValuePerPricePoint (input #5)";
        const double contractValuePerPricePoint = Excel::ToDouble(xl_contractValuePerPricePoint);
        argName = "convexityAdjustment (input #6)";
        const double convexityAdjustment = Excel::ToDouble(xl_convexityAdjustment);
        argName = "index (input #7)";
        const Handle_<StorableRateIndexConvention_> index = Excel::ToHandle<StorableRateIndexConvention_>(_env, xl_index);
        argName = "fixingIdentity (input #8)";
        const Handle_<StorableFixingIdentity_> fixingIdentity = Excel::ToHandle<StorableFixingIdentity_>(_env, xl_fixingIdentity);
        argName = "forecastComponentKey (input #9)";
        const String_ forecastComponentKey = Excel::ToString(xl_forecastComponentKey);
        argName = 0;
		Handle_<StorableRateTradeDefinition_> trade;
        RateFutureTrade_New(header, contractCount, longPosition, referencePrice, contractValuePerPricePoint, convexityAdjustment, index, fixingIdentity, forecastComponentKey, &trade);
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
        argHelp.push_back("The trade header (from RATETRADEHEADER.NEW)");
        argHelp.push_back("Positive contract count");
        argHelp.push_back("True when long the future");
        argHelp.push_back("Reference (trade) price");
        argHelp.push_back("Positive value per price point");
        argHelp.push_back("Convexity adjustment");
        argHelp.push_back("The rate index convention");
        argHelp.push_back("The fixing publication identity (from RATEFIXINGIDENTITY.NEW)");
        argHelp.push_back("Market component key of the forecast curve");
        Excel::Register("Base", "xl_RateFutureTrade_New", "RATEFUTURETRADE.NEW", "Create a futures rate trade for node-sensitivity pricing", "QQQQQQQQQQ", "header,contractCount,longPosition,referencePrice,contractValuePerPricePoint,convexityAdjustment,index,fixingIdentity,forecastComponentKey", argHelp, false);
    }
};
static XlRegister_RateFutureTrade_New_ The_RateFutureTrade_New_XlRegisterer;



extern "C" __declspec(dllexport) OPER_* xl_RateBasisTrade_New
    (const OPER_* xl_instrumentId, const OPER_* xl_tradeDate, const OPER_* xl_start, const OPER_* xl_maturity, const OPER_* xl_currency, const OPER_* xl_notional, const OPER_* xl_contractSpread, const OPER_* xl_receiveReference, const OPER_* xl_spreadLeg, const OPER_* xl_referenceLeg, const OPER_* xl_spreadIndex, const OPER_* xl_referenceIndex, const OPER_* xl_spreadFixingIndexName, const OPER_* xl_spreadFixingHour, const OPER_* xl_spreadFixingMinute, const OPER_* xl_referenceFixingIndexName, const OPER_* xl_referenceFixingHour, const OPER_* xl_referenceFixingMinute, const OPER_* xl_spreadForecastComponentKey, const OPER_* xl_referenceForecastComponentKey, const OPER_* xl_discountComponentKey)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("RateBasisTrade_New");        
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
        argName = "contractSpread (input #7)";
        const double contractSpread = Excel::ToDouble(xl_contractSpread);
        argName = "receiveReference (input #8)";
        const bool receiveReference = Excel::ToBool(xl_receiveReference);
        argName = "spreadLeg (input #9)";
        const Handle_<StorableRateLegConvention_> spreadLeg = Excel::ToHandle<StorableRateLegConvention_>(_env, xl_spreadLeg);
        argName = "referenceLeg (input #10)";
        const Handle_<StorableRateLegConvention_> referenceLeg = Excel::ToHandle<StorableRateLegConvention_>(_env, xl_referenceLeg);
        argName = "spreadIndex (input #11)";
        const Handle_<StorableRateIndexConvention_> spreadIndex = Excel::ToHandle<StorableRateIndexConvention_>(_env, xl_spreadIndex);
        argName = "referenceIndex (input #12)";
        const Handle_<StorableRateIndexConvention_> referenceIndex = Excel::ToHandle<StorableRateIndexConvention_>(_env, xl_referenceIndex);
        argName = "spreadFixingIndexName (input #13)";
        const String_ spreadFixingIndexName = Excel::ToString(xl_spreadFixingIndexName);
        argName = "spreadFixingHour (input #14)";
        const int spreadFixingHour = Excel::ToInt(xl_spreadFixingHour);
        argName = "spreadFixingMinute (input #15)";
        const int spreadFixingMinute = Excel::ToInt(xl_spreadFixingMinute);
        argName = "referenceFixingIndexName (input #16)";
        const String_ referenceFixingIndexName = Excel::ToString(xl_referenceFixingIndexName);
        argName = "referenceFixingHour (input #17)";
        const int referenceFixingHour = Excel::ToInt(xl_referenceFixingHour);
        argName = "referenceFixingMinute (input #18)";
        const int referenceFixingMinute = Excel::ToInt(xl_referenceFixingMinute);
        argName = "spreadForecastComponentKey (input #19)";
        const String_ spreadForecastComponentKey = Excel::ToString(xl_spreadForecastComponentKey);
        argName = "referenceForecastComponentKey (input #20)";
        const String_ referenceForecastComponentKey = Excel::ToString(xl_referenceForecastComponentKey);
        argName = "discountComponentKey (input #21)";
        const String_ discountComponentKey = Excel::ToString(xl_discountComponentKey);
        argName = 0;
		Handle_<StorableRateTradeDefinition_> trade;
        RateBasisTrade_New(instrumentId, tradeDate, start, maturity, currency, notional, contractSpread, receiveReference, spreadLeg, referenceLeg, spreadIndex, referenceIndex, spreadFixingIndexName, spreadFixingHour, spreadFixingMinute, referenceFixingIndexName, referenceFixingHour, referenceFixingMinute, spreadForecastComponentKey, referenceForecastComponentKey, discountComponentKey, &trade);
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

struct XlRegister_RateBasisTrade_New_
{
    XlRegister_RateBasisTrade_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Trade identifier");
        argHelp.push_back("The trade date");
        argHelp.push_back("The start date");
        argHelp.push_back("The maturity date");
        argHelp.push_back("Trade currency code (the actual PV denomination)");
        argHelp.push_back("Positive notional");
        argHelp.push_back("Contract spread on the spread leg");
        argHelp.push_back("True to receive the reference leg");
        argHelp.push_back("Spread leg convention");
        argHelp.push_back("Reference leg convention");
        argHelp.push_back("Spread leg index convention");
        argHelp.push_back("Reference leg index convention");
        argHelp.push_back("Spread leg fixing index name");
        argHelp.push_back("Spread leg fixing publication hour (0-23)");
        argHelp.push_back("Spread leg fixing publication minute (0-59)");
        argHelp.push_back("Reference leg fixing index name");
        argHelp.push_back("Reference leg fixing publication hour (0-23)");
        argHelp.push_back("Reference leg fixing publication minute (0-59)");
        argHelp.push_back("Market component key of the spread forecast curve");
        argHelp.push_back("Market component key of the reference forecast curve");
        argHelp.push_back("Market component key of the discount curve");
        Excel::Register("Base", "xl_RateBasisTrade_New", "RATEBASISTRADE.NEW", "Create a basis swap rate trade for node-sensitivity pricing", "QQQQQQQQQQQQQQQQQQQQQQ", "instrumentId,tradeDate,start,maturity,currency,notional,contractSpread,receiveReference,spreadLeg,referenceLeg,spreadIndex,referenceIndex,spreadFixingIndexName,spreadFixingHour,spreadFixingMinute,referenceFixingIndexName,referenceFixingHour,referenceFixingMinute,spreadForecastComponentKey,referenceForecastComponentKey,discountComponentKey", argHelp, false);
    }
};
static XlRegister_RateBasisTrade_New_ The_RateBasisTrade_New_XlRegisterer;

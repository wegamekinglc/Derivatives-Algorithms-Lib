

extern "C" __declspec(dllexport) OPER_* xl_RateFixedFloatTrade_New
    (const OPER_* xl_instrumentId, const OPER_* xl_family, const OPER_* xl_tradeDate, const OPER_* xl_start, const OPER_* xl_maturity, const OPER_* xl_currency, const OPER_* xl_notional, const OPER_* xl_contractRate, const OPER_* xl_payFixed, const OPER_* xl_fixedLeg, const OPER_* xl_floatLeg, const OPER_* xl_floatIndex, const OPER_* xl_fixingIndexName, const OPER_* xl_fixingHour, const OPER_* xl_fixingMinute, const OPER_* xl_forecastComponentKey, const OPER_* xl_discountComponentKey)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("RateFixedFloatTrade_New");        
        argName = "instrumentId (input #1)";
        const String_ instrumentId = Excel::ToString(xl_instrumentId);
        argName = "family (input #2)";
        const String_ family = Excel::ToString(xl_family);
        argName = "tradeDate (input #3)";
        const Date_ tradeDate = Excel::ToDate(xl_tradeDate);
        argName = "start (input #4)";
        const Date_ start = Excel::ToDate(xl_start);
        argName = "maturity (input #5)";
        const Date_ maturity = Excel::ToDate(xl_maturity);
        argName = "currency (input #6)";
        const String_ currency = Excel::ToString(xl_currency);
        argName = "notional (input #7)";
        const double notional = Excel::ToDouble(xl_notional);
        argName = "contractRate (input #8)";
        const double contractRate = Excel::ToDouble(xl_contractRate);
        argName = "payFixed (input #9)";
        const bool payFixed = Excel::ToBool(xl_payFixed);
        argName = "fixedLeg (input #10)";
        const Handle_<StorableRateLegConvention_> fixedLeg = Excel::ToHandle<StorableRateLegConvention_>(_env, xl_fixedLeg);
        argName = "floatLeg (input #11)";
        const Handle_<StorableRateLegConvention_> floatLeg = Excel::ToHandle<StorableRateLegConvention_>(_env, xl_floatLeg);
        argName = "floatIndex (input #12)";
        const Handle_<StorableRateIndexConvention_> floatIndex = Excel::ToHandle<StorableRateIndexConvention_>(_env, xl_floatIndex);
        argName = "fixingIndexName (input #13)";
        const String_ fixingIndexName = Excel::ToString(xl_fixingIndexName);
        argName = "fixingHour (input #14)";
        const int fixingHour = Excel::ToInt(xl_fixingHour);
        argName = "fixingMinute (input #15)";
        const int fixingMinute = Excel::ToInt(xl_fixingMinute);
        argName = "forecastComponentKey (input #16)";
        const String_ forecastComponentKey = Excel::ToString(xl_forecastComponentKey);
        argName = "discountComponentKey (input #17)";
        const String_ discountComponentKey = Excel::ToString(xl_discountComponentKey);
        argName = 0;
		Handle_<StorableRateTradeDefinition_> trade;
        RateFixedFloatTrade_New(instrumentId, family, tradeDate, start, maturity, currency, notional, contractRate, payFixed, fixedLeg, floatLeg, floatIndex, fixingIndexName, fixingHour, fixingMinute, forecastComponentKey, discountComponentKey, &trade);
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

struct XlRegister_RateFixedFloatTrade_New_
{
    XlRegister_RateFixedFloatTrade_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Trade identifier");
        argHelp.push_back("Family: OIS or IRS");
        argHelp.push_back("The trade date");
        argHelp.push_back("The start date");
        argHelp.push_back("The maturity date");
        argHelp.push_back("Trade currency code (the actual PV denomination)");
        argHelp.push_back("Positive notional");
        argHelp.push_back("Fixed contract rate");
        argHelp.push_back("True to pay the fixed leg");
        argHelp.push_back("Fixed leg convention");
        argHelp.push_back("Float leg convention");
        argHelp.push_back("Float index convention");
        argHelp.push_back("Fixing index name");
        argHelp.push_back("Fixing publication hour (0-23)");
        argHelp.push_back("Fixing publication minute (0-59)");
        argHelp.push_back("Market component key of the forecast curve");
        argHelp.push_back("Market component key of the discount curve");
        Excel::Register("Base", "xl_RateFixedFloatTrade_New", "RATEFIXEDFLOATTRADE.NEW", "Create an OIS or IRS rate trade for node-sensitivity pricing", "QQQQQQQQQQQQQQQQQQ", "instrumentId,family,tradeDate,start,maturity,currency,notional,contractRate,payFixed,fixedLeg,floatLeg,floatIndex,fixingIndexName,fixingHour,fixingMinute,forecastComponentKey,discountComponentKey", argHelp, false);
    }
};
static XlRegister_RateFixedFloatTrade_New_ The_RateFixedFloatTrade_New_XlRegisterer;

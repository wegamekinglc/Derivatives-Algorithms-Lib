

extern "C" __declspec(dllexport) OPER_* xl_RateFixedFloatTrade_New
    (const OPER_* xl_header, const OPER_* xl_family, const OPER_* xl_notional, const OPER_* xl_contractRate, const OPER_* xl_payFixed, const OPER_* xl_fixedLeg, const OPER_* xl_floatLeg, const OPER_* xl_floatIndex, const OPER_* xl_fixingIdentity, const OPER_* xl_forecastComponentKey, const OPER_* xl_discountComponentKey)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("RateFixedFloatTrade_New");        
        argName = "header (input #1)";
        const Handle_<StorableRateTradeDefinition_> header = Excel::ToHandle<StorableRateTradeDefinition_>(_env, xl_header);
        argName = "family (input #2)";
        const String_ family = Excel::ToString(xl_family);
        argName = "notional (input #3)";
        const double notional = Excel::ToDouble(xl_notional);
        argName = "contractRate (input #4)";
        const double contractRate = Excel::ToDouble(xl_contractRate);
        argName = "payFixed (input #5)";
        const bool payFixed = Excel::ToBool(xl_payFixed);
        argName = "fixedLeg (input #6)";
        const Handle_<StorableRateLegConvention_> fixedLeg = Excel::ToHandle<StorableRateLegConvention_>(_env, xl_fixedLeg);
        argName = "floatLeg (input #7)";
        const Handle_<StorableRateLegConvention_> floatLeg = Excel::ToHandle<StorableRateLegConvention_>(_env, xl_floatLeg);
        argName = "floatIndex (input #8)";
        const Handle_<StorableRateIndexConvention_> floatIndex = Excel::ToHandle<StorableRateIndexConvention_>(_env, xl_floatIndex);
        argName = "fixingIdentity (input #9)";
        const Handle_<StorableFixingIdentity_> fixingIdentity = Excel::ToHandle<StorableFixingIdentity_>(_env, xl_fixingIdentity);
        argName = "forecastComponentKey (input #10)";
        const String_ forecastComponentKey = Excel::ToString(xl_forecastComponentKey);
        argName = "discountComponentKey (input #11)";
        const String_ discountComponentKey = Excel::ToString(xl_discountComponentKey);
        argName = 0;
		Handle_<StorableRateTradeDefinition_> trade;
        RateFixedFloatTrade_New(header, family, notional, contractRate, payFixed, fixedLeg, floatLeg, floatIndex, fixingIdentity, forecastComponentKey, discountComponentKey, &trade);
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
        argHelp.push_back("The trade header (from RATETRADEHEADER.NEW)");
        argHelp.push_back("Family: OIS or IRS");
        argHelp.push_back("Positive notional");
        argHelp.push_back("Fixed contract rate");
        argHelp.push_back("True to pay the fixed leg");
        argHelp.push_back("Fixed leg convention");
        argHelp.push_back("Float leg convention");
        argHelp.push_back("Float index convention");
        argHelp.push_back("The fixing publication identity (from RATEFIXINGIDENTITY.NEW)");
        argHelp.push_back("Market component key of the forecast curve");
        argHelp.push_back("Market component key of the discount curve");
        Excel::Register("Base", "xl_RateFixedFloatTrade_New", "RATEFIXEDFLOATTRADE.NEW", "Create an OIS or IRS rate trade for node-sensitivity pricing", "QQQQQQQQQQQQ", "header,family,notional,contractRate,payFixed,fixedLeg,floatLeg,floatIndex,fixingIdentity,forecastComponentKey,discountComponentKey", argHelp, false);
    }
};
static XlRegister_RateFixedFloatTrade_New_ The_RateFixedFloatTrade_New_XlRegisterer;

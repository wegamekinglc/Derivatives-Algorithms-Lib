

extern "C" __declspec(dllexport) OPER_* xl_RateBasisTrade_New
    (const OPER_* xl_header, const OPER_* xl_notional, const OPER_* xl_contractSpread, const OPER_* xl_receiveReference, const OPER_* xl_spreadLeg, const OPER_* xl_referenceLeg, const OPER_* xl_spreadIndex, const OPER_* xl_referenceIndex, const OPER_* xl_spreadFixingIdentity, const OPER_* xl_referenceFixingIdentity, const OPER_* xl_spreadForecastComponentKey, const OPER_* xl_referenceForecastComponentKey, const OPER_* xl_discountComponentKey)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("RateBasisTrade_New");        
        argName = "header (input #1)";
        const Handle_<StorableRateTradeDefinition_> header = Excel::ToHandle<StorableRateTradeDefinition_>(_env, xl_header);
        argName = "notional (input #2)";
        const double notional = Excel::ToDouble(xl_notional);
        argName = "contractSpread (input #3)";
        const double contractSpread = Excel::ToDouble(xl_contractSpread);
        argName = "receiveReference (input #4)";
        const bool receiveReference = Excel::ToBool(xl_receiveReference);
        argName = "spreadLeg (input #5)";
        const Handle_<StorableRateLegConvention_> spreadLeg = Excel::ToHandle<StorableRateLegConvention_>(_env, xl_spreadLeg);
        argName = "referenceLeg (input #6)";
        const Handle_<StorableRateLegConvention_> referenceLeg = Excel::ToHandle<StorableRateLegConvention_>(_env, xl_referenceLeg);
        argName = "spreadIndex (input #7)";
        const Handle_<StorableRateIndexConvention_> spreadIndex = Excel::ToHandle<StorableRateIndexConvention_>(_env, xl_spreadIndex);
        argName = "referenceIndex (input #8)";
        const Handle_<StorableRateIndexConvention_> referenceIndex = Excel::ToHandle<StorableRateIndexConvention_>(_env, xl_referenceIndex);
        argName = "spreadFixingIdentity (input #9)";
        const Handle_<StorableFixingIdentity_> spreadFixingIdentity = Excel::ToHandle<StorableFixingIdentity_>(_env, xl_spreadFixingIdentity);
        argName = "referenceFixingIdentity (input #10)";
        const Handle_<StorableFixingIdentity_> referenceFixingIdentity = Excel::ToHandle<StorableFixingIdentity_>(_env, xl_referenceFixingIdentity);
        argName = "spreadForecastComponentKey (input #11)";
        const String_ spreadForecastComponentKey = Excel::ToString(xl_spreadForecastComponentKey);
        argName = "referenceForecastComponentKey (input #12)";
        const String_ referenceForecastComponentKey = Excel::ToString(xl_referenceForecastComponentKey);
        argName = "discountComponentKey (input #13)";
        const String_ discountComponentKey = Excel::ToString(xl_discountComponentKey);
        argName = 0;
		Handle_<StorableRateTradeDefinition_> trade;
        RateBasisTrade_New(header, notional, contractSpread, receiveReference, spreadLeg, referenceLeg, spreadIndex, referenceIndex, spreadFixingIdentity, referenceFixingIdentity, spreadForecastComponentKey, referenceForecastComponentKey, discountComponentKey, &trade);
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
        argHelp.push_back("The trade header (from RATETRADEHEADER.NEW)");
        argHelp.push_back("Positive notional");
        argHelp.push_back("Contract spread on the spread leg");
        argHelp.push_back("True to receive the reference leg");
        argHelp.push_back("Spread leg convention");
        argHelp.push_back("Reference leg convention");
        argHelp.push_back("Spread leg index convention");
        argHelp.push_back("Reference leg index convention");
        argHelp.push_back("Spread leg fixing identity");
        argHelp.push_back("Reference leg fixing identity");
        argHelp.push_back("Market component key of the spread forecast curve");
        argHelp.push_back("Market component key of the reference forecast curve");
        argHelp.push_back("Market component key of the discount curve");
        Excel::Register("Base", "xl_RateBasisTrade_New", "RATEBASISTRADE.NEW", "Create a basis swap rate trade for node-sensitivity pricing", "QQQQQQQQQQQQQQ", "header,notional,contractSpread,receiveReference,spreadLeg,referenceLeg,spreadIndex,referenceIndex,spreadFixingIdentity,referenceFixingIdentity,spreadForecastComponentKey,referenceForecastComponentKey,discountComponentKey", argHelp, false);
    }
};
static XlRegister_RateBasisTrade_New_ The_RateBasisTrade_New_XlRegisterer;

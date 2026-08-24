

extern "C" __declspec(dllexport) OPER_* xl_RateFraTrade_New
    (const OPER_* xl_header, const OPER_* xl_notional, const OPER_* xl_contractRate, const OPER_* xl_receiveFloating, const OPER_* xl_settleAtStart, const OPER_* xl_index, const OPER_* xl_fixingIdentity, const OPER_* xl_forecastComponentKey, const OPER_* xl_discountComponentKey)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("RateFraTrade_New");        
        argName = "header (input #1)";
        const Handle_<StorableRateTradeDefinition_> header = Excel::ToHandle<StorableRateTradeDefinition_>(_env, xl_header);
        argName = "notional (input #2)";
        const double notional = Excel::ToDouble(xl_notional);
        argName = "contractRate (input #3)";
        const double contractRate = Excel::ToDouble(xl_contractRate);
        argName = "receiveFloating (input #4)";
        const bool receiveFloating = Excel::ToBool(xl_receiveFloating);
        argName = "settleAtStart (input #5)";
        const bool settleAtStart = Excel::ToBool(xl_settleAtStart);
        argName = "index (input #6)";
        const Handle_<StorableRateIndexConvention_> index = Excel::ToHandle<StorableRateIndexConvention_>(_env, xl_index);
        argName = "fixingIdentity (input #7)";
        const Handle_<StorableFixingIdentity_> fixingIdentity = Excel::ToHandle<StorableFixingIdentity_>(_env, xl_fixingIdentity);
        argName = "forecastComponentKey (input #8)";
        const String_ forecastComponentKey = Excel::ToString(xl_forecastComponentKey);
        argName = "discountComponentKey (input #9)";
        const String_ discountComponentKey = Excel::ToString(xl_discountComponentKey);
        argName = 0;
		Handle_<StorableRateTradeDefinition_> trade;
        RateFraTrade_New(header, notional, contractRate, receiveFloating, settleAtStart, index, fixingIdentity, forecastComponentKey, discountComponentKey, &trade);
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
        argHelp.push_back("The trade header (from RATETRADEHEADER.NEW)");
        argHelp.push_back("Positive notional");
        argHelp.push_back("Contract rate");
        argHelp.push_back("True to receive the floating leg");
        argHelp.push_back("True to settle at the period start");
        argHelp.push_back("The rate index convention");
        argHelp.push_back("The fixing publication identity (from RATEFIXINGIDENTITY.NEW)");
        argHelp.push_back("Market component key of the forecast curve");
        argHelp.push_back("Market component key of the discount curve");
        Excel::Register("Base", "xl_RateFraTrade_New", "RATEFRATRADE.NEW", "Create a FRA rate trade for node-sensitivity pricing", "QQQQQQQQQQ", "header,notional,contractRate,receiveFloating,settleAtStart,index,fixingIdentity,forecastComponentKey,discountComponentKey", argHelp, false);
    }
};
static XlRegister_RateFraTrade_New_ The_RateFraTrade_New_XlRegisterer;

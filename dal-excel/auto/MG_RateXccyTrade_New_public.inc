

extern "C" __declspec(dllexport) OPER_* xl_RateXccyTrade_New
    (const OPER_* xl_instrumentId, const OPER_* xl_tradeDate, const OPER_* xl_start, const OPER_* xl_maturity, const OPER_* xl_positionCount, const OPER_* xl_contractSpread, const OPER_* xl_spreadOnForeignLeg, const OPER_* xl_receiveNonSpread, const OPER_* xl_config)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("RateXccyTrade_New");        
        argName = "instrumentId (input #1)";
        const String_ instrumentId = Excel::ToString(xl_instrumentId);
        argName = "tradeDate (input #2)";
        const Date_ tradeDate = Excel::ToDate(xl_tradeDate);
        argName = "start (input #3)";
        const Date_ start = Excel::ToDate(xl_start);
        argName = "maturity (input #4)";
        const Date_ maturity = Excel::ToDate(xl_maturity);
        argName = "positionCount (input #5)";
        const double positionCount = Excel::ToDouble(xl_positionCount);
        argName = "contractSpread (input #6)";
        const double contractSpread = Excel::ToDouble(xl_contractSpread);
        argName = "spreadOnForeignLeg (input #7)";
        const bool spreadOnForeignLeg = Excel::ToBool(xl_spreadOnForeignLeg);
        argName = "receiveNonSpread (input #8)";
        const bool receiveNonSpread = Excel::ToBool(xl_receiveNonSpread);
        argName = "config (input #9)";
        const Handle_<StorableCrossCurrencySwapConfig_> config = Excel::ToHandle<StorableCrossCurrencySwapConfig_>(_env, xl_config);
        argName = 0;
		Handle_<StorableRateTradeDefinition_> trade;
        RateXccyTrade_New(instrumentId, tradeDate, start, maturity, positionCount, contractSpread, spreadOnForeignLeg, receiveNonSpread, config, &trade);
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

struct XlRegister_RateXccyTrade_New_
{
    XlRegister_RateXccyTrade_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Trade identifier");
        argHelp.push_back("The trade date");
        argHelp.push_back("The start date");
        argHelp.push_back("The maturity date");
        argHelp.push_back("Positive position count");
        argHelp.push_back("Contract spread on the spread leg");
        argHelp.push_back("True when the spread rides the foreign leg (must match the config convention)");
        argHelp.push_back("True to receive the non-spread leg");
        argHelp.push_back("The cross-currency swap config (domestic currency of the pair is the actual PV denomination)");
        Excel::Register("Base", "xl_RateXccyTrade_New", "RATEXCCYTRADE.NEW", "Create a cross-currency swap rate trade for node-sensitivity pricing", "QQQQQQQQQQ", "instrumentId,tradeDate,start,maturity,positionCount,contractSpread,spreadOnForeignLeg,receiveNonSpread,config", argHelp, false);
    }
};
static XlRegister_RateXccyTrade_New_ The_RateXccyTrade_New_XlRegisterer;

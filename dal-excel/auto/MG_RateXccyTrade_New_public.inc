

extern "C" __declspec(dllexport) OPER_* xl_RateXccyTrade_New
    (const OPER_* xl_header, const OPER_* xl_positionCount, const OPER_* xl_contractSpread, const OPER_* xl_spreadOnForeignLeg, const OPER_* xl_receiveNonSpread, const OPER_* xl_config)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("RateXccyTrade_New");        
        argName = "header (input #1)";
        const Handle_<StorableRateTradeDefinition_> header = Excel::ToHandle<StorableRateTradeDefinition_>(_env, xl_header);
        argName = "positionCount (input #2)";
        const double positionCount = Excel::ToDouble(xl_positionCount);
        argName = "contractSpread (input #3)";
        const double contractSpread = Excel::ToDouble(xl_contractSpread);
        argName = "spreadOnForeignLeg (input #4)";
        const bool spreadOnForeignLeg = Excel::ToBool(xl_spreadOnForeignLeg);
        argName = "receiveNonSpread (input #5)";
        const bool receiveNonSpread = Excel::ToBool(xl_receiveNonSpread);
        argName = "config (input #6)";
        const Handle_<StorableCrossCurrencySwapConfig_> config = Excel::ToHandle<StorableCrossCurrencySwapConfig_>(_env, xl_config);
        argName = 0;
		Handle_<StorableRateTradeDefinition_> trade;
        RateXccyTrade_New(header, positionCount, contractSpread, spreadOnForeignLeg, receiveNonSpread, config, &trade);
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
        argHelp.push_back("The trade header (from RATETRADEHEADER.NEW); its currency must be the domestic currency");
        argHelp.push_back("Positive position count");
        argHelp.push_back("Contract spread on the spread leg");
        argHelp.push_back("True when the spread rides the foreign leg (must match the config convention)");
        argHelp.push_back("True to receive the non-spread leg");
        argHelp.push_back("The cross-currency swap config");
        Excel::Register("Base", "xl_RateXccyTrade_New", "RATEXCCYTRADE.NEW", "Create a cross-currency swap rate trade for node-sensitivity pricing", "QQQQQQQ", "header,positionCount,contractSpread,spreadOnForeignLeg,receiveNonSpread,config", argHelp, false);
    }
};
static XlRegister_RateXccyTrade_New_ The_RateXccyTrade_New_XlRegisterer;

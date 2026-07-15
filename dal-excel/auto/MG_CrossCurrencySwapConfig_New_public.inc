

extern "C" __declspec(dllexport) OPER_* xl_CrossCurrencySwapConfig_New
    (const OPER_* xl_currencies, const OPER_* xl_domesticLeg, const OPER_* xl_domesticIndex, const OPER_* xl_foreignLeg, const OPER_* xl_foreignIndex, const OPER_* xl_resetConvention, const OPER_* xl_notionalMode, const OPER_* xl_domesticRateIndex, const OPER_* xl_domesticRateFixingHour, const OPER_* xl_domesticRateFixingMinute, const OPER_* xl_foreignRateIndex, const OPER_* xl_foreignRateFixingHour, const OPER_* xl_foreignRateFixingMinute, const OPER_* xl_domesticNotional, const OPER_* xl_foreignNotional)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("CrossCurrencySwapConfig_New");        
        argName = "currencies (input #1)";
        const Handle_<StorableCurrencyPair_> currencies = Excel::ToHandle<StorableCurrencyPair_>(_env, xl_currencies);
        argName = "domesticLeg (input #2)";
        const Handle_<StorableRateLegConvention_> domesticLeg = Excel::ToHandle<StorableRateLegConvention_>(_env, xl_domesticLeg);
        argName = "domesticIndex (input #3)";
        const Handle_<StorableRateIndexConvention_> domesticIndex = Excel::ToHandle<StorableRateIndexConvention_>(_env, xl_domesticIndex);
        argName = "foreignLeg (input #4)";
        const Handle_<StorableRateLegConvention_> foreignLeg = Excel::ToHandle<StorableRateLegConvention_>(_env, xl_foreignLeg);
        argName = "foreignIndex (input #5)";
        const Handle_<StorableRateIndexConvention_> foreignIndex = Excel::ToHandle<StorableRateIndexConvention_>(_env, xl_foreignIndex);
        argName = "resetConvention (input #6)";
        const Handle_<StorableFxResetConvention_> resetConvention = Excel::ToHandle<StorableFxResetConvention_>(_env, xl_resetConvention);
        argName = "notionalMode (input #7)";
        const String_ notionalMode = Excel::ToString(xl_notionalMode);
        argName = "domesticRateIndex (input #8)";
        const String_ domesticRateIndex = Excel::ToString(xl_domesticRateIndex);
        argName = "domesticRateFixingHour (input #9)";
        const int domesticRateFixingHour = Excel::ToInt(xl_domesticRateFixingHour);
        argName = "domesticRateFixingMinute (input #10)";
        const int domesticRateFixingMinute = Excel::ToInt(xl_domesticRateFixingMinute);
        argName = "foreignRateIndex (input #11)";
        const String_ foreignRateIndex = Excel::ToString(xl_foreignRateIndex);
        argName = "foreignRateFixingHour (input #12)";
        const int foreignRateFixingHour = Excel::ToInt(xl_foreignRateFixingHour);
        argName = "foreignRateFixingMinute (input #13)";
        const int foreignRateFixingMinute = Excel::ToInt(xl_foreignRateFixingMinute);
        argName = "domesticNotional (input #14)";
        const double domesticNotional = Excel::ToDouble(xl_domesticNotional, true).value_or(100.0);
        argName = "foreignNotional (input #15)";
        const double foreignNotional = Excel::ToDouble(xl_foreignNotional, true).value_or(100.0);
        argName = 0;
		Handle_<StorableCrossCurrencySwapConfig_> config;
        CrossCurrencySwapConfig_New(currencies, domesticLeg, domesticIndex, foreignLeg, foreignIndex, resetConvention, notionalMode, domesticRateIndex, domesticRateFixingHour, domesticRateFixingMinute, foreignRateIndex, foreignRateFixingHour, foreignRateFixingMinute, domesticNotional, foreignNotional, &config);
        Excel::Retval_ retval;
        retval.Load(_env, config);
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

struct XlRegister_CrossCurrencySwapConfig_New_
{
    XlRegister_CrossCurrencySwapConfig_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("The currency pair (domestic, foreign)");
        argHelp.push_back("The domestic leg convention");
        argHelp.push_back("The domestic rate index convention");
        argHelp.push_back("The foreign leg convention");
        argHelp.push_back("The foreign rate index convention");
        argHelp.push_back("The FX reset convention (created with XCCYRESETCONVENTION.NEW)");
        argHelp.push_back("Notional behavior: FIXED, RESETTABLE, or MARK_TO_MARKET");
        argHelp.push_back("Domestic rate fixing index name");
        argHelp.push_back("Domestic rate fixing hour in the range 0 to 23");
        argHelp.push_back("Domestic rate fixing minute in the range 0 to 59");
        argHelp.push_back("Foreign rate fixing index name");
        argHelp.push_back("Foreign rate fixing hour in the range 0 to 23");
        argHelp.push_back("Foreign rate fixing minute in the range 0 to 59");
        argHelp.push_back("Domestic notional");
        argHelp.push_back("Foreign notional");
        Excel::Register("Base", "xl_CrossCurrencySwapConfig_New", "CROSSCURRENCYSWAPCONFIG.NEW", "Create a configured cross-currency swap input handle", "QQQQQQQQQQQQQQQQ", "currencies,domesticLeg,domesticIndex,foreignLeg,foreignIndex,resetConvention,notionalMode,domesticRateIndex,domesticRateFixingHour,domesticRateFixingMinute,foreignRateIndex,foreignRateFixingHour,foreignRateFixingMinute,[domesticNotional],[foreignNotional]", argHelp, false);
    }
};
static XlRegister_CrossCurrencySwapConfig_New_ The_CrossCurrencySwapConfig_New_XlRegisterer;



extern "C" __declspec(dllexport) OPER_* xl_CrossCurrencySwap_Config_New
    (const OPER_* xl_tradeDate, const OPER_* xl_start, const OPER_* xl_maturity, const OPER_* xl_marketRate, const OPER_* xl_config)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("CrossCurrencySwap_Config_New");        
        argName = "tradeDate (input #1)";
        const Date_ tradeDate = Excel::ToDate(xl_tradeDate);
        argName = "start (input #2)";
        const Date_ start = Excel::ToDate(xl_start);
        argName = "maturity (input #3)";
        const Date_ maturity = Excel::ToDate(xl_maturity);
        argName = "marketRate (input #4)";
        const double marketRate = Excel::ToDouble(xl_marketRate);
        argName = "config (input #5)";
        const Handle_<StorableCrossCurrencySwapConfig_> config = Excel::ToHandle<StorableCrossCurrencySwapConfig_>(_env, xl_config);
        argName = 0;
		Handle_<StorableCrossCurrencySwap_> instrument;
        CrossCurrencySwap_Config_New(tradeDate, start, maturity, marketRate, config, &instrument);
        Excel::Retval_ retval;
        retval.Load(_env, instrument);
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

struct XlRegister_CrossCurrencySwap_Config_New_
{
    XlRegister_CrossCurrencySwap_Config_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("The trade or settlement date");
        argHelp.push_back("The swap start date");
        argHelp.push_back("The swap maturity date");
        argHelp.push_back("The market quoted spread");
        argHelp.push_back("The cross-currency swap configuration");
        Excel::Register("Base", "xl_CrossCurrencySwap_Config_New", "CROSSCURRENCYSWAP.CONFIG.NEW", "Create a cross-currency swap instrument from a configuration handle", "QQQQQQ", "tradeDate,start,maturity,marketRate,config", argHelp, false);
    }
};
static XlRegister_CrossCurrencySwap_Config_New_ The_CrossCurrencySwap_Config_New_XlRegisterer;

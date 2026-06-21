

extern "C" __declspec(dllexport) OPER_* xl_CrossCurrencySwap_New
    (const OPER_* xl_tradeDate, const OPER_* xl_start, const OPER_* xl_maturity, const OPER_* xl_marketRate, const OPER_* xl_currencies, const OPER_* xl_domesticLeg, const OPER_* xl_domesticIndex, const OPER_* xl_foreignLeg, const OPER_* xl_foreignIndex, const OPER_* xl_domesticNotional, const OPER_* xl_foreignNotional)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("CrossCurrencySwap_New");        
        argName = "tradeDate (input #1)";
        const Date_ tradeDate = Excel::ToDate(xl_tradeDate);
        argName = "start (input #2)";
        const Date_ start = Excel::ToDate(xl_start);
        argName = "maturity (input #3)";
        const Date_ maturity = Excel::ToDate(xl_maturity);
        argName = "marketRate (input #4)";
        const double marketRate = Excel::ToDouble(xl_marketRate);
        argName = "currencies (input #5)";
        const Handle_<StorableCurrencyPair_> currencies = Excel::ToHandle<StorableCurrencyPair_>(_env, xl_currencies);
        argName = "domesticLeg (input #6)";
        const Handle_<StorableRateLegConvention_> domesticLeg = Excel::ToHandle<StorableRateLegConvention_>(_env, xl_domesticLeg);
        argName = "domesticIndex (input #7)";
        const Handle_<StorableRateIndexConvention_> domesticIndex = Excel::ToHandle<StorableRateIndexConvention_>(_env, xl_domesticIndex);
        argName = "foreignLeg (input #8)";
        const Handle_<StorableRateLegConvention_> foreignLeg = Excel::ToHandle<StorableRateLegConvention_>(_env, xl_foreignLeg);
        argName = "foreignIndex (input #9)";
        const Handle_<StorableRateIndexConvention_> foreignIndex = Excel::ToHandle<StorableRateIndexConvention_>(_env, xl_foreignIndex);
        argName = "domesticNotional (input #10)";
        const double domesticNotional = Excel::ToDouble(xl_domesticNotional, true).value_or(100.0);
        argName = "foreignNotional (input #11)";
        const double foreignNotional = Excel::ToDouble(xl_foreignNotional, true).value_or(100.0);
        argName = 0;
		Handle_<StorableCrossCurrencySwap_> instrument;
        CrossCurrencySwap_New(tradeDate, start, maturity, marketRate, currencies, domesticLeg, domesticIndex, foreignLeg, foreignIndex, domesticNotional, foreignNotional, &instrument);
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

struct XlRegister_CrossCurrencySwap_New_
{
    XlRegister_CrossCurrencySwap_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("The trade/settlement date");
        argHelp.push_back("The start date of the swap");
        argHelp.push_back("The maturity date of the swap");
        argHelp.push_back("The market quoted spread");
        argHelp.push_back("The currency pair (domestic, foreign)");
        argHelp.push_back("The domestic leg convention");
        argHelp.push_back("The domestic index convention");
        argHelp.push_back("The foreign leg convention");
        argHelp.push_back("The foreign index convention");
        argHelp.push_back("Domestic notional");
        argHelp.push_back("Foreign notional");
        Excel::Register("Base", "xl_CrossCurrencySwap_New", "DA.CROSSCURRENCYSWAPNEW", "Create a cross-currency swap instrument for xccy calibration", "QQQQQQQQQQQQ", "tradeDate,start,maturity,marketRate,currencies,domesticLeg,domesticIndex,foreignLeg,foreignIndex,[domesticNotional],[foreignNotional]", argHelp, false);
    }
};
static XlRegister_CrossCurrencySwap_New_ The_CrossCurrencySwap_New_XlRegisterer;

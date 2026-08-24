

extern "C" __declspec(dllexport) OPER_* xl_RateDepositTrade_New
    (const OPER_* xl_header, const OPER_* xl_notional, const OPER_* xl_contractRate, const OPER_* xl_lend, const OPER_* xl_index, const OPER_* xl_discountComponentKey)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("RateDepositTrade_New");        
        argName = "header (input #1)";
        const Handle_<StorableRateTradeDefinition_> header = Excel::ToHandle<StorableRateTradeDefinition_>(_env, xl_header);
        argName = "notional (input #2)";
        const double notional = Excel::ToDouble(xl_notional);
        argName = "contractRate (input #3)";
        const double contractRate = Excel::ToDouble(xl_contractRate);
        argName = "lend (input #4)";
        const bool lend = Excel::ToBool(xl_lend);
        argName = "index (input #5)";
        const Handle_<StorableRateIndexConvention_> index = Excel::ToHandle<StorableRateIndexConvention_>(_env, xl_index);
        argName = "discountComponentKey (input #6)";
        const String_ discountComponentKey = Excel::ToString(xl_discountComponentKey);
        argName = 0;
		Handle_<StorableRateTradeDefinition_> trade;
        RateDepositTrade_New(header, notional, contractRate, lend, index, discountComponentKey, &trade);
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

struct XlRegister_RateDepositTrade_New_
{
    XlRegister_RateDepositTrade_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("The trade header (from RATETRADEHEADER.NEW)");
        argHelp.push_back("Positive notional");
        argHelp.push_back("Contract rate");
        argHelp.push_back("True to lend (receive the maturity payment)");
        argHelp.push_back("The rate index convention");
        argHelp.push_back("Market component key of the discount curve");
        Excel::Register("Base", "xl_RateDepositTrade_New", "RATEDEPOSITTRADE.NEW", "Create a deposit rate trade for node-sensitivity pricing", "QQQQQQQ", "header,notional,contractRate,lend,index,discountComponentKey", argHelp, false);
    }
};
static XlRegister_RateDepositTrade_New_ The_RateDepositTrade_New_XlRegisterer;



extern "C" __declspec(dllexport) OPER_* xl_RatePortfolioNodeRisk_Spill
    (const OPER_* xl_trades, const OPER_* xl_componentKeys, const OPER_* xl_market)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("RatePortfolioNodeRisk_Spill");        
        argName = "trades (input #1)";
        const Vector_<Handle_<Storable_>> trades = Excel::ToHandleBaseVector(_env, xl_trades);
        argName = "componentKeys (input #2)";
        const Vector_<String_> componentKeys = Excel::ToStringVector(xl_componentKeys);
        argName = "market (input #3)";
        const Handle_<StorableRatePricingMarket_> market = Excel::ToHandle<StorableRatePricingMarket_>(_env, xl_market);
        argName = 0;
		Matrix_<Cell_> spill;
        RatePortfolioNodeRisk_Spill(trades, componentKeys, market, &spill);
        Excel::Retval_ retval;
        retval.Load(spill);
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

struct XlRegister_RatePortfolioNodeRisk_Spill_
{
    XlRegister_RatePortfolioNodeRisk_Spill_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Rate trade definition handles");
        argHelp.push_back("One shared component key list applied to every trade");
        argHelp.push_back("The rate pricing market");
        Excel::Register("Base", "xl_RatePortfolioNodeRisk_Spill", "RATEPORTFOLIONODERISK.SPILL", "Portfolio node-risk aggregation as a long-form spill", "QQQQ", "trades,componentKeys,market", argHelp, false);
    }
};
static XlRegister_RatePortfolioNodeRisk_Spill_ The_RatePortfolioNodeRisk_Spill_XlRegisterer;

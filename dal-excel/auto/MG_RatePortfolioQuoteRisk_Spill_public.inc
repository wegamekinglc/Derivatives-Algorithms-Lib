

extern "C" __declspec(dllexport) OPER_* xl_RatePortfolioQuoteRisk_Spill
    (const OPER_* xl_trades, const OPER_* xl_market, const OPER_* xl_provenances)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("RatePortfolioQuoteRisk_Spill");        
        argName = "trades (input #1)";
        const Vector_<Handle_<Storable_>> trades = Excel::ToHandleBaseVector(_env, xl_trades);
        argName = "market (input #2)";
        const Handle_<StorableRatePricingMarket_> market = Excel::ToHandle<StorableRatePricingMarket_>(_env, xl_market);
        argName = "provenances (input #3)";
        const Vector_<Handle_<Storable_>> provenances = Excel::ToHandleBaseVector(_env, xl_provenances);
        argName = 0;
		Matrix_<Cell_> spill;
        RatePortfolioQuoteRisk_Spill(trades, market, provenances, &spill);
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

struct XlRegister_RatePortfolioQuoteRisk_Spill_
{
    XlRegister_RatePortfolioQuoteRisk_Spill_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Rate trade definition handles");
        argHelp.push_back("The rate pricing market used by the immutable provenance handles");
        argHelp.push_back("Quote-risk provenance handles with unique non-empty calibration identifiers");
        Excel::Register("Base", "xl_RatePortfolioQuoteRisk_Spill", "RATEPORTFOLIOQUOTERISK.SPILL", "Aggregate portfolio quote sensitivity and DV01 into a fixed long-form spill", "QQQQ", "trades,market,provenances", argHelp, false);
    }
};
static XlRegister_RatePortfolioQuoteRisk_Spill_ The_RatePortfolioQuoteRisk_Spill_XlRegisterer;

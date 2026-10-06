

extern "C" __declspec(dllexport) OPER_* xl_PortfolioRiskResult_Get_TradeProvenance
    (const OPER_* xl_result, const OPER_* xl_trade)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("PortfolioRiskResult_Get_TradeProvenance");        
        argName = "result (input #1)";
        const Handle_<Storable_> result = Excel::ToHandle<Storable_>(_env, xl_result);
        argName = "trade"; Excel::ValidatePortfolioTradeOrdinal(xl_trade, "PortfolioRiskResult_Get_TradeProvenance");
        argName = "trade (input #2)";
        const double trade = Excel::ToDouble(xl_trade);
        argName = 0;
		Matrix_<Cell_> provenance;
        PortfolioRiskResult_Get_TradeProvenance(result, trade, &provenance);
        Excel::Retval_ retval;
        retval.Load(provenance);
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

struct XlRegister_PortfolioRiskResult_Get_TradeProvenance_
{
    XlRegister_PortfolioRiskResult_Get_TradeProvenance_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Completed weighted or attribution portfolio result");
        argHelp.push_back("Strict zero-based original trade ordinal");
        Excel::Register("Base", "xl_PortfolioRiskResult_Get_TradeProvenance", "PORTFOLIORISKRESULT.GET.TRADEPROVENANCE", "Extract one original trade's frozen valuation provenance", "QQQ", "result,trade", argHelp, false);
    }
};
static XlRegister_PortfolioRiskResult_Get_TradeProvenance_ The_PortfolioRiskResult_Get_TradeProvenance_XlRegisterer;

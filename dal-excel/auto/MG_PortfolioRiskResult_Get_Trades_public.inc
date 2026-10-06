

extern "C" __declspec(dllexport) OPER_* xl_PortfolioRiskResult_Get_Trades
    (const OPER_* xl_result)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("PortfolioRiskResult_Get_Trades");        
        argName = "result (input #1)";
        const Handle_<Storable_> result = Excel::ToHandle<Storable_>(_env, xl_result);
        argName = 0;
		Matrix_<Cell_> trades;
        PortfolioRiskResult_Get_Trades(result, &trades);
        Excel::Retval_ retval;
        retval.Load(trades);
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

struct XlRegister_PortfolioRiskResult_Get_Trades_
{
    XlRegister_PortfolioRiskResult_Get_Trades_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Completed weighted or attribution portfolio result");
        Excel::Register("Base", "xl_PortfolioRiskResult_Get_Trades", "PORTFOLIORISKRESULT.GET.TRADES", "Extract original sealed trade and model-owner mapping", "QQ", "result", argHelp, false);
    }
};
static XlRegister_PortfolioRiskResult_Get_Trades_ The_PortfolioRiskResult_Get_Trades_XlRegisterer;

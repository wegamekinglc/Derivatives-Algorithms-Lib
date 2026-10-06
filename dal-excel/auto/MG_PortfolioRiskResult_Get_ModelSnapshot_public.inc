

extern "C" __declspec(dllexport) OPER_* xl_PortfolioRiskResult_Get_ModelSnapshot
    (const OPER_* xl_result, const OPER_* xl_trade)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("PortfolioRiskResult_Get_ModelSnapshot");        
        argName = "result (input #1)";
        const Handle_<Storable_> result = Excel::ToHandle<Storable_>(_env, xl_result);
        argName = "trade"; Excel::ValidatePortfolioTradeOrdinal(xl_trade, "PortfolioRiskResult_Get_ModelSnapshot");
        argName = "trade (input #2)";
        const double trade = Excel::ToDouble(xl_trade);
        argName = 0;
		Vector_<String_> json;
        PortfolioRiskResult_Get_ModelSnapshot(result, trade, &json);
        Excel::Retval_ retval;
        retval.Load(json);
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

struct XlRegister_PortfolioRiskResult_Get_ModelSnapshot_
{
    XlRegister_PortfolioRiskResult_Get_ModelSnapshot_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Completed weighted or attribution portfolio result");
        argHelp.push_back("Strict zero-based original trade ordinal");
        Excel::Register("Base", "xl_PortfolioRiskResult_Get_ModelSnapshot", "PORTFOLIORISKRESULT.GET.MODELSNAPSHOT", "Extract one original trade's passive model JSON chunks", "QQQ", "result,trade", argHelp, false);
    }
};
static XlRegister_PortfolioRiskResult_Get_ModelSnapshot_ The_PortfolioRiskResult_Get_ModelSnapshot_XlRegisterer;

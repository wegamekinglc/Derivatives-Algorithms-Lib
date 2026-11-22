

extern "C" __declspec(dllexport) OPER_* xl_MarketIvScriptRefitRiskResult_Get_Rows
    (const OPER_* xl_result)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("MarketIvScriptRefitRiskResult_Get_Rows");        
        argName = "result (input #1)";
        const Handle_<StorableMarketIvScriptRefitRiskResult_> result = Excel::ToHandle<StorableMarketIvScriptRefitRiskResult_>(_env, xl_result);
        argName = 0;
		Matrix_<Cell_> rows;
        MarketIvScriptRefitRiskResult_Get_Rows(result, &rows);
        Excel::Retval_ retval;
        retval.Load(rows);
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

struct XlRegister_MarketIvScriptRefitRiskResult_Get_Rows_
{
    XlRegister_MarketIvScriptRefitRiskResult_Get_Rows_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Script quote-refit result");
        Excel::Register("Base", "xl_MarketIvScriptRefitRiskResult_Get_Rows", "MARKETIVSCRIPTREFITRISKRESULT.GET.ROWS", "Quote PVs, paired slope and standard error, fit branches and failures", "QQ", "result", argHelp, false);
    }
};
static XlRegister_MarketIvScriptRefitRiskResult_Get_Rows_ The_MarketIvScriptRefitRiskResult_Get_Rows_XlRegisterer;

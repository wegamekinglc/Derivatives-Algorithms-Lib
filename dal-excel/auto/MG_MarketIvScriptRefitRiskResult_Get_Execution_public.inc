

extern "C" __declspec(dllexport) OPER_* xl_MarketIvScriptRefitRiskResult_Get_Execution
    (const OPER_* xl_result)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("MarketIvScriptRefitRiskResult_Get_Execution");        
        argName = "result (input #1)";
        const Handle_<StorableMarketIvScriptRefitRiskResult_> result = Excel::ToHandle<StorableMarketIvScriptRefitRiskResult_>(_env, xl_result);
        argName = 0;
		Matrix_<Cell_> table;
        MarketIvScriptRefitRiskResult_Get_Execution(result, &table);
        Excel::Retval_ retval;
        retval.Load(table);
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

struct XlRegister_MarketIvScriptRefitRiskResult_Get_Execution_
{
    XlRegister_MarketIvScriptRefitRiskResult_Get_Execution_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Script quote-refit result");
        Excel::Register("Base", "xl_MarketIvScriptRefitRiskResult_Get_Execution", "MARKETIVSCRIPTREFITRISKRESULT.GET.EXECUTION", "Return refit, grid, sampling and workspace counts", "QQ", "result", argHelp, false);
    }
};
static XlRegister_MarketIvScriptRefitRiskResult_Get_Execution_ The_MarketIvScriptRefitRiskResult_Get_Execution_XlRegisterer;

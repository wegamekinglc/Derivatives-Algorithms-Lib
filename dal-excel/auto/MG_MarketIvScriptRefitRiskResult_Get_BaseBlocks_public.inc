

extern "C" __declspec(dllexport) OPER_* xl_MarketIvScriptRefitRiskResult_Get_BaseBlocks
    (const OPER_* xl_result)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("MarketIvScriptRefitRiskResult_Get_BaseBlocks");        
        argName = "result (input #1)";
        const Handle_<StorableMarketIvScriptRefitRiskResult_> result = Excel::ToHandle<StorableMarketIvScriptRefitRiskResult_>(_env, xl_result);
        argName = 0;
		Vector_<double> means;
        MarketIvScriptRefitRiskResult_Get_BaseBlocks(result, &means);
        Excel::Retval_ retval;
        retval.Load(means);
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

struct XlRegister_MarketIvScriptRefitRiskResult_Get_BaseBlocks_
{
    XlRegister_MarketIvScriptRefitRiskResult_Get_BaseBlocks_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Script quote-refit result");
        Excel::Register("Base", "xl_MarketIvScriptRefitRiskResult_Get_BaseBlocks", "MARKETIVSCRIPTREFITRISKRESULT.GET.BASEBLOCKS", "Base mean PV of each sampling block in path order", "QQ", "result", argHelp, false);
    }
};
static XlRegister_MarketIvScriptRefitRiskResult_Get_BaseBlocks_ The_MarketIvScriptRefitRiskResult_Get_BaseBlocks_XlRegisterer;

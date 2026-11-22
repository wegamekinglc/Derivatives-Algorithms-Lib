

extern "C" __declspec(dllexport) OPER_* xl_MarketIvScriptRefitRiskResult_New
    (const OPER_* xl_name, const OPER_* xl_request)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("MarketIvScriptRefitRiskResult_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "name"; Excel::ValidateRiskRequestText(xl_name, "InvalidMarketIvLocalVolRequest: /name");
        argName = "request (input #2)";
        const Handle_<StorableMarketIvScriptRefitRequest_> request = Excel::ToHandle<StorableMarketIvScriptRefitRequest_>(_env, xl_request);
        argName = 0;
		Handle_<StorableMarketIvScriptRefitRiskResult_> result;
        MarketIvScriptRefitRiskResult_New(name, request, &result);
        Excel::Retval_ retval;
        retval.Load(_env, result);
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

struct XlRegister_MarketIvScriptRefitRiskResult_New_
{
    XlRegister_MarketIvScriptRefitRiskResult_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Result name");
        argHelp.push_back("Typed quote-refit request");
        Excel::Register("Base", "xl_MarketIvScriptRefitRiskResult_New", "MARKETIVSCRIPTREFITRISKRESULT.NEW", "Evaluate script PV on the base and independently refitted sides", "QQQ", "name,request", argHelp, false);
    }
};
static XlRegister_MarketIvScriptRefitRiskResult_New_ The_MarketIvScriptRefitRiskResult_New_XlRegisterer;

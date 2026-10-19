

extern "C" __declspec(dllexport) OPER_* xl_MarketIvRefitRiskResult_New
    (const OPER_* xl_name, const OPER_* xl_snapshot, const OPER_* xl_request)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("MarketIvRefitRiskResult_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "snapshot (input #2)";
        const Handle_<StorableMarketIvCalibrationSnapshot_> snapshot = Excel::ToHandle<StorableMarketIvCalibrationSnapshot_>(_env, xl_snapshot);
        argName = "request (input #3)";
        const Handle_<StorableMarketIvRefitRiskRequest_> request = Excel::ToHandle<StorableMarketIvRefitRiskRequest_>(_env, xl_request);
        argName = 0;
		Handle_<StorableMarketIvRefitRiskResult_> result;
        MarketIvRefitRiskResult_New(name, snapshot, request, &result);
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

struct XlRegister_MarketIvRefitRiskResult_New_
{
    XlRegister_MarketIvRefitRiskResult_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("name");
        argHelp.push_back("snapshot");
        argHelp.push_back("request");
        Excel::Register("Base", "xl_MarketIvRefitRiskResult_New", "MARKETIVREFITRISKRESULT.NEW", "Market IV surface owning value", "QQQQ", "name,snapshot,request", argHelp, false);
    }
};
static XlRegister_MarketIvRefitRiskResult_New_ The_MarketIvRefitRiskResult_New_XlRegisterer;

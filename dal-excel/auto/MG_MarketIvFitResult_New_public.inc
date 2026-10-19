

extern "C" __declspec(dllexport) OPER_* xl_MarketIvFitResult_New
    (const OPER_* xl_name, const OPER_* xl_request)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("MarketIvFitResult_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "request (input #2)";
        const Handle_<StorableMarketIvFitRequest_> request = Excel::ToHandle<StorableMarketIvFitRequest_>(_env, xl_request);
        argName = 0;
		Handle_<StorableMarketIvFitResult_> result;
        MarketIvFitResult_New(name, request, &result);
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

struct XlRegister_MarketIvFitResult_New_
{
    XlRegister_MarketIvFitResult_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("name");
        argHelp.push_back("request");
        Excel::Register("Base", "xl_MarketIvFitResult_New", "MARKETIVFITRESULT.NEW", "Market IV surface owning value", "QQQ", "name,request", argHelp, false);
    }
};
static XlRegister_MarketIvFitResult_New_ The_MarketIvFitResult_New_XlRegisterer;

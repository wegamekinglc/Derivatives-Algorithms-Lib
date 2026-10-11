

extern "C" __declspec(dllexport) OPER_* xl_EuropeanPdeRiskResult_Get_Request
    (const OPER_* xl_result)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("EuropeanPdeRiskResult_Get_Request");        
        argName = "result (input #1)";
        const Handle_<StorableEuropeanPdeRiskResult_> result = Excel::ToHandle<StorableEuropeanPdeRiskResult_>(_env, xl_result);
        argName = 0;
		Handle_<StorableEuropeanPdeRiskRequest_> request;
        EuropeanPdeRiskResult_Get_Request(result, &request);
        Excel::Retval_ retval;
        retval.Load(_env, request);
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

struct XlRegister_EuropeanPdeRiskResult_Get_Request_
{
    XlRegister_EuropeanPdeRiskResult_Get_Request_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Completed result");
        Excel::Register("Base", "xl_EuropeanPdeRiskResult_Get_Request", "EUROPEANPDERISKRESULT.GET.REQUEST", "Copy the actually resolved European PDE request", "QQ", "result", argHelp, false);
    }
};
static XlRegister_EuropeanPdeRiskResult_Get_Request_ The_EuropeanPdeRiskResult_Get_Request_XlRegisterer;

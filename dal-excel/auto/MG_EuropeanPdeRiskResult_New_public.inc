

extern "C" __declspec(dllexport) OPER_* xl_EuropeanPdeRiskResult_New
    (const OPER_* xl_name, const OPER_* xl_request)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("EuropeanPdeRiskResult_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "name"; Excel::ValidateRiskRequestText(xl_name, "EuropeanPdeRiskResult_New; name");
        argName = "request (input #2)";
        const Handle_<StorableEuropeanPdeRiskRequest_> request = Excel::ToHandle<StorableEuropeanPdeRiskRequest_>(_env, xl_request);
        argName = 0;
		Handle_<StorableEuropeanPdeRiskResult_> result;
        EuropeanPdeRiskResult_New(name, request, &result);
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

struct XlRegister_EuropeanPdeRiskResult_New_
{
    XlRegister_EuropeanPdeRiskResult_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Object name");
        argHelp.push_back("Checked passive request");
        Excel::Register("Base", "xl_EuropeanPdeRiskResult_New", "EUROPEANPDERISKRESULT.NEW", "Execute native AAD European call and put risk on an empty caller tape", "QQQ", "name,request", argHelp, false);
    }
};
static XlRegister_EuropeanPdeRiskResult_New_ The_EuropeanPdeRiskResult_New_XlRegisterer;

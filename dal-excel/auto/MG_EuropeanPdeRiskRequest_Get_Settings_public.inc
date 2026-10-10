

extern "C" __declspec(dllexport) OPER_* xl_EuropeanPdeRiskRequest_Get_Settings
    (const OPER_* xl_request)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("EuropeanPdeRiskRequest_Get_Settings");        
        argName = "request (input #1)";
        const Handle_<StorableEuropeanPdeRiskRequest_> request = Excel::ToHandle<StorableEuropeanPdeRiskRequest_>(_env, xl_request);
        argName = 0;
		Handle_<StorableEuropeanPdeRiskSettings_> settings;
        EuropeanPdeRiskRequest_Get_Settings(request, &settings);
        Excel::Retval_ retval;
        retval.Load(_env, settings);
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

struct XlRegister_EuropeanPdeRiskRequest_Get_Settings_
{
    XlRegister_EuropeanPdeRiskRequest_Get_Settings_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Passive request");
        Excel::Register("Base", "xl_EuropeanPdeRiskRequest_Get_Settings", "EUROPEANPDERISKREQUEST.GET.SETTINGS", "Copy settings retained by a European PDE request", "QQ", "request", argHelp, false);
    }
};
static XlRegister_EuropeanPdeRiskRequest_Get_Settings_ The_EuropeanPdeRiskRequest_Get_Settings_XlRegisterer;

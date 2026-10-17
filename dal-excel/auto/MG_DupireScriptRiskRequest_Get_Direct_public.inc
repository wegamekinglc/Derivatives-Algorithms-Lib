

extern "C" __declspec(dllexport) OPER_* xl_DupireScriptRiskRequest_Get_Direct
    (const OPER_* xl_request)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("DupireScriptRiskRequest_Get_Direct");        
        argName = "request (input #1)";
        const Handle_<StorableDupireScriptRiskRequest_> request = Excel::ToHandle<StorableDupireScriptRiskRequest_>(_env, xl_request);
        argName = 0;
		Handle_<StorableCalibrationDirectQuoteAdjoints_> direct;
        DupireScriptRiskRequest_Get_Direct(request, &direct);
        Excel::Retval_ retval;
        retval.Load(_env, direct);
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

struct XlRegister_DupireScriptRiskRequest_Get_Direct_
{
    XlRegister_DupireScriptRiskRequest_Get_Direct_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Automatic request with external direct seeds");
        Excel::Register("Base", "xl_DupireScriptRiskRequest_Get_Direct", "DUPIRESCRIPTRISKREQUEST.GET.DIRECT", "Copy external direct seeds or reject if absent", "QQ", "request", argHelp, false);
    }
};
static XlRegister_DupireScriptRiskRequest_Get_Direct_ The_DupireScriptRiskRequest_Get_Direct_XlRegisterer;
